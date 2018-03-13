/*
 * Copyright (c) 2016, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <assert.h>

#include "av1/common/cfl.h"
#include "av1/common/common.h"
#include "av1/common/entropy.h"
#include "av1/common/entropymode.h"
#include "av1/common/entropymv.h"
#include "av1/common/mvref_common.h"
#include "av1/common/pred_common.h"
#include "av1/common/reconinter.h"
#include "av1/common/reconintra.h"
#include "av1/common/seg_common.h"
#include "av1/common/warped_motion.h"

#include "av1/decoder/decodeframe.h"
#include "av1/decoder/decodemv.h"

#include "aom_dsp/aom_dsp_common.h"

#define ACCT_STR __func__

#define DEC_MISMATCH_DEBUG 0

static PREDICTION_MODE read_intra_mode(aom_reader *r, aom_cdf_prob *cdf) {
  return (PREDICTION_MODE)aom_read_symbol(r, cdf, INTRA_MODES, ACCT_STR);
}

static void read_cdef(AV1_COMMON *cm, aom_reader *r, MB_MODE_INFO *const mbmi,
                      int mi_col, int mi_row) {
  if (cm->all_lossless) return;
  if (cm->allow_intrabc && NO_FILTER_FOR_IBC) {
    assert(cm->cdef_bits == 0);
    return;
  }

  if (!(mi_col & (cm->seq_params.mib_size - 1)) &&
      !(mi_row & (cm->seq_params.mib_size - 1))) {  // Top left?
    cm->cdef_preset[0] = cm->cdef_preset[1] = cm->cdef_preset[2] =
        cm->cdef_preset[3] = -1;
  }
  // Read CDEF param at the first non-skip coding block
  const int mask = (1 << (6 - MI_SIZE_LOG2));
  const int m = ~(mask - 1);
  const int index = cm->seq_params.sb_size == BLOCK_128X128
                        ? !!(mi_col & mask) + 2 * !!(mi_row & mask)
                        : 0;
  cm->mi_grid_visible[(mi_row & m) * cm->mi_stride + (mi_col & m)]
      ->mbmi.cdef_strength = cm->cdef_preset[index] =
      cm->cdef_preset[index] == -1 && !mbmi->skip
          ? aom_read_literal(r, cm->cdef_bits, ACCT_STR)
          : cm->cdef_preset[index];
}

static int read_delta_qindex(AV1_COMMON *cm, MACROBLOCKD *xd, aom_reader *r,
                             MB_MODE_INFO *const mbmi, int mi_col, int mi_row) {
  int sign, abs, reduced_delta_qindex = 0;
  BLOCK_SIZE bsize = mbmi->sb_type;
  const int b_col = mi_col & (cm->seq_params.mib_size - 1);
  const int b_row = mi_row & (cm->seq_params.mib_size - 1);
  const int read_delta_q_flag = (b_col == 0 && b_row == 0);
  FRAME_CONTEXT *ec_ctx = xd->tile_ctx;

  if ((bsize != cm->seq_params.sb_size || mbmi->skip == 0) &&
      read_delta_q_flag) {
    abs = aom_read_symbol(r, ec_ctx->delta_q_cdf, DELTA_Q_PROBS + 1, ACCT_STR);
    const int smallval = (abs < DELTA_Q_SMALL);

    if (!smallval) {
      const int rem_bits = aom_read_literal(r, 3, ACCT_STR) + 1;
      const int thr = (1 << rem_bits) + 1;
      abs = aom_read_literal(r, rem_bits, ACCT_STR) + thr;
    }

    if (abs) {
      sign = aom_read_bit(r, ACCT_STR);
    } else {
      sign = 1;
    }

    reduced_delta_qindex = sign ? -abs : abs;
  }
  return reduced_delta_qindex;
}
#if CONFIG_EXT_DELTA_Q
static int read_delta_lflevel(AV1_COMMON *cm, MACROBLOCKD *xd, aom_reader *r,
                              int lf_id, MB_MODE_INFO *const mbmi, int mi_col,
                              int mi_row) {
  int sign, abs, reduced_delta_lflevel = 0;
  BLOCK_SIZE bsize = mbmi->sb_type;
  const int b_col = mi_col & (cm->seq_params.mib_size - 1);
  const int b_row = mi_row & (cm->seq_params.mib_size - 1);
  const int read_delta_lf_flag = (b_col == 0 && b_row == 0);
  FRAME_CONTEXT *ec_ctx = xd->tile_ctx;

  if ((bsize != cm->seq_params.sb_size || mbmi->skip == 0) &&
      read_delta_lf_flag) {
    if (cm->delta_lf_multi) {
      assert(lf_id >= 0 &&
             lf_id < (av1_num_planes(cm) > 1 ? FRAME_LF_COUNT
                                             : FRAME_LF_COUNT - 2));
      abs = aom_read_symbol(r, ec_ctx->delta_lf_multi_cdf[lf_id],
                            DELTA_LF_PROBS + 1, ACCT_STR);
    } else {
      abs = aom_read_symbol(r, ec_ctx->delta_lf_cdf, DELTA_LF_PROBS + 1,
                            ACCT_STR);
    }
    const int smallval = (abs < DELTA_LF_SMALL);
    if (!smallval) {
      const int rem_bits = aom_read_literal(r, 3, ACCT_STR) + 1;
      const int thr = (1 << rem_bits) + 1;
      abs = aom_read_literal(r, rem_bits, ACCT_STR) + thr;
    }

    if (abs) {
      sign = aom_read_bit(r, ACCT_STR);
    } else {
      sign = 1;
    }

    reduced_delta_lflevel = sign ? -abs : abs;
  }
  return reduced_delta_lflevel;
}
#endif

static UV_PREDICTION_MODE read_intra_mode_uv(FRAME_CONTEXT *ec_ctx,
                                             aom_reader *r,
                                             CFL_ALLOWED_TYPE cfl_allowed,
                                             PREDICTION_MODE y_mode) {
  const UV_PREDICTION_MODE uv_mode =
      aom_read_symbol(r, ec_ctx->uv_mode_cdf[cfl_allowed][y_mode],
                      UV_INTRA_MODES - !cfl_allowed, ACCT_STR);
  return uv_mode;
}

static int read_cfl_alphas(FRAME_CONTEXT *const ec_ctx, aom_reader *r,
                           int *signs_out) {
  const int joint_sign =
      aom_read_symbol(r, ec_ctx->cfl_sign_cdf, CFL_JOINT_SIGNS, "cfl:signs");
  int idx = 0;
  // Magnitudes are only coded for nonzero values
  if (CFL_SIGN_U(joint_sign) != CFL_SIGN_ZERO) {
    aom_cdf_prob *cdf_u = ec_ctx->cfl_alpha_cdf[CFL_CONTEXT_U(joint_sign)];
    idx = aom_read_symbol(r, cdf_u, CFL_ALPHABET_SIZE, "cfl:alpha_u")
          << CFL_ALPHABET_SIZE_LOG2;
  }
  if (CFL_SIGN_V(joint_sign) != CFL_SIGN_ZERO) {
    aom_cdf_prob *cdf_v = ec_ctx->cfl_alpha_cdf[CFL_CONTEXT_V(joint_sign)];
    idx += aom_read_symbol(r, cdf_v, CFL_ALPHABET_SIZE, "cfl:alpha_v");
  }
  *signs_out = joint_sign;
  return idx;
}

static INTERINTRA_MODE read_interintra_mode(MACROBLOCKD *xd, aom_reader *r,
                                            int size_group) {
  const INTERINTRA_MODE ii_mode = (INTERINTRA_MODE)aom_read_symbol(
      r, xd->tile_ctx->interintra_mode_cdf[size_group], INTERINTRA_MODES,
      ACCT_STR);
  return ii_mode;
}

static PREDICTION_MODE read_inter_mode(FRAME_CONTEXT *ec_ctx, aom_reader *r,
                                       int16_t ctx) {
  int16_t mode_ctx = ctx & NEWMV_CTX_MASK;
  int is_newmv, is_zeromv, is_refmv;
  is_newmv = aom_read_symbol(r, ec_ctx->newmv_cdf[mode_ctx], 2, ACCT_STR) == 0;
  if (is_newmv) return NEWMV;

  mode_ctx = (ctx >> GLOBALMV_OFFSET) & GLOBALMV_CTX_MASK;
  is_zeromv =
      aom_read_symbol(r, ec_ctx->zeromv_cdf[mode_ctx], 2, ACCT_STR) == 0;
  if (is_zeromv) return GLOBALMV;

  mode_ctx = (ctx >> REFMV_OFFSET) & REFMV_CTX_MASK;
  is_refmv = aom_read_symbol(r, ec_ctx->refmv_cdf[mode_ctx], 2, ACCT_STR) == 0;
  if (is_refmv)
    return NEARESTMV;
  else
    return NEARMV;
}

static void read_drl_idx(FRAME_CONTEXT *ec_ctx, MACROBLOCKD *xd,
                         MB_MODE_INFO *mbmi, aom_reader *r) {
  uint8_t ref_frame_type = av1_ref_frame_type(mbmi->ref_frame);
  mbmi->ref_mv_idx = 0;
  if (mbmi->mode == NEWMV || mbmi->mode == NEW_NEWMV) {
    for (int idx = 0; idx < 2; ++idx) {
      if (xd->ref_mv_count[ref_frame_type] > idx + 1) {
        uint8_t drl_ctx = av1_drl_ctx(xd->ref_mv_stack[ref_frame_type], idx);
        int drl_idx = aom_read_symbol(r, ec_ctx->drl_cdf[drl_ctx], 2, ACCT_STR);
        mbmi->ref_mv_idx = idx + drl_idx;
        if (!drl_idx) return;
      }
    }
  }
  if (have_nearmv_in_inter_mode(mbmi->mode)) {
    // Offset the NEARESTMV mode.
    // TODO(jingning): Unify the two syntax decoding loops after the NEARESTMV
    // mode is factored in.
    for (int idx = 1; idx < 3; ++idx) {
      if (xd->ref_mv_count[ref_frame_type] > idx + 1) {
        uint8_t drl_ctx = av1_drl_ctx(xd->ref_mv_stack[ref_frame_type], idx);
        int drl_idx = aom_read_symbol(r, ec_ctx->drl_cdf[drl_ctx], 2, ACCT_STR);
        mbmi->ref_mv_idx = idx + drl_idx - 1;
        if (!drl_idx) return;
      }
    }
  }
}

static MOTION_MODE read_motion_mode(AV1_COMMON *cm, MACROBLOCKD *xd,
                                    MODE_INFO *mi, aom_reader *r) {
  MB_MODE_INFO *mbmi = &mi->mbmi;

  if (cm->switchable_motion_mode == 0) return SIMPLE_TRANSLATION;
  if (mbmi->skip_mode) return SIMPLE_TRANSLATION;

  const MOTION_MODE last_motion_mode_allowed =
      motion_mode_allowed(xd->global_motion, xd, mi);
  int motion_mode;

  if (last_motion_mode_allowed == SIMPLE_TRANSLATION) return SIMPLE_TRANSLATION;

  if (last_motion_mode_allowed == OBMC_CAUSAL) {
    motion_mode =
        aom_read_symbol(r, xd->tile_ctx->obmc_cdf[mbmi->sb_type], 2, ACCT_STR);
    return (MOTION_MODE)(SIMPLE_TRANSLATION + motion_mode);
  } else {
    motion_mode =
        aom_read_symbol(r, xd->tile_ctx->motion_mode_cdf[mbmi->sb_type],
                        MOTION_MODES, ACCT_STR);
    return (MOTION_MODE)(SIMPLE_TRANSLATION + motion_mode);
  }
}

static PREDICTION_MODE read_inter_compound_mode(MACROBLOCKD *xd, aom_reader *r,
                                                int16_t ctx) {
  const int mode =
      aom_read_symbol(r, xd->tile_ctx->inter_compound_mode_cdf[ctx],
                      INTER_COMPOUND_MODES, ACCT_STR);
  assert(is_inter_compound_mode(NEAREST_NEARESTMV + mode));
  return NEAREST_NEARESTMV + mode;
}

#if CONFIG_SPATIAL_SEGMENTATION
int av1_neg_deinterleave(int diff, int ref, int max) {
  if (!ref) return diff;
  if (ref >= (max - 1)) return max - diff - 1;
  if (2 * ref < max) {
    if (diff <= 2 * ref) {
      if (diff & 1)
        return ref + ((diff + 1) >> 1);
      else
        return ref - (diff >> 1);
    }
    return diff;
  } else {
    if (diff <= 2 * (max - ref - 1)) {
      if (diff & 1)
        return ref + ((diff + 1) >> 1);
      else
        return ref - (diff >> 1);
    }
    return max - (diff + 1);
  }
}

static int read_segment_id(AV1_COMMON *const cm, MACROBLOCKD *const xd,
                           int mi_row, int mi_col, aom_reader *r, int skip) {
  FRAME_CONTEXT *ec_ctx = xd->tile_ctx;
  struct segmentation *const seg = &cm->seg;
  struct segmentation_probs *const segp = &ec_ctx->seg;
  int prev_ul = -1; /* Top left segment_id */
  int prev_l = -1;  /* Current left segment_id */
  int prev_u = -1;  /* Current top segment_id */

  if ((xd->up_available) && (xd->left_available))
    prev_ul = get_segment_id(cm, cm->current_frame_seg_map, BLOCK_4X4,
                             mi_row - 1, mi_col - 1);

  if (xd->up_available)
    prev_u = get_segment_id(cm, cm->current_frame_seg_map, BLOCK_4X4,
                            mi_row - 1, mi_col - 0);

  if (xd->left_available)
    prev_l = get_segment_id(cm, cm->current_frame_seg_map, BLOCK_4X4,
                            mi_row - 0, mi_col - 1);

  int cdf_num = pick_spatial_seg_cdf(prev_ul, prev_u, prev_l);
  int pred = pick_spatial_seg_pred(prev_ul, prev_u, prev_l);

  if (skip) return pred;

  aom_cdf_prob *pred_cdf = segp->spatial_pred_seg_cdf[cdf_num];
  int coded_id = aom_read_symbol(r, pred_cdf, 8, ACCT_STR);

  int segment_id =
      av1_neg_deinterleave(coded_id, pred, seg->last_active_segid + 1);

  if (segment_id < 0 || segment_id > seg->last_active_segid) {
    aom_internal_error(&cm->error, AOM_CODEC_CORRUPT_FRAME,
                       "Corrupted segment_ids");
  }
  return segment_id;
}
#else
static int read_segment_id(aom_reader *r, struct segmentation_probs *segp) {
  return aom_read_symbol(r, segp->tree_cdf, MAX_SEGMENTS, ACCT_STR);
}
#endif

static int dec_get_segment_id(const AV1_COMMON *cm, const uint8_t *segment_ids,
                              int mi_offset, int x_mis, int y_mis) {
  int segment_id = INT_MAX;

  for (int y = 0; y < y_mis; y++)
    for (int x = 0; x < x_mis; x++)
      segment_id =
          AOMMIN(segment_id, segment_ids[mi_offset + y * cm->mi_cols + x]);

  assert(segment_id >= 0 && segment_id < MAX_SEGMENTS);
  return segment_id;
}

static void set_segment_id(AV1_COMMON *cm, int mi_offset, int x_mis, int y_mis,
                           int segment_id) {
  assert(segment_id >= 0 && segment_id < MAX_SEGMENTS);

  for (int y = 0; y < y_mis; y++)
    for (int x = 0; x < x_mis; x++)
      cm->current_frame_seg_map[mi_offset + y * cm->mi_cols + x] = segment_id;
}

static int read_intra_segment_id(AV1_COMMON *const cm, MACROBLOCKD *const xd,
                                 int mi_row, int mi_col, int bsize,
                                 aom_reader *r, int skip) {
  struct segmentation *const seg = &cm->seg;
  const int mi_offset = mi_row * cm->mi_cols + mi_col;
  const int bw = mi_size_wide[bsize];
  const int bh = mi_size_high[bsize];
  const int x_mis = AOMMIN(cm->mi_cols - mi_col, bw);
  const int y_mis = AOMMIN(cm->mi_rows - mi_row, bh);

  if (!seg->enabled) return 0;  // Default for disabled segmentation

  assert(seg->update_map && !seg->temporal_update);

#if CONFIG_SPATIAL_SEGMENTATION
  const int segment_id = read_segment_id(cm, xd, mi_row, mi_col, r, skip);
#else
  FRAME_CONTEXT *ec_ctx = xd->tile_ctx;
  (void)skip;
  const int segment_id = read_segment_id(r, &ec_ctx->seg);
#endif
  set_segment_id(cm, mi_offset, x_mis, y_mis, segment_id);
  return segment_id;
}

static void copy_segment_id(const AV1_COMMON *cm,
                            const uint8_t *last_segment_ids,
                            uint8_t *current_segment_ids, int mi_offset,
                            int x_mis, int y_mis) {
  for (int y = 0; y < y_mis; y++)
    for (int x = 0; x < x_mis; x++)
      current_segment_ids[mi_offset + y * cm->mi_cols + x] =
          last_segment_ids ? last_segment_ids[mi_offset + y * cm->mi_cols + x]
                           : 0;
}

static int get_predicted_segment_id(AV1_COMMON *const cm, int mi_offset,
                                    int x_mis, int y_mis) {
  return cm->last_frame_seg_map ? dec_get_segment_id(cm, cm->last_frame_seg_map,
                                                     mi_offset, x_mis, y_mis)
                                : 0;
}

static int read_inter_segment_id(AV1_COMMON *const cm, MACROBLOCKD *const xd,
                                 int mi_row, int mi_col, int preskip,
                                 aom_reader *r) {
  struct segmentation *const seg = &cm->seg;
  FRAME_CONTEXT *ec_ctx = xd->tile_ctx;
  struct segmentation_probs *const segp = &ec_ctx->seg;

  MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;
  int segment_id;
  const int mi_offset = mi_row * cm->mi_cols + mi_col;
  const int bw = mi_size_wide[mbmi->sb_type];
  const int bh = mi_size_high[mbmi->sb_type];

  // TODO(slavarnway): move x_mis, y_mis into xd ?????
  const int x_mis = AOMMIN(cm->mi_cols - mi_col, bw);
  const int y_mis = AOMMIN(cm->mi_rows - mi_row, bh);

  if (!seg->enabled) return 0;  // Default for disabled segmentation

  if (!seg->update_map) {
    copy_segment_id(cm, cm->last_frame_seg_map, cm->current_frame_seg_map,
                    mi_offset, x_mis, y_mis);
    return get_predicted_segment_id(cm, mi_offset, x_mis, y_mis);
  }

#if CONFIG_SPATIAL_SEGMENTATION
  if (preskip) {
    if (!seg->preskip_segid) return 0;
  } else {
    if (seg->preskip_segid) return mbmi->segment_id;
    if (mbmi->skip) {
      if (seg->temporal_update) {
        mbmi->seg_id_predicted = 0;
      }
      segment_id = read_segment_id(cm, xd, mi_row, mi_col, r, 1);
      set_segment_id(cm, mi_offset, x_mis, y_mis, segment_id);
      return segment_id;
    }
  }
#endif
  (void)preskip;
  if (seg->temporal_update) {
    const int ctx = av1_get_pred_context_seg_id(xd);
    aom_cdf_prob *pred_cdf = segp->pred_cdf[ctx];
    mbmi->seg_id_predicted = aom_read_symbol(r, pred_cdf, 2, ACCT_STR);
    if (mbmi->seg_id_predicted) {
      segment_id = get_predicted_segment_id(cm, mi_offset, x_mis, y_mis);
    } else {
#if CONFIG_SPATIAL_SEGMENTATION
      segment_id = read_segment_id(cm, xd, mi_row, mi_col, r, 0);
#else
      segment_id = read_segment_id(r, segp);
#endif
    }
  } else {
#if CONFIG_SPATIAL_SEGMENTATION
    segment_id = read_segment_id(cm, xd, mi_row, mi_col, r, 0);
#else
    segment_id = read_segment_id(r, segp);
#endif
  }
  set_segment_id(cm, mi_offset, x_mis, y_mis, segment_id);
  return segment_id;
}

static int read_skip_mode(AV1_COMMON *cm, const MACROBLOCKD *xd, int segment_id,
                          aom_reader *r) {
  if (!cm->skip_mode_flag) return 0;

  if (segfeature_active(&cm->seg, segment_id, SEG_LVL_SKIP)) {
    return 0;
  }

  if (!is_comp_ref_allowed(xd->mi[0]->mbmi.sb_type)) return 0;

  const int ctx = av1_get_skip_mode_context(xd);
  FRAME_CONTEXT *ec_ctx = xd->tile_ctx;
  const int skip_mode =
      aom_read_symbol(r, ec_ctx->skip_mode_cdfs[ctx], 2, ACCT_STR);
  return skip_mode;
}

static int read_skip(AV1_COMMON *cm, const MACROBLOCKD *xd, int segment_id,
                     aom_reader *r) {
  if (segfeature_active(&cm->seg, segment_id, SEG_LVL_SKIP)) {
    return 1;
  } else {
    const int ctx = av1_get_skip_context(xd);
    FRAME_CONTEXT *ec_ctx = xd->tile_ctx;
    const int skip = aom_read_symbol(r, ec_ctx->skip_cdfs[ctx], 2, ACCT_STR);
    return skip;
  }
}

// Merge the sorted list of cached colors(cached_colors[0...n_cached_colors-1])
// and the sorted list of transmitted colors(colors[n_cached_colors...n-1]) into
// one single sorted list(colors[...]).
static void merge_colors(uint16_t *colors, uint16_t *cached_colors,
                         int n_colors, int n_cached_colors) {
  if (n_cached_colors == 0) return;
  int cache_idx = 0, trans_idx = n_cached_colors;
  for (int i = 0; i < n_colors; ++i) {
    if (cache_idx < n_cached_colors &&
        (trans_idx >= n_colors ||
         cached_colors[cache_idx] <= colors[trans_idx])) {
      colors[i] = cached_colors[cache_idx++];
    } else {
      assert(trans_idx < n_colors);
      colors[i] = colors[trans_idx++];
    }
  }
}

static void read_palette_colors_y(MACROBLOCKD *const xd, int bit_depth,
                                  PALETTE_MODE_INFO *const pmi, aom_reader *r) {
  uint16_t color_cache[2 * PALETTE_MAX_SIZE];
  uint16_t cached_colors[PALETTE_MAX_SIZE];
  const int n_cache = av1_get_palette_cache(xd, 0, color_cache);
  const int n = pmi->palette_size[0];
  int idx = 0;
  for (int i = 0; i < n_cache && idx < n; ++i)
    if (aom_read_bit(r, ACCT_STR)) cached_colors[idx++] = color_cache[i];
  if (idx < n) {
    const int n_cached_colors = idx;
    pmi->palette_colors[idx++] = aom_read_literal(r, bit_depth, ACCT_STR);
    if (idx < n) {
      const int min_bits = bit_depth - 3;
      int bits = min_bits + aom_read_literal(r, 2, ACCT_STR);
      int range = (1 << bit_depth) - pmi->palette_colors[idx - 1] - 1;
      for (; idx < n; ++idx) {
        assert(range >= 0);
        const int delta = aom_read_literal(r, bits, ACCT_STR) + 1;
        pmi->palette_colors[idx] = clamp(pmi->palette_colors[idx - 1] + delta,
                                         0, (1 << bit_depth) - 1);
        range -= (pmi->palette_colors[idx] - pmi->palette_colors[idx - 1]);
        bits = AOMMIN(bits, av1_ceil_log2(range));
      }
    }
    merge_colors(pmi->palette_colors, cached_colors, n, n_cached_colors);
  } else {
    memcpy(pmi->palette_colors, cached_colors, n * sizeof(cached_colors[0]));
  }
}

static void read_palette_colors_uv(MACROBLOCKD *const xd, int bit_depth,
                                   PALETTE_MODE_INFO *const pmi,
                                   aom_reader *r) {
  const int n = pmi->palette_size[1];
  // U channel colors.
  uint16_t color_cache[2 * PALETTE_MAX_SIZE];
  uint16_t cached_colors[PALETTE_MAX_SIZE];
  const int n_cache = av1_get_palette_cache(xd, 1, color_cache);
  int idx = 0;
  for (int i = 0; i < n_cache && idx < n; ++i)
    if (aom_read_bit(r, ACCT_STR)) cached_colors[idx++] = color_cache[i];
  if (idx < n) {
    const int n_cached_colors = idx;
    idx += PALETTE_MAX_SIZE;
    pmi->palette_colors[idx++] = aom_read_literal(r, bit_depth, ACCT_STR);
    if (idx < PALETTE_MAX_SIZE + n) {
      const int min_bits = bit_depth - 3;
      int bits = min_bits + aom_read_literal(r, 2, ACCT_STR);
      int range = (1 << bit_depth) - pmi->palette_colors[idx - 1];
      for (; idx < PALETTE_MAX_SIZE + n; ++idx) {
        assert(range >= 0);
        const int delta = aom_read_literal(r, bits, ACCT_STR);
        pmi->palette_colors[idx] = clamp(pmi->palette_colors[idx - 1] + delta,
                                         0, (1 << bit_depth) - 1);
        range -= (pmi->palette_colors[idx] - pmi->palette_colors[idx - 1]);
        bits = AOMMIN(bits, av1_ceil_log2(range));
      }
    }
    merge_colors(pmi->palette_colors + PALETTE_MAX_SIZE, cached_colors, n,
                 n_cached_colors);
  } else {
    memcpy(pmi->palette_colors + PALETTE_MAX_SIZE, cached_colors,
           n * sizeof(cached_colors[0]));
  }

  // V channel colors.
  if (aom_read_bit(r, ACCT_STR)) {  // Delta encoding.
    const int min_bits_v = bit_depth - 4;
    const int max_val = 1 << bit_depth;
    int bits = min_bits_v + aom_read_literal(r, 2, ACCT_STR);
    pmi->palette_colors[2 * PALETTE_MAX_SIZE] =
        aom_read_literal(r, bit_depth, ACCT_STR);
    for (int i = 1; i < n; ++i) {
      int delta = aom_read_literal(r, bits, ACCT_STR);
      if (delta && aom_read_bit(r, ACCT_STR)) delta = -delta;
      int val = (int)pmi->palette_colors[2 * PALETTE_MAX_SIZE + i - 1] + delta;
      if (val < 0) val += max_val;
      if (val >= max_val) val -= max_val;
      pmi->palette_colors[2 * PALETTE_MAX_SIZE + i] = val;
    }
  } else {
    for (int i = 0; i < n; ++i) {
      pmi->palette_colors[2 * PALETTE_MAX_SIZE + i] =
          aom_read_literal(r, bit_depth, ACCT_STR);
    }
  }
}

static void read_palette_mode_info(AV1_COMMON *const cm, MACROBLOCKD *const xd,
                                   int mi_row, int mi_col, aom_reader *r) {
  const int num_planes = av1_num_planes(cm);
  MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;
  const BLOCK_SIZE bsize = mbmi->sb_type;
  assert(av1_allow_palette(cm->allow_screen_content_tools, bsize));
  PALETTE_MODE_INFO *const pmi = &mbmi->palette_mode_info;
  const int bsize_ctx = av1_get_palette_bsize_ctx(bsize);

  if (mbmi->mode == DC_PRED) {
    const int palette_mode_ctx = av1_get_palette_mode_ctx(xd);
    const int modev = aom_read_symbol(
        r, xd->tile_ctx->palette_y_mode_cdf[bsize_ctx][palette_mode_ctx], 2,
        ACCT_STR);
    if (modev) {
      pmi->palette_size[0] =
          aom_read_symbol(r, xd->tile_ctx->palette_y_size_cdf[bsize_ctx],
                          PALETTE_SIZES, ACCT_STR) +
          2;
      read_palette_colors_y(xd, cm->bit_depth, pmi, r);
    }
  }
  if (num_planes > 1 && mbmi->uv_mode == UV_DC_PRED &&
      is_chroma_reference(mi_row, mi_col, bsize, xd->plane[1].subsampling_x,
                          xd->plane[1].subsampling_y)) {
    const int palette_uv_mode_ctx = (pmi->palette_size[0] > 0);
    const int modev = aom_read_symbol(
        r, xd->tile_ctx->palette_uv_mode_cdf[palette_uv_mode_ctx], 2, ACCT_STR);
    if (modev) {
      pmi->palette_size[1] =
          aom_read_symbol(r, xd->tile_ctx->palette_uv_size_cdf[bsize_ctx],
                          PALETTE_SIZES, ACCT_STR) +
          2;
      read_palette_colors_uv(xd, cm->bit_depth, pmi, r);
    }
  }
}

static int read_angle_delta(aom_reader *r, aom_cdf_prob *cdf) {
  const int sym = aom_read_symbol(r, cdf, 2 * MAX_ANGLE_DELTA + 1, ACCT_STR);
  return sym - MAX_ANGLE_DELTA;
}

static void read_filter_intra_mode_info(const AV1_COMMON *const cm,
                                        MACROBLOCKD *const xd, aom_reader *r) {
  MODE_INFO *const mi = xd->mi[0];
  MB_MODE_INFO *const mbmi = &mi->mbmi;
  FILTER_INTRA_MODE_INFO *filter_intra_mode_info =
      &mbmi->filter_intra_mode_info;

  if (av1_filter_intra_allowed(cm, mbmi)) {
    filter_intra_mode_info->use_filter_intra = aom_read_symbol(
        r, xd->tile_ctx->filter_intra_cdfs[mbmi->sb_type], 2, ACCT_STR);
    if (filter_intra_mode_info->use_filter_intra) {
      filter_intra_mode_info->filter_intra_mode = aom_read_symbol(
          r, xd->tile_ctx->filter_intra_mode_cdf, FILTER_INTRA_MODES, ACCT_STR);
    }
  } else {
    filter_intra_mode_info->use_filter_intra = 0;
  }
}

void av1_read_tx_type(const AV1_COMMON *const cm, MACROBLOCKD *xd, int blk_row,
                      int blk_col, int plane, TX_SIZE tx_size, aom_reader *r) {
  MB_MODE_INFO *mbmi = &xd->mi[0]->mbmi;
  const int inter_block = is_inter_block(mbmi);
  FRAME_CONTEXT *ec_ctx = xd->tile_ctx;

  // only y plane's tx_type is transmitted
  if (plane > 0) return;
  const int txk_type_idx =
      av1_get_txk_type_index(mbmi->sb_type, blk_row, blk_col);
  TX_TYPE *tx_type = &mbmi->txk_type[txk_type_idx];

  const TX_SIZE square_tx_size = txsize_sqr_map[tx_size];
  if (get_ext_tx_types(tx_size, mbmi->sb_type, inter_block,
                       cm->reduced_tx_set_used) > 1 &&
      ((!cm->seg.enabled && cm->base_qindex > 0) ||
       (cm->seg.enabled && xd->qindex[mbmi->segment_id] > 0)) &&
      !mbmi->skip &&
      !segfeature_active(&cm->seg, mbmi->segment_id, SEG_LVL_SKIP)) {
    const TxSetType tx_set_type = get_ext_tx_set_type(
        tx_size, mbmi->sb_type, inter_block, cm->reduced_tx_set_used);
    const int eset = get_ext_tx_set(tx_size, mbmi->sb_type, inter_block,
                                    cm->reduced_tx_set_used);
    // eset == 0 should correspond to a set with only DCT_DCT and
    // there is no need to read the tx_type
    assert(eset != 0);

    if (inter_block) {
      *tx_type = av1_ext_tx_inv[tx_set_type][aom_read_symbol(
          r, ec_ctx->inter_ext_tx_cdf[eset][square_tx_size],
          av1_num_ext_tx_set[tx_set_type], ACCT_STR)];
    } else {
      PREDICTION_MODE intra_dir;
      if (mbmi->filter_intra_mode_info.use_filter_intra)
        intra_dir =
            fimode_to_intradir[mbmi->filter_intra_mode_info.filter_intra_mode];
      else
        intra_dir = mbmi->mode;
      *tx_type = av1_ext_tx_inv[tx_set_type][aom_read_symbol(
          r, ec_ctx->intra_ext_tx_cdf[eset][square_tx_size][intra_dir],
          av1_num_ext_tx_set[tx_set_type], ACCT_STR)];
    }
  } else {
    *tx_type = DCT_DCT;
  }
}

static INLINE void read_mv(aom_reader *r, MV *mv, const MV *ref,
                           nmv_context *ctx, MvSubpelPrecision precision);

static INLINE int is_mv_valid(const MV *mv);

static INLINE int assign_dv(AV1_COMMON *cm, MACROBLOCKD *xd, int_mv *mv,
                            const int_mv *ref_mv, int mi_row, int mi_col,
                            BLOCK_SIZE bsize, aom_reader *r) {
  FRAME_CONTEXT *ec_ctx = xd->tile_ctx;
  read_mv(r, &mv->as_mv, &ref_mv->as_mv, &ec_ctx->ndvc, MV_SUBPEL_NONE);
  // DV should not have sub-pel.
  assert((mv->as_mv.col & 7) == 0);
  assert((mv->as_mv.row & 7) == 0);
  mv->as_mv.col = (mv->as_mv.col >> 3) * 8;
  mv->as_mv.row = (mv->as_mv.row >> 3) * 8;
  int valid = is_mv_valid(&mv->as_mv) &&
              av1_is_dv_valid(mv->as_mv, &xd->tile, mi_row, mi_col, bsize,
                              cm->seq_params.mib_size_log2);
  return valid;
}

static void read_intrabc_info(AV1_COMMON *const cm, MACROBLOCKD *const xd,
                              int mi_row, int mi_col, aom_reader *r) {
  MODE_INFO *const mi = xd->mi[0];
  MB_MODE_INFO *const mbmi = &mi->mbmi;
  FRAME_CONTEXT *ec_ctx = xd->tile_ctx;
  mbmi->use_intrabc = aom_read_symbol(r, ec_ctx->intrabc_cdf, 2, ACCT_STR);
  if (mbmi->use_intrabc) {
    BLOCK_SIZE bsize = mbmi->sb_type;
    mbmi->mode = DC_PRED;
    mbmi->uv_mode = UV_DC_PRED;
    mbmi->interp_filters = av1_broadcast_interp_filter(BILINEAR);
    mbmi->motion_mode = SIMPLE_TRANSLATION;

    int16_t inter_mode_ctx[MODE_CTX_REF_FRAMES];
    int_mv ref_mvs[INTRA_FRAME + 1][MAX_MV_REF_CANDIDATES];

    av1_find_mv_refs(cm, xd, mi, INTRA_FRAME, xd->ref_mv_count,
                     xd->ref_mv_stack, ref_mvs, mi_row, mi_col, inter_mode_ctx);

    int_mv nearestmv, nearmv;

#if CONFIG_AMVR
    av1_find_best_ref_mvs(0, ref_mvs[INTRA_FRAME], &nearestmv, &nearmv, 0);
#else
    av1_find_best_ref_mvs(0, ref_mvs[INTRA_FRAME], &nearestmv, &nearmv);
#endif
    int_mv dv_ref = nearestmv.as_int == 0 ? nearmv : nearestmv;
    if (dv_ref.as_int == 0)
      av1_find_ref_dv(&dv_ref, &xd->tile, cm->seq_params.mib_size, mi_row,
                      mi_col);
    // Ref DV should not have sub-pel.
    int valid_dv = (dv_ref.as_mv.col & 7) == 0 && (dv_ref.as_mv.row & 7) == 0;
    dv_ref.as_mv.col = (dv_ref.as_mv.col >> 3) * 8;
    dv_ref.as_mv.row = (dv_ref.as_mv.row >> 3) * 8;
    valid_dv = valid_dv && assign_dv(cm, xd, &mbmi->mv[0], &dv_ref, mi_row,
                                     mi_col, bsize, r);
    if (!valid_dv) {
      // Intra bc motion vectors are not valid - signal corrupt frame
      aom_merge_corrupted_flag(&xd->corrupted, 1);
    }
  }
}

static void read_intra_frame_mode_info(AV1_COMMON *const cm,
                                       MACROBLOCKD *const xd, int mi_row,
                                       int mi_col, aom_reader *r) {
  MODE_INFO *const mi = xd->mi[0];
  MB_MODE_INFO *const mbmi = &mi->mbmi;
  const MODE_INFO *above_mi = xd->above_mi;
  const MODE_INFO *left_mi = xd->left_mi;
  const BLOCK_SIZE bsize = mbmi->sb_type;
#if CONFIG_SPATIAL_SEGMENTATION
  struct segmentation *const seg = &cm->seg;
#endif

  FRAME_CONTEXT *ec_ctx = xd->tile_ctx;

#if !CONFIG_SPATIAL_SEGMENTATION
  mbmi->segment_id = read_intra_segment_id(cm, xd, mi_row, mi_col, bsize, r, 0);
#else
  if (seg->preskip_segid)
    mbmi->segment_id =
        read_intra_segment_id(cm, xd, mi_row, mi_col, bsize, r, 0);
#endif

  mbmi->skip = read_skip(cm, xd, mbmi->segment_id, r);

#if CONFIG_SPATIAL_SEGMENTATION
  if (!seg->preskip_segid)
    mbmi->segment_id =
        read_intra_segment_id(cm, xd, mi_row, mi_col, bsize, r, mbmi->skip);
#endif

  read_cdef(cm, r, mbmi, mi_col, mi_row);

  if (cm->delta_q_present_flag) {
    xd->current_qindex =
        xd->prev_qindex +
        read_delta_qindex(cm, xd, r, mbmi, mi_col, mi_row) * cm->delta_q_res;
    /* Normative: Clamp to [1,MAXQ] to not interfere with lossless mode */
    xd->current_qindex = clamp(xd->current_qindex, 1, MAXQ);
    xd->prev_qindex = xd->current_qindex;
#if CONFIG_EXT_DELTA_Q
    if (cm->delta_lf_present_flag) {
      if (cm->delta_lf_multi) {
        const int frame_lf_count =
            av1_num_planes(cm) > 1 ? FRAME_LF_COUNT : FRAME_LF_COUNT - 2;
        for (int lf_id = 0; lf_id < frame_lf_count; ++lf_id) {
          const int tmp_lvl =
              xd->prev_delta_lf[lf_id] +
              read_delta_lflevel(cm, xd, r, lf_id, mbmi, mi_col, mi_row) *
                  cm->delta_lf_res;
          mbmi->curr_delta_lf[lf_id] = xd->curr_delta_lf[lf_id] =
              clamp(tmp_lvl, -MAX_LOOP_FILTER, MAX_LOOP_FILTER);
          xd->prev_delta_lf[lf_id] = xd->curr_delta_lf[lf_id];
        }
      } else {
        const int tmp_lvl =
            xd->prev_delta_lf_from_base +
            read_delta_lflevel(cm, xd, r, -1, mbmi, mi_col, mi_row) *
                cm->delta_lf_res;
        mbmi->current_delta_lf_from_base = xd->current_delta_lf_from_base =
            clamp(tmp_lvl, -MAX_LOOP_FILTER, MAX_LOOP_FILTER);
        xd->prev_delta_lf_from_base = xd->current_delta_lf_from_base;
      }
    }
#endif
  }

  mbmi->current_q_index = xd->current_qindex;

  mbmi->ref_frame[0] = INTRA_FRAME;
  mbmi->ref_frame[1] = NONE_FRAME;
  mbmi->palette_mode_info.palette_size[0] = 0;
  mbmi->palette_mode_info.palette_size[1] = 0;
  mbmi->filter_intra_mode_info.use_filter_intra = 0;

  xd->above_txfm_context =
      cm->above_txfm_context + (mi_col << TX_UNIT_WIDE_LOG2);
  xd->left_txfm_context = xd->left_txfm_context_buffer +
                          ((mi_row & MAX_MIB_MASK) << TX_UNIT_HIGH_LOG2);

  if (av1_allow_intrabc(cm)) {
    read_intrabc_info(cm, xd, mi_row, mi_col, r);
    if (is_intrabc_block(mbmi)) return;
  }

  mbmi->mode = read_intra_mode(r, get_y_mode_cdf(ec_ctx, above_mi, left_mi));

  const int use_angle_delta = av1_use_angle_delta(bsize);
  mbmi->angle_delta[PLANE_TYPE_Y] =
      (use_angle_delta && av1_is_directional_mode(mbmi->mode))
          ? read_angle_delta(r, ec_ctx->angle_delta_cdf[mbmi->mode - V_PRED])
          : 0;

#if CONFIG_MONO_VIDEO
  if (!cm->seq_params.monochrome &&
      is_chroma_reference(mi_row, mi_col, bsize, xd->plane[1].subsampling_x,
                          xd->plane[1].subsampling_y))
#else
  if (is_chroma_reference(mi_row, mi_col, bsize, xd->plane[1].subsampling_x,
                          xd->plane[1].subsampling_y))
#endif  // CONFIG_MONO_VIDEO
  {
    xd->cfl.is_chroma_reference = 1;
    mbmi->uv_mode =
        read_intra_mode_uv(ec_ctx, r, is_cfl_allowed(mbmi), mbmi->mode);
    if (mbmi->uv_mode == UV_CFL_PRED) {
      mbmi->cfl_alpha_idx = read_cfl_alphas(ec_ctx, r, &mbmi->cfl_alpha_signs);
      xd->cfl.store_y = 1;
    } else {
      xd->cfl.store_y = 0;
    }
    mbmi->angle_delta[PLANE_TYPE_UV] =
        (use_angle_delta && av1_is_directional_mode(get_uv_mode(mbmi->uv_mode)))
            ? read_angle_delta(r,
                               ec_ctx->angle_delta_cdf[mbmi->uv_mode - V_PRED])
            : 0;
  } else {
    // Avoid decoding angle_info if there is is no chroma prediction
    mbmi->uv_mode = UV_DC_PRED;
    xd->cfl.is_chroma_reference = 0;
    xd->cfl.store_y = 1;
  }

  if (av1_allow_palette(cm->allow_screen_content_tools, bsize))
    read_palette_mode_info(cm, xd, mi_row, mi_col, r);

  read_filter_intra_mode_info(cm, xd, r);
}

static int read_mv_component(aom_reader *r, nmv_component *mvcomp,
                             int use_subpel, int usehp) {
  int mag, d, fr, hp;
  const int sign = aom_read_symbol(r, mvcomp->sign_cdf, 2, ACCT_STR);
  const int mv_class =
      aom_read_symbol(r, mvcomp->classes_cdf, MV_CLASSES, ACCT_STR);
  const int class0 = mv_class == MV_CLASS_0;

  // Integer part
  if (class0) {
    d = aom_read_symbol(r, mvcomp->class0_cdf, CLASS0_SIZE, ACCT_STR);
    mag = 0;
  } else {
    const int n = mv_class + CLASS0_BITS - 1;  // number of bits
    d = 0;
    for (int i = 0; i < n; ++i)
      d |= aom_read_symbol(r, mvcomp->bits_cdf[i], 2, ACCT_STR) << i;
    mag = CLASS0_SIZE << (mv_class + 2);
  }

  if (use_subpel) {
    // Fractional part
    fr = aom_read_symbol(r, class0 ? mvcomp->class0_fp_cdf[d] : mvcomp->fp_cdf,
                         MV_FP_SIZE, ACCT_STR);

    // High precision part (if hp is not used, the default value of the hp is 1)
    hp = usehp ? aom_read_symbol(
                     r, class0 ? mvcomp->class0_hp_cdf : mvcomp->hp_cdf, 2,
                     ACCT_STR)
               : 1;
  } else {
    fr = 3;
    hp = 1;
  }

  // Result
  mag += ((d << 3) | (fr << 1) | hp) + 1;
  return sign ? -mag : mag;
}

static INLINE void read_mv(aom_reader *r, MV *mv, const MV *ref,
                           nmv_context *ctx, MvSubpelPrecision precision) {
  MV diff = { 0, 0 };
  const MV_JOINT_TYPE joint_type =
      (MV_JOINT_TYPE)aom_read_symbol(r, ctx->joints_cdf, MV_JOINTS, ACCT_STR);

  if (mv_joint_vertical(joint_type))
    diff.row = read_mv_component(r, &ctx->comps[0], precision > MV_SUBPEL_NONE,
                                 precision > MV_SUBPEL_LOW_PRECISION);

  if (mv_joint_horizontal(joint_type))
    diff.col = read_mv_component(r, &ctx->comps[1], precision > MV_SUBPEL_NONE,
                                 precision > MV_SUBPEL_LOW_PRECISION);

  mv->row = ref->row + diff.row;
  mv->col = ref->col + diff.col;
}

static REFERENCE_MODE read_block_reference_mode(AV1_COMMON *cm,
                                                const MACROBLOCKD *xd,
                                                aom_reader *r) {
  if (!is_comp_ref_allowed(xd->mi[0]->mbmi.sb_type)) return SINGLE_REFERENCE;
  if (cm->reference_mode == REFERENCE_MODE_SELECT) {
    const int ctx = av1_get_reference_mode_context(cm, xd);
    const REFERENCE_MODE mode = (REFERENCE_MODE)aom_read_symbol(
        r, xd->tile_ctx->comp_inter_cdf[ctx], 2, ACCT_STR);
    return mode;  // SINGLE_REFERENCE or COMPOUND_REFERENCE
  } else {
    assert(cm->reference_mode == SINGLE_REFERENCE);
    return cm->reference_mode;
  }
}

#define READ_REF_BIT(pname) \
  aom_read_symbol(r, av1_get_pred_cdf_##pname(xd), 2, ACCT_STR)

static COMP_REFERENCE_TYPE read_comp_reference_type(const MACROBLOCKD *xd,
                                                    aom_reader *r) {
  const int ctx = av1_get_comp_reference_type_context(xd);
  const COMP_REFERENCE_TYPE comp_ref_type =
      (COMP_REFERENCE_TYPE)aom_read_symbol(
          r, xd->tile_ctx->comp_ref_type_cdf[ctx], 2, ACCT_STR);
  return comp_ref_type;  // UNIDIR_COMP_REFERENCE or BIDIR_COMP_REFERENCE
}

static void set_ref_frames_for_skip_mode(AV1_COMMON *const cm,
                                         MV_REFERENCE_FRAME ref_frame[2]) {
  ref_frame[0] = LAST_FRAME + cm->ref_frame_idx_0;
  ref_frame[1] = LAST_FRAME + cm->ref_frame_idx_1;
}

// Read the referncence frame
static void read_ref_frames(AV1_COMMON *const cm, MACROBLOCKD *const xd,
                            aom_reader *r, int segment_id,
                            MV_REFERENCE_FRAME ref_frame[2]) {
  if (xd->mi[0]->mbmi.skip_mode) {
    set_ref_frames_for_skip_mode(cm, ref_frame);
    return;
  }

  if (segfeature_active(&cm->seg, segment_id, SEG_LVL_REF_FRAME)) {
    ref_frame[0] = (MV_REFERENCE_FRAME)get_segdata(&cm->seg, segment_id,
                                                   SEG_LVL_REF_FRAME);
    ref_frame[1] = NONE_FRAME;
  }
#if CONFIG_SEGMENT_GLOBALMV
  else if (segfeature_active(&cm->seg, segment_id, SEG_LVL_SKIP) ||
           segfeature_active(&cm->seg, segment_id, SEG_LVL_GLOBALMV))
#else
  else if (segfeature_active(&cm->seg, segment_id, SEG_LVL_SKIP))
#endif
  {
    ref_frame[0] = LAST_FRAME;
    ref_frame[1] = NONE_FRAME;
  } else {
    const REFERENCE_MODE mode = read_block_reference_mode(cm, xd, r);

    if (mode == COMPOUND_REFERENCE) {
      const COMP_REFERENCE_TYPE comp_ref_type = read_comp_reference_type(xd, r);

      if (comp_ref_type == UNIDIR_COMP_REFERENCE) {
        const int bit = READ_REF_BIT(uni_comp_ref_p);
        if (bit) {
          ref_frame[0] = BWDREF_FRAME;
          ref_frame[1] = ALTREF_FRAME;
        } else {
          const int bit1 = READ_REF_BIT(uni_comp_ref_p1);
          if (bit1) {
            const int bit2 = READ_REF_BIT(uni_comp_ref_p2);
            if (bit2) {
              ref_frame[0] = LAST_FRAME;
              ref_frame[1] = GOLDEN_FRAME;
            } else {
              ref_frame[0] = LAST_FRAME;
              ref_frame[1] = LAST3_FRAME;
            }
          } else {
            ref_frame[0] = LAST_FRAME;
            ref_frame[1] = LAST2_FRAME;
          }
        }

        return;
      }

      assert(comp_ref_type == BIDIR_COMP_REFERENCE);

      const int idx = 1;
      const int bit = READ_REF_BIT(comp_ref_p);
      // Decode forward references.
      if (!bit) {
        const int bit1 = READ_REF_BIT(comp_ref_p1);
        ref_frame[!idx] = cm->comp_fwd_ref[bit1 ? 1 : 0];
      } else {
        const int bit2 = READ_REF_BIT(comp_ref_p2);
        ref_frame[!idx] = cm->comp_fwd_ref[bit2 ? 3 : 2];
      }

      // Decode backward references.
      const int bit_bwd = READ_REF_BIT(comp_bwdref_p);
      if (!bit_bwd) {
        const int bit1_bwd = READ_REF_BIT(comp_bwdref_p1);
        ref_frame[idx] = cm->comp_bwd_ref[bit1_bwd];
      } else {
        ref_frame[idx] = cm->comp_bwd_ref[2];
      }
    } else if (mode == SINGLE_REFERENCE) {
      const int bit0 = READ_REF_BIT(single_ref_p1);
      if (bit0) {
        const int bit1 = READ_REF_BIT(single_ref_p2);
        if (!bit1) {
          const int bit5 = READ_REF_BIT(single_ref_p6);
          ref_frame[0] = bit5 ? ALTREF2_FRAME : BWDREF_FRAME;
        } else {
          ref_frame[0] = ALTREF_FRAME;
        }
      } else {
        const int bit2 = READ_REF_BIT(single_ref_p3);
        if (bit2) {
          const int bit4 = READ_REF_BIT(single_ref_p5);
          ref_frame[0] = bit4 ? GOLDEN_FRAME : LAST3_FRAME;
        } else {
          const int bit3 = READ_REF_BIT(single_ref_p4);
          ref_frame[0] = bit3 ? LAST2_FRAME : LAST_FRAME;
        }
      }

      ref_frame[1] = NONE_FRAME;
    } else {
      assert(0 && "Invalid prediction mode.");
    }
  }
}

static INLINE void read_mb_interp_filter(AV1_COMMON *const cm,
                                         MACROBLOCKD *const xd,
                                         MB_MODE_INFO *const mbmi,
                                         aom_reader *r) {
  FRAME_CONTEXT *ec_ctx = xd->tile_ctx;

  if (!av1_is_interp_needed(xd)) {
    set_default_interp_filters(mbmi, cm->interp_filter);
    return;
  }

  if (cm->interp_filter != SWITCHABLE) {
    mbmi->interp_filters = av1_broadcast_interp_filter(cm->interp_filter);
  } else {
    InterpFilter ref0_filter[2] = { EIGHTTAP_REGULAR, EIGHTTAP_REGULAR };
    for (int dir = 0; dir < 2; ++dir) {
      const int ctx = av1_get_pred_context_switchable_interp(xd, dir);
      ref0_filter[dir] = (InterpFilter)aom_read_symbol(
          r, ec_ctx->switchable_interp_cdf[ctx], SWITCHABLE_FILTERS, ACCT_STR);
      if (cm->seq_params.enable_dual_filter == 0) {
        ref0_filter[1] = ref0_filter[0];
        break;
      }
    }
    // The index system works as: (0, 1) -> (vertical, horizontal) filter types
    mbmi->interp_filters =
        av1_make_interp_filters(ref0_filter[0], ref0_filter[1]);
  }
}

static void read_intra_block_mode_info(AV1_COMMON *const cm, const int mi_row,
                                       const int mi_col, MACROBLOCKD *const xd,
                                       MODE_INFO *mi, aom_reader *r) {
  MB_MODE_INFO *const mbmi = &mi->mbmi;
  const BLOCK_SIZE bsize = mi->mbmi.sb_type;
  const int use_angle_delta = av1_use_angle_delta(bsize);

  mbmi->ref_frame[0] = INTRA_FRAME;
  mbmi->ref_frame[1] = NONE_FRAME;

  FRAME_CONTEXT *ec_ctx = xd->tile_ctx;

  mbmi->mode = read_intra_mode(r, ec_ctx->y_mode_cdf[size_group_lookup[bsize]]);

  mbmi->angle_delta[PLANE_TYPE_Y] =
      use_angle_delta && av1_is_directional_mode(mbmi->mode)
          ? read_angle_delta(r, ec_ctx->angle_delta_cdf[mbmi->mode - V_PRED])
          : 0;
#if CONFIG_MONO_VIDEO
  if (!cm->seq_params.monochrome &&
      is_chroma_reference(mi_row, mi_col, bsize, xd->plane[1].subsampling_x,
                          xd->plane[1].subsampling_y))
#else
  if (is_chroma_reference(mi_row, mi_col, bsize, xd->plane[1].subsampling_x,
                          xd->plane[1].subsampling_y))
#endif  // CONFIG_MONO_VIDEO
  {
    mbmi->uv_mode =
        read_intra_mode_uv(ec_ctx, r, is_cfl_allowed(mbmi), mbmi->mode);
    if (mbmi->uv_mode == UV_CFL_PRED) {
      mbmi->cfl_alpha_idx =
          read_cfl_alphas(xd->tile_ctx, r, &mbmi->cfl_alpha_signs);
      xd->cfl.store_y = 1;
    } else {
      xd->cfl.store_y = 0;
    }
    mbmi->angle_delta[PLANE_TYPE_UV] =
        use_angle_delta && av1_is_directional_mode(get_uv_mode(mbmi->uv_mode))
            ? read_angle_delta(r,
                               ec_ctx->angle_delta_cdf[mbmi->uv_mode - V_PRED])
            : 0;
  } else {
    // Avoid decoding angle_info if there is is no chroma prediction
    mbmi->uv_mode = UV_DC_PRED;
    xd->cfl.is_chroma_reference = 0;
    xd->cfl.store_y = 1;
  }

  mbmi->palette_mode_info.palette_size[0] = 0;
  mbmi->palette_mode_info.palette_size[1] = 0;
  if (av1_allow_palette(cm->allow_screen_content_tools, bsize))
    read_palette_mode_info(cm, xd, mi_row, mi_col, r);

  read_filter_intra_mode_info(cm, xd, r);
}

static INLINE int is_mv_valid(const MV *mv) {
  return mv->row > MV_LOW && mv->row < MV_UPP && mv->col > MV_LOW &&
         mv->col < MV_UPP;
}

static INLINE int assign_mv(AV1_COMMON *cm, MACROBLOCKD *xd,
                            PREDICTION_MODE mode,
                            MV_REFERENCE_FRAME ref_frame[2], int_mv mv[2],
                            int_mv ref_mv[2], int_mv nearest_mv[2],
                            int_mv near_mv[2], int mi_row, int mi_col,
                            int is_compound, int allow_hp, aom_reader *r) {
  int ret = 1;
  FRAME_CONTEXT *ec_ctx = xd->tile_ctx;
  BLOCK_SIZE bsize = xd->mi[0]->mbmi.sb_type;
  MB_MODE_INFO *mbmi = &xd->mi[0]->mbmi;
  int_mv *pred_mv = mbmi->pred_mv;
#if CONFIG_AMVR
  if (cm->cur_frame_force_integer_mv) {
    allow_hp = MV_SUBPEL_NONE;
  }
#endif
  switch (mode) {
    case NEWMV: {
      for (int i = 0; i < 1 + is_compound; ++i) {
        nmv_context *const nmvc = &ec_ctx->nmvc;
        read_mv(r, &mv[i].as_mv, &ref_mv[i].as_mv, nmvc, allow_hp);
        ret = ret && is_mv_valid(&mv[i].as_mv);

        pred_mv[i].as_int = ref_mv[i].as_int;
      }
      break;
    }
    case NEARESTMV: {
      mv[0].as_int = nearest_mv[0].as_int;
      if (is_compound) mv[1].as_int = nearest_mv[1].as_int;

      pred_mv[0].as_int = nearest_mv[0].as_int;
      if (is_compound) pred_mv[1].as_int = nearest_mv[1].as_int;
      break;
    }
    case NEARMV: {
      mv[0].as_int = near_mv[0].as_int;
      if (is_compound) mv[1].as_int = near_mv[1].as_int;

      pred_mv[0].as_int = near_mv[0].as_int;
      if (is_compound) pred_mv[1].as_int = near_mv[1].as_int;
      break;
    }
    case GLOBALMV: {
      mv[0].as_int = gm_get_motion_vector(&cm->global_motion[ref_frame[0]],
                                          cm->allow_high_precision_mv, bsize,
                                          mi_col, mi_row
#if CONFIG_AMVR
                                          ,
                                          cm->cur_frame_force_integer_mv
#endif
                                          )
                         .as_int;
      if (is_compound)
        mv[1].as_int = gm_get_motion_vector(&cm->global_motion[ref_frame[1]],
                                            cm->allow_high_precision_mv, bsize,
                                            mi_col, mi_row
#if CONFIG_AMVR
                                            ,
                                            cm->cur_frame_force_integer_mv
#endif
                                            )
                           .as_int;

      pred_mv[0].as_int = mv[0].as_int;
      if (is_compound) pred_mv[1].as_int = mv[1].as_int;
      break;
    }
    case NEW_NEWMV: {
      assert(is_compound);
      for (int i = 0; i < 2; ++i) {
        nmv_context *const nmvc = &ec_ctx->nmvc;
        read_mv(r, &mv[i].as_mv, &ref_mv[i].as_mv, nmvc, allow_hp);
        ret = ret && is_mv_valid(&mv[i].as_mv);
      }
      break;
    }
    case NEAREST_NEARESTMV: {
      assert(is_compound);
      mv[0].as_int = nearest_mv[0].as_int;
      mv[1].as_int = nearest_mv[1].as_int;
      break;
    }
    case NEAR_NEARMV: {
      assert(is_compound);
      mv[0].as_int = near_mv[0].as_int;
      mv[1].as_int = near_mv[1].as_int;
      break;
    }
    case NEW_NEARESTMV: {
      nmv_context *const nmvc = &ec_ctx->nmvc;
      read_mv(r, &mv[0].as_mv, &ref_mv[0].as_mv, nmvc, allow_hp);
      assert(is_compound);
      ret = ret && is_mv_valid(&mv[0].as_mv);
      mv[1].as_int = nearest_mv[1].as_int;
      break;
    }
    case NEAREST_NEWMV: {
      nmv_context *const nmvc = &ec_ctx->nmvc;
      mv[0].as_int = nearest_mv[0].as_int;
      read_mv(r, &mv[1].as_mv, &ref_mv[1].as_mv, nmvc, allow_hp);
      assert(is_compound);
      ret = ret && is_mv_valid(&mv[1].as_mv);
      break;
    }
    case NEAR_NEWMV: {
      nmv_context *const nmvc = &ec_ctx->nmvc;
      mv[0].as_int = near_mv[0].as_int;
      read_mv(r, &mv[1].as_mv, &ref_mv[1].as_mv, nmvc, allow_hp);
      assert(is_compound);

      ret = ret && is_mv_valid(&mv[1].as_mv);
      break;
    }
    case NEW_NEARMV: {
      nmv_context *const nmvc = &ec_ctx->nmvc;
      read_mv(r, &mv[0].as_mv, &ref_mv[0].as_mv, nmvc, allow_hp);
      assert(is_compound);
      ret = ret && is_mv_valid(&mv[0].as_mv);
      mv[1].as_int = near_mv[1].as_int;
      break;
    }
    case GLOBAL_GLOBALMV: {
      assert(is_compound);
      mv[0].as_int = gm_get_motion_vector(&cm->global_motion[ref_frame[0]],
                                          cm->allow_high_precision_mv, bsize,
                                          mi_col, mi_row
#if CONFIG_AMVR
                                          ,
                                          cm->cur_frame_force_integer_mv
#endif
                                          )
                         .as_int;
      mv[1].as_int = gm_get_motion_vector(&cm->global_motion[ref_frame[1]],
                                          cm->allow_high_precision_mv, bsize,
                                          mi_col, mi_row
#if CONFIG_AMVR
                                          ,
                                          cm->cur_frame_force_integer_mv
#endif
                                          )
                         .as_int;
      break;
    }
    default: { return 0; }
  }
  return ret;
}

static int read_is_inter_block(AV1_COMMON *const cm, MACROBLOCKD *const xd,
                               int segment_id, aom_reader *r) {
  if (segfeature_active(&cm->seg, segment_id, SEG_LVL_REF_FRAME)) {
    const int frame = get_segdata(&cm->seg, segment_id, SEG_LVL_REF_FRAME);
    RefBuffer *ref_buf = &cm->frame_refs[frame - LAST_FRAME];
    return frame != INTRA_FRAME && av1_is_valid_scale(&ref_buf->sf);
  }
  if (segfeature_active(&cm->seg, segment_id, SEG_LVL_SKIP)
#if CONFIG_SEGMENT_GLOBALMV
      || segfeature_active(&cm->seg, segment_id, SEG_LVL_GLOBALMV)
#endif
  ) {
    if (!av1_is_valid_scale(&cm->frame_refs[0].sf)) return 0;
  }
  const int ctx = av1_get_intra_inter_context(xd);
  FRAME_CONTEXT *ec_ctx = xd->tile_ctx;
  const int is_inter =
      aom_read_symbol(r, ec_ctx->intra_inter_cdf[ctx], 2, ACCT_STR);
  return is_inter;
}

static void fpm_sync(void *const data, int mi_row) {
  AV1Decoder *const pbi = (AV1Decoder *)data;
  av1_frameworker_wait(pbi->frame_worker_owner, pbi->common.prev_frame,
                       mi_row << pbi->common.seq_params.mib_size_log2);
}

#if DEC_MISMATCH_DEBUG
static void dec_dump_logs(AV1_COMMON *cm, MODE_INFO *const mi, int mi_row,
                          int mi_col, int16_t mode_ctx) {
  int_mv mv[2] = { { 0 } };
  MB_MODE_INFO *const mbmi = &mi->mbmi;
  for (int ref = 0; ref < 1 + has_second_ref(mbmi); ++ref)
    mv[ref].as_mv = mbmi->mv[ref].as_mv;

  const int16_t newmv_ctx = mode_ctx & NEWMV_CTX_MASK;
  int16_t zeromv_ctx = -1;
  int16_t refmv_ctx = -1;
  if (mbmi->mode != NEWMV) {
    zeromv_ctx = (mode_ctx >> GLOBALMV_OFFSET) & GLOBALMV_CTX_MASK;
    if (mbmi->mode != GLOBALMV)
      refmv_ctx = (mode_ctx >> REFMV_OFFSET) & REFMV_CTX_MASK;
  }

#define FRAME_TO_CHECK 11
  if (cm->current_video_frame == FRAME_TO_CHECK && cm->show_frame == 1) {
    printf(
        "=== DECODER ===: "
        "Frame=%d, (mi_row,mi_col)=(%d,%d), skip_mode=%d, mode=%d, bsize=%d, "
        "show_frame=%d, mv[0]=(%d,%d), mv[1]=(%d,%d), ref[0]=%d, "
        "ref[1]=%d, motion_mode=%d, mode_ctx=%d, "
        "newmv_ctx=%d, zeromv_ctx=%d, refmv_ctx=%d, tx_size=%d\n",
        cm->current_video_frame, mi_row, mi_col, mbmi->skip_mode, mbmi->mode,
        mbmi->sb_type, cm->show_frame, mv[0].as_mv.row, mv[0].as_mv.col,
        mv[1].as_mv.row, mv[1].as_mv.col, mbmi->ref_frame[0],
        mbmi->ref_frame[1], mbmi->motion_mode, mode_ctx, newmv_ctx, zeromv_ctx,
        refmv_ctx, mbmi->tx_size);
  }
}
#endif  // DEC_MISMATCH_DEBUG

static void read_inter_block_mode_info(AV1Decoder *const pbi,
                                       MACROBLOCKD *const xd,
                                       MODE_INFO *const mi, int mi_row,
                                       int mi_col, aom_reader *r) {
  AV1_COMMON *const cm = &pbi->common;
  MB_MODE_INFO *const mbmi = &mi->mbmi;
  const BLOCK_SIZE bsize = mbmi->sb_type;
  const int allow_hp = cm->allow_high_precision_mv;
  int_mv nearestmv[2], nearmv[2];
  int_mv ref_mvs[MODE_CTX_REF_FRAMES][MAX_MV_REF_CANDIDATES] = { { { 0 } } };
  int16_t inter_mode_ctx[MODE_CTX_REF_FRAMES];
  int pts[SAMPLES_ARRAY_SIZE], pts_inref[SAMPLES_ARRAY_SIZE];
  FRAME_CONTEXT *ec_ctx = xd->tile_ctx;

  mbmi->uv_mode = UV_DC_PRED;
  mbmi->palette_mode_info.palette_size[0] = 0;
  mbmi->palette_mode_info.palette_size[1] = 0;

  av1_collect_neighbors_ref_counts(xd);

  read_ref_frames(cm, xd, r, mbmi->segment_id, mbmi->ref_frame);
  const int is_compound = has_second_ref(mbmi);

  MV_REFERENCE_FRAME ref_frame = av1_ref_frame_type(mbmi->ref_frame);
  av1_find_mv_refs(cm, xd, mi, ref_frame, xd->ref_mv_count, xd->ref_mv_stack,
                   ref_mvs, mi_row, mi_col, inter_mode_ctx);

  int mode_ctx = av1_mode_context_analyzer(inter_mode_ctx, mbmi->ref_frame);
  mbmi->ref_mv_idx = 0;

  if (mbmi->skip_mode) {
    assert(is_compound);
    mbmi->mode = NEAREST_NEARESTMV;
  } else {
#if CONFIG_SEGMENT_GLOBALMV
    if (segfeature_active(&cm->seg, mbmi->segment_id, SEG_LVL_SKIP) ||
        segfeature_active(&cm->seg, mbmi->segment_id, SEG_LVL_GLOBALMV))
#else
    if (segfeature_active(&cm->seg, mbmi->segment_id, SEG_LVL_SKIP))
#endif
    {
      mbmi->mode = GLOBALMV;
    } else {
      if (is_compound)
        mbmi->mode = read_inter_compound_mode(xd, r, mode_ctx);
      else
        mbmi->mode = read_inter_mode(ec_ctx, r, mode_ctx);
      if (mbmi->mode == NEWMV || mbmi->mode == NEW_NEWMV ||
          have_nearmv_in_inter_mode(mbmi->mode))
        read_drl_idx(ec_ctx, xd, mbmi, r);
    }
  }

  if (is_compound != is_inter_compound_mode(mbmi->mode)) {
    aom_internal_error(&cm->error, AOM_CODEC_CORRUPT_FRAME,
                       "Prediction mode %d invalid with ref frame %d %d",
                       mbmi->mode, mbmi->ref_frame[0], mbmi->ref_frame[1]);
  }

  if (mbmi->mode != GLOBALMV && mbmi->mode != GLOBAL_GLOBALMV) {
    for (int ref = 0; ref < 1 + is_compound; ++ref) {
#if CONFIG_AMVR
      av1_find_best_ref_mvs(allow_hp, ref_mvs[mbmi->ref_frame[ref]],
                            &nearestmv[ref], &nearmv[ref],
                            cm->cur_frame_force_integer_mv);
#else
      av1_find_best_ref_mvs(allow_hp, ref_mvs[mbmi->ref_frame[ref]],
                            &nearestmv[ref], &nearmv[ref]);
#endif
    }
  }

  if (is_compound && mbmi->mode != GLOBAL_GLOBALMV) {
    int ref_mv_idx = mbmi->ref_mv_idx + 1;
    nearestmv[0] = xd->ref_mv_stack[ref_frame][0].this_mv;
    nearestmv[1] = xd->ref_mv_stack[ref_frame][0].comp_mv;
    nearmv[0] = xd->ref_mv_stack[ref_frame][ref_mv_idx].this_mv;
    nearmv[1] = xd->ref_mv_stack[ref_frame][ref_mv_idx].comp_mv;
#if CONFIG_AMVR
    lower_mv_precision(&nearestmv[0].as_mv, allow_hp,
                       cm->cur_frame_force_integer_mv);
    lower_mv_precision(&nearestmv[1].as_mv, allow_hp,
                       cm->cur_frame_force_integer_mv);
    lower_mv_precision(&nearmv[0].as_mv, allow_hp,
                       cm->cur_frame_force_integer_mv);
    lower_mv_precision(&nearmv[1].as_mv, allow_hp,
                       cm->cur_frame_force_integer_mv);
#else
    lower_mv_precision(&nearestmv[0].as_mv, allow_hp);
    lower_mv_precision(&nearestmv[1].as_mv, allow_hp);
    lower_mv_precision(&nearmv[0].as_mv, allow_hp);
    lower_mv_precision(&nearmv[1].as_mv, allow_hp);
#endif
  } else if (mbmi->ref_mv_idx > 0 && mbmi->mode == NEARMV) {
    int_mv cur_mv =
        xd->ref_mv_stack[mbmi->ref_frame[0]][1 + mbmi->ref_mv_idx].this_mv;
    nearmv[0] = cur_mv;
  }

  int_mv ref_mv[2];
  ref_mv[0] = nearestmv[0];
  ref_mv[1] = nearestmv[1];

  if (is_compound) {
    int ref_mv_idx = mbmi->ref_mv_idx;
    // Special case: NEAR_NEWMV and NEW_NEARMV modes use
    // 1 + mbmi->ref_mv_idx (like NEARMV) instead of
    // mbmi->ref_mv_idx (like NEWMV)
    if (mbmi->mode == NEAR_NEWMV || mbmi->mode == NEW_NEARMV)
      ref_mv_idx = 1 + mbmi->ref_mv_idx;

    if (compound_ref0_mode(mbmi->mode) == NEWMV)
      ref_mv[0] = xd->ref_mv_stack[ref_frame][ref_mv_idx].this_mv;

    if (compound_ref1_mode(mbmi->mode) == NEWMV)
      ref_mv[1] = xd->ref_mv_stack[ref_frame][ref_mv_idx].comp_mv;
  } else {
    if (mbmi->mode == NEWMV) {
      if (xd->ref_mv_count[ref_frame] > 1)
        ref_mv[0] = xd->ref_mv_stack[ref_frame][mbmi->ref_mv_idx].this_mv;
    }
  }

  if (mbmi->skip_mode) {
    assert(mbmi->mode == NEAREST_NEARESTMV);
    mbmi->mv[0].as_int = nearestmv[0].as_int;
    mbmi->mv[1].as_int = nearestmv[1].as_int;
  } else {
    int mv_corrupted_flag =
        !assign_mv(cm, xd, mbmi->mode, mbmi->ref_frame, mbmi->mv, ref_mv,
                   nearestmv, nearmv, mi_row, mi_col, is_compound, allow_hp, r);
    aom_merge_corrupted_flag(&xd->corrupted, mv_corrupted_flag);
  }

  mbmi->use_wedge_interintra = 0;
  if (cm->allow_interintra_compound && !mbmi->skip_mode &&
      is_interintra_allowed(mbmi)) {
    const int bsize_group = size_group_lookup[bsize];
    const int interintra =
        aom_read_symbol(r, ec_ctx->interintra_cdf[bsize_group], 2, ACCT_STR);
    assert(mbmi->ref_frame[1] == NONE_FRAME);
    if (interintra) {
      const INTERINTRA_MODE interintra_mode =
          read_interintra_mode(xd, r, bsize_group);
      mbmi->ref_frame[1] = INTRA_FRAME;
      mbmi->interintra_mode = interintra_mode;
      mbmi->angle_delta[PLANE_TYPE_Y] = 0;
      mbmi->angle_delta[PLANE_TYPE_UV] = 0;
      mbmi->filter_intra_mode_info.use_filter_intra = 0;
      if (is_interintra_wedge_used(bsize)) {
        mbmi->use_wedge_interintra = aom_read_symbol(
            r, ec_ctx->wedge_interintra_cdf[bsize], 2, ACCT_STR);
        if (mbmi->use_wedge_interintra) {
#if II_WEDGE_IDX_ENTROPY_CODING
          mbmi->interintra_wedge_index =
              aom_read_symbol(r, ec_ctx->wedge_idx_cdf[bsize], 16, ACCT_STR);
#else
          mbmi->interintra_wedge_index =
              aom_read_literal(r, get_wedge_bits_lookup(bsize), ACCT_STR);
#endif
          mbmi->interintra_wedge_sign = 0;
        }
      }
    }
  }

  for (int ref = 0; ref < 1 + has_second_ref(mbmi); ++ref) {
    const MV_REFERENCE_FRAME frame = mbmi->ref_frame[ref];
    RefBuffer *ref_buf = &cm->frame_refs[frame - LAST_FRAME];

    xd->block_refs[ref] = ref_buf;
  }

  mbmi->motion_mode = SIMPLE_TRANSLATION;
  if (mbmi->sb_type >= BLOCK_8X8 && !mbmi->skip_mode && !has_second_ref(mbmi))
    mbmi->num_proj_ref[0] = findSamples(cm, xd, mi_row, mi_col, pts, pts_inref);
  av1_count_overlappable_neighbors(cm, xd, mi_row, mi_col);

  if (mbmi->ref_frame[1] != INTRA_FRAME)
    mbmi->motion_mode = read_motion_mode(cm, xd, mi, r);

  // init
  mbmi->comp_group_idx = 0;
  mbmi->compound_idx = 1;
  mbmi->interinter_compound_type = COMPOUND_AVERAGE;

  if (has_second_ref(mbmi) && !mbmi->skip_mode) {
    // Read idx to indicate current compound inter prediction mode group
    const int masked_compound_used =
        is_any_masked_compound_used(bsize) && cm->allow_masked_compound;

    if (masked_compound_used) {
      const int ctx_comp_group_idx = get_comp_group_idx_context(xd);
      mbmi->comp_group_idx = aom_read_symbol(
          r, ec_ctx->comp_group_idx_cdf[ctx_comp_group_idx], 2, ACCT_STR);
    }

    if (mbmi->comp_group_idx == 0) {
      if (cm->seq_params.enable_jnt_comp && !cm->error_resilient_mode) {
        const int comp_index_ctx = get_comp_index_context(cm, xd);
        mbmi->compound_idx = aom_read_symbol(
            r, ec_ctx->compound_index_cdf[comp_index_ctx], 2, ACCT_STR);
      } else {
        // Distance-weighted compound is disabled, so always use average
        mbmi->compound_idx = 1;
      }
    } else {
      assert(cm->reference_mode != SINGLE_REFERENCE &&
             is_inter_compound_mode(mbmi->mode) &&
             mbmi->motion_mode == SIMPLE_TRANSLATION);
      assert(masked_compound_used);

      // compound_segment, wedge
      if (is_interinter_compound_used(COMPOUND_WEDGE, bsize))
        mbmi->interinter_compound_type =
            1 + aom_read_symbol(r, ec_ctx->compound_type_cdf[bsize],
                                COMPOUND_TYPES - 1, ACCT_STR);
      else
        mbmi->interinter_compound_type = COMPOUND_SEG;

      if (mbmi->interinter_compound_type == COMPOUND_WEDGE) {
        assert(is_interinter_compound_used(COMPOUND_WEDGE, bsize));
#if WEDGE_IDX_ENTROPY_CODING
        mbmi->wedge_index =
            aom_read_symbol(r, ec_ctx->wedge_idx_cdf[bsize], 16, ACCT_STR);
#else
        mbmi->wedge_index =
            aom_read_literal(r, get_wedge_bits_lookup(bsize), ACCT_STR);
#endif
        mbmi->wedge_sign = aom_read_bit(r, ACCT_STR);
      } else {
        assert(mbmi->interinter_compound_type == COMPOUND_SEG);
        mbmi->mask_type = aom_read_literal(r, MAX_SEG_MASK_BITS, ACCT_STR);
      }
    }
  }

  read_mb_interp_filter(cm, xd, mbmi, r);

  if (mbmi->motion_mode == WARPED_CAUSAL) {
    mbmi->wm_params[0].wmtype = DEFAULT_WMTYPE;

#if CONFIG_EXT_WARPED_MOTION
    if (mbmi->num_proj_ref[0] > 1)
      mbmi->num_proj_ref[0] = selectSamples(&mbmi->mv[0].as_mv, pts, pts_inref,
                                            mbmi->num_proj_ref[0], bsize);
#endif  // CONFIG_EXT_WARPED_MOTION

    if (find_projection(mbmi->num_proj_ref[0], pts, pts_inref, bsize,
                        mbmi->mv[0].as_mv.row, mbmi->mv[0].as_mv.col,
                        &mbmi->wm_params[0], mi_row, mi_col)) {
#if WARPED_MOTION_DEBUG
      printf("Warning: unexpected warped model from aomenc\n");
#endif
      mbmi->wm_params[0].invalid = 1;
    }
  }

#if DEC_MISMATCH_DEBUG
  dec_dump_logs(cm, mi, mi_row, mi_col, mode_ctx);
#endif  // DEC_MISMATCH_DEBUG
}

static void read_inter_frame_mode_info(AV1Decoder *const pbi,
                                       MACROBLOCKD *const xd, int mi_row,
                                       int mi_col, aom_reader *r) {
  AV1_COMMON *const cm = &pbi->common;
  MODE_INFO *const mi = xd->mi[0];
  MB_MODE_INFO *const mbmi = &mi->mbmi;
  int inter_block = 1;

  mbmi->mv[0].as_int = 0;
  mbmi->mv[1].as_int = 0;
  mbmi->segment_id = read_inter_segment_id(cm, xd, mi_row, mi_col, 1, r);

  mbmi->skip_mode = read_skip_mode(cm, xd, mbmi->segment_id, r);

  if (mbmi->skip_mode)
    mbmi->skip = 1;
  else
    mbmi->skip = read_skip(cm, xd, mbmi->segment_id, r);

#if CONFIG_SPATIAL_SEGMENTATION
  mbmi->segment_id = read_inter_segment_id(cm, xd, mi_row, mi_col, 0, r);
#endif

  read_cdef(cm, r, mbmi, mi_col, mi_row);

  if (cm->delta_q_present_flag) {
    xd->current_qindex =
        xd->prev_qindex +
        read_delta_qindex(cm, xd, r, mbmi, mi_col, mi_row) * cm->delta_q_res;
    /* Normative: Clamp to [1,MAXQ] to not interfere with lossless mode */
    xd->current_qindex = clamp(xd->current_qindex, 1, MAXQ);
    xd->prev_qindex = xd->current_qindex;
#if CONFIG_EXT_DELTA_Q
    if (cm->delta_lf_present_flag) {
      if (cm->delta_lf_multi) {
        const int frame_lf_count =
            av1_num_planes(cm) > 1 ? FRAME_LF_COUNT : FRAME_LF_COUNT - 2;
        for (int lf_id = 0; lf_id < frame_lf_count; ++lf_id) {
          const int tmp_lvl =
              xd->prev_delta_lf[lf_id] +
              read_delta_lflevel(cm, xd, r, lf_id, mbmi, mi_col, mi_row) *
                  cm->delta_lf_res;
          mbmi->curr_delta_lf[lf_id] = xd->curr_delta_lf[lf_id] =
              clamp(tmp_lvl, -MAX_LOOP_FILTER, MAX_LOOP_FILTER);
          xd->prev_delta_lf[lf_id] = xd->curr_delta_lf[lf_id];
        }
      } else {
        const int tmp_lvl =
            xd->prev_delta_lf_from_base +
            read_delta_lflevel(cm, xd, r, -1, mbmi, mi_col, mi_row) *
                cm->delta_lf_res;
        mbmi->current_delta_lf_from_base = xd->current_delta_lf_from_base =
            clamp(tmp_lvl, -MAX_LOOP_FILTER, MAX_LOOP_FILTER);
        xd->prev_delta_lf_from_base = xd->current_delta_lf_from_base;
      }
    }
#endif  // CONFIG_EXT_DELTA_Q
  }

  if (!mbmi->skip_mode)
    inter_block = read_is_inter_block(cm, xd, mbmi->segment_id, r);

  mbmi->current_q_index = xd->current_qindex;

  xd->above_txfm_context =
      cm->above_txfm_context + (mi_col << TX_UNIT_WIDE_LOG2);
  xd->left_txfm_context = xd->left_txfm_context_buffer +
                          ((mi_row & MAX_MIB_MASK) << TX_UNIT_HIGH_LOG2);

  if (inter_block)
    read_inter_block_mode_info(pbi, xd, mi, mi_row, mi_col, r);
  else
    read_intra_block_mode_info(cm, mi_row, mi_col, xd, mi, r);
}

static void intra_copy_frame_mvs(AV1_COMMON *const cm, int mi_row, int mi_col,
                                 int x_mis, int y_mis) {
  const int frame_mvs_stride = ROUND_POWER_OF_TWO(cm->mi_cols, 1);
  MV_REF *frame_mvs =
      cm->cur_frame->mvs + (mi_row >> 1) * frame_mvs_stride + (mi_col >> 1);
  x_mis = ROUND_POWER_OF_TWO(x_mis, 1);
  y_mis = ROUND_POWER_OF_TWO(y_mis, 1);

  for (int h = 0; h < y_mis; h++) {
    MV_REF *mv = frame_mvs;
    for (int w = 0; w < x_mis; w++) {
      mv->ref_frame[0] = NONE_FRAME;
      mv->ref_frame[1] = NONE_FRAME;
      mv++;
    }
    frame_mvs += frame_mvs_stride;
  }
}

void av1_read_mode_info(AV1Decoder *const pbi, MACROBLOCKD *xd, int mi_row,
                        int mi_col, aom_reader *r, int x_mis, int y_mis) {
  AV1_COMMON *const cm = &pbi->common;
  MODE_INFO *const mi = xd->mi[0];
  mi->mbmi.use_intrabc = 0;

  if (frame_is_intra_only(cm)) {
    read_intra_frame_mode_info(cm, xd, mi_row, mi_col, r);
    intra_copy_frame_mvs(cm, mi_row, mi_col, x_mis, y_mis);
  } else {
    read_inter_frame_mode_info(pbi, xd, mi_row, mi_col, r);
    av1_copy_frame_mvs(cm, mi, mi_row, mi_col, x_mis, y_mis);
  }
}
