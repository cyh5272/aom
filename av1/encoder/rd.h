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

#ifndef AOM_AV1_ENCODER_RD_H_
#define AOM_AV1_ENCODER_RD_H_

#include <limits.h>

#include "av1/common/blockd.h"

#include "av1/encoder/block.h"
#include "av1/encoder/context_tree.h"
#include "av1/encoder/cost.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RDDIV_BITS 7
#define RD_EPB_SHIFT 6

#define RDCOST(RM, R, D)                                            \
  (ROUND_POWER_OF_TWO(((int64_t)(R)) * (RM), AV1_PROB_COST_SHIFT) + \
   ((D) * (1 << RDDIV_BITS)))

#define RDCOST_NEG_R(RM, R, D) \
  (((D) * (1 << RDDIV_BITS)) - \
   ROUND_POWER_OF_TWO(((int64_t)(R)) * (RM), AV1_PROB_COST_SHIFT))

#define RDCOST_DBL(RM, R, D)                                       \
  (((((double)(R)) * (RM)) / (double)(1 << AV1_PROB_COST_SHIFT)) + \
   ((double)(D) * (1 << RDDIV_BITS)))

#define QIDX_SKIP_THRESH 115

#define MV_COST_WEIGHT 108
#define MV_COST_WEIGHT_SUB 120

#define RD_THRESH_MAX_FACT 64
#define RD_THRESH_INC 1

// Factor to weigh the rate for switchable interp filters.
#define SWITCHABLE_INTERP_RATE_FACTOR 1

enum {
  THR_LAST,
  THR_LAST2,
  THR_LAST3,
  THR_BWDR,
  THR_ALTR2,
  THR_GOLD,
  THR_ALTR,

  THR_COMP_LA,
  THR_COMP_L2A,
  THR_COMP_L3A,
  THR_COMP_GA,

  THR_COMP_LB,
  THR_COMP_L2B,
  THR_COMP_L3B,
  THR_COMP_GB,

  THR_COMP_LA2,
  THR_COMP_L2A2,
  THR_COMP_L3A2,
  THR_COMP_GA2,

  THR_INTRA,

  MAX_REFS
} UENUM1BYTE(THR_MODES_SUB8X8);

typedef struct {
  PREDICTION_MODE mode;
  MV_REFERENCE_FRAME ref_frame[2];
} MODE_DEFINITION;

static const MODE_DEFINITION av1_mode_order[MAX_MODES] = {
  { NEARESTMV, { LAST_FRAME, NONE_FRAME } },
  { NEARESTMV, { LAST2_FRAME, NONE_FRAME } },
  { NEARESTMV, { LAST3_FRAME, NONE_FRAME } },
  { NEARESTMV, { BWDREF_FRAME, NONE_FRAME } },
  { NEARESTMV, { ALTREF2_FRAME, NONE_FRAME } },
  { NEARESTMV, { ALTREF_FRAME, NONE_FRAME } },
  { NEARESTMV, { GOLDEN_FRAME, NONE_FRAME } },

  { NEWMV, { LAST_FRAME, NONE_FRAME } },
  { NEWMV, { LAST2_FRAME, NONE_FRAME } },
  { NEWMV, { LAST3_FRAME, NONE_FRAME } },
  { NEWMV, { BWDREF_FRAME, NONE_FRAME } },
  { NEWMV, { ALTREF2_FRAME, NONE_FRAME } },
  { NEWMV, { ALTREF_FRAME, NONE_FRAME } },
  { NEWMV, { GOLDEN_FRAME, NONE_FRAME } },

  { NEARMV, { LAST_FRAME, NONE_FRAME } },
  { NEARMV, { LAST2_FRAME, NONE_FRAME } },
  { NEARMV, { LAST3_FRAME, NONE_FRAME } },
  { NEARMV, { BWDREF_FRAME, NONE_FRAME } },
  { NEARMV, { ALTREF2_FRAME, NONE_FRAME } },
  { NEARMV, { ALTREF_FRAME, NONE_FRAME } },
  { NEARMV, { GOLDEN_FRAME, NONE_FRAME } },

  { GLOBALMV, { LAST_FRAME, NONE_FRAME } },
  { GLOBALMV, { LAST2_FRAME, NONE_FRAME } },
  { GLOBALMV, { LAST3_FRAME, NONE_FRAME } },
  { GLOBALMV, { BWDREF_FRAME, NONE_FRAME } },
  { GLOBALMV, { ALTREF2_FRAME, NONE_FRAME } },
  { GLOBALMV, { GOLDEN_FRAME, NONE_FRAME } },
  { GLOBALMV, { ALTREF_FRAME, NONE_FRAME } },

  // TODO(zoeliu): May need to reconsider the order on the modes to check

  { NEAREST_NEARESTMV, { LAST_FRAME, ALTREF_FRAME } },
  { NEAREST_NEARESTMV, { LAST2_FRAME, ALTREF_FRAME } },
  { NEAREST_NEARESTMV, { LAST3_FRAME, ALTREF_FRAME } },
  { NEAREST_NEARESTMV, { GOLDEN_FRAME, ALTREF_FRAME } },
  { NEAREST_NEARESTMV, { LAST_FRAME, BWDREF_FRAME } },
  { NEAREST_NEARESTMV, { LAST2_FRAME, BWDREF_FRAME } },
  { NEAREST_NEARESTMV, { LAST3_FRAME, BWDREF_FRAME } },
  { NEAREST_NEARESTMV, { GOLDEN_FRAME, BWDREF_FRAME } },
  { NEAREST_NEARESTMV, { LAST_FRAME, ALTREF2_FRAME } },
  { NEAREST_NEARESTMV, { LAST2_FRAME, ALTREF2_FRAME } },
  { NEAREST_NEARESTMV, { LAST3_FRAME, ALTREF2_FRAME } },
  { NEAREST_NEARESTMV, { GOLDEN_FRAME, ALTREF2_FRAME } },

  { NEAREST_NEARESTMV, { LAST_FRAME, LAST2_FRAME } },
  { NEAREST_NEARESTMV, { LAST_FRAME, LAST3_FRAME } },
  { NEAREST_NEARESTMV, { LAST_FRAME, GOLDEN_FRAME } },
  { NEAREST_NEARESTMV, { BWDREF_FRAME, ALTREF_FRAME } },

  { NEAR_NEARMV, { LAST_FRAME, ALTREF_FRAME } },
  { NEW_NEARESTMV, { LAST_FRAME, ALTREF_FRAME } },
  { NEAREST_NEWMV, { LAST_FRAME, ALTREF_FRAME } },
  { NEW_NEARMV, { LAST_FRAME, ALTREF_FRAME } },
  { NEAR_NEWMV, { LAST_FRAME, ALTREF_FRAME } },
  { NEW_NEWMV, { LAST_FRAME, ALTREF_FRAME } },
  { GLOBAL_GLOBALMV, { LAST_FRAME, ALTREF_FRAME } },

  { NEAR_NEARMV, { LAST2_FRAME, ALTREF_FRAME } },
  { NEW_NEARESTMV, { LAST2_FRAME, ALTREF_FRAME } },
  { NEAREST_NEWMV, { LAST2_FRAME, ALTREF_FRAME } },
  { NEW_NEARMV, { LAST2_FRAME, ALTREF_FRAME } },
  { NEAR_NEWMV, { LAST2_FRAME, ALTREF_FRAME } },
  { NEW_NEWMV, { LAST2_FRAME, ALTREF_FRAME } },
  { GLOBAL_GLOBALMV, { LAST2_FRAME, ALTREF_FRAME } },

  { NEAR_NEARMV, { LAST3_FRAME, ALTREF_FRAME } },
  { NEW_NEARESTMV, { LAST3_FRAME, ALTREF_FRAME } },
  { NEAREST_NEWMV, { LAST3_FRAME, ALTREF_FRAME } },
  { NEW_NEARMV, { LAST3_FRAME, ALTREF_FRAME } },
  { NEAR_NEWMV, { LAST3_FRAME, ALTREF_FRAME } },
  { NEW_NEWMV, { LAST3_FRAME, ALTREF_FRAME } },
  { GLOBAL_GLOBALMV, { LAST3_FRAME, ALTREF_FRAME } },

  { NEAR_NEARMV, { GOLDEN_FRAME, ALTREF_FRAME } },
  { NEW_NEARESTMV, { GOLDEN_FRAME, ALTREF_FRAME } },
  { NEAREST_NEWMV, { GOLDEN_FRAME, ALTREF_FRAME } },
  { NEW_NEARMV, { GOLDEN_FRAME, ALTREF_FRAME } },
  { NEAR_NEWMV, { GOLDEN_FRAME, ALTREF_FRAME } },
  { NEW_NEWMV, { GOLDEN_FRAME, ALTREF_FRAME } },
  { GLOBAL_GLOBALMV, { GOLDEN_FRAME, ALTREF_FRAME } },

  { NEAR_NEARMV, { LAST_FRAME, BWDREF_FRAME } },
  { NEW_NEARESTMV, { LAST_FRAME, BWDREF_FRAME } },
  { NEAREST_NEWMV, { LAST_FRAME, BWDREF_FRAME } },
  { NEW_NEARMV, { LAST_FRAME, BWDREF_FRAME } },
  { NEAR_NEWMV, { LAST_FRAME, BWDREF_FRAME } },
  { NEW_NEWMV, { LAST_FRAME, BWDREF_FRAME } },
  { GLOBAL_GLOBALMV, { LAST_FRAME, BWDREF_FRAME } },

  { NEAR_NEARMV, { LAST2_FRAME, BWDREF_FRAME } },
  { NEW_NEARESTMV, { LAST2_FRAME, BWDREF_FRAME } },
  { NEAREST_NEWMV, { LAST2_FRAME, BWDREF_FRAME } },
  { NEW_NEARMV, { LAST2_FRAME, BWDREF_FRAME } },
  { NEAR_NEWMV, { LAST2_FRAME, BWDREF_FRAME } },
  { NEW_NEWMV, { LAST2_FRAME, BWDREF_FRAME } },
  { GLOBAL_GLOBALMV, { LAST2_FRAME, BWDREF_FRAME } },

  { NEAR_NEARMV, { LAST3_FRAME, BWDREF_FRAME } },
  { NEW_NEARESTMV, { LAST3_FRAME, BWDREF_FRAME } },
  { NEAREST_NEWMV, { LAST3_FRAME, BWDREF_FRAME } },
  { NEW_NEARMV, { LAST3_FRAME, BWDREF_FRAME } },
  { NEAR_NEWMV, { LAST3_FRAME, BWDREF_FRAME } },
  { NEW_NEWMV, { LAST3_FRAME, BWDREF_FRAME } },
  { GLOBAL_GLOBALMV, { LAST3_FRAME, BWDREF_FRAME } },

  { NEAR_NEARMV, { GOLDEN_FRAME, BWDREF_FRAME } },
  { NEW_NEARESTMV, { GOLDEN_FRAME, BWDREF_FRAME } },
  { NEAREST_NEWMV, { GOLDEN_FRAME, BWDREF_FRAME } },
  { NEW_NEARMV, { GOLDEN_FRAME, BWDREF_FRAME } },
  { NEAR_NEWMV, { GOLDEN_FRAME, BWDREF_FRAME } },
  { NEW_NEWMV, { GOLDEN_FRAME, BWDREF_FRAME } },
  { GLOBAL_GLOBALMV, { GOLDEN_FRAME, BWDREF_FRAME } },

  { NEAR_NEARMV, { LAST_FRAME, ALTREF2_FRAME } },
  { NEW_NEARESTMV, { LAST_FRAME, ALTREF2_FRAME } },
  { NEAREST_NEWMV, { LAST_FRAME, ALTREF2_FRAME } },
  { NEW_NEARMV, { LAST_FRAME, ALTREF2_FRAME } },
  { NEAR_NEWMV, { LAST_FRAME, ALTREF2_FRAME } },
  { NEW_NEWMV, { LAST_FRAME, ALTREF2_FRAME } },
  { GLOBAL_GLOBALMV, { LAST_FRAME, ALTREF2_FRAME } },

  { NEAR_NEARMV, { LAST2_FRAME, ALTREF2_FRAME } },
  { NEW_NEARESTMV, { LAST2_FRAME, ALTREF2_FRAME } },
  { NEAREST_NEWMV, { LAST2_FRAME, ALTREF2_FRAME } },
  { NEW_NEARMV, { LAST2_FRAME, ALTREF2_FRAME } },
  { NEAR_NEWMV, { LAST2_FRAME, ALTREF2_FRAME } },
  { NEW_NEWMV, { LAST2_FRAME, ALTREF2_FRAME } },
  { GLOBAL_GLOBALMV, { LAST2_FRAME, ALTREF2_FRAME } },

  { NEAR_NEARMV, { LAST3_FRAME, ALTREF2_FRAME } },
  { NEW_NEARESTMV, { LAST3_FRAME, ALTREF2_FRAME } },
  { NEAREST_NEWMV, { LAST3_FRAME, ALTREF2_FRAME } },
  { NEW_NEARMV, { LAST3_FRAME, ALTREF2_FRAME } },
  { NEAR_NEWMV, { LAST3_FRAME, ALTREF2_FRAME } },
  { NEW_NEWMV, { LAST3_FRAME, ALTREF2_FRAME } },
  { GLOBAL_GLOBALMV, { LAST3_FRAME, ALTREF2_FRAME } },

  { NEAR_NEARMV, { GOLDEN_FRAME, ALTREF2_FRAME } },
  { NEW_NEARESTMV, { GOLDEN_FRAME, ALTREF2_FRAME } },
  { NEAREST_NEWMV, { GOLDEN_FRAME, ALTREF2_FRAME } },
  { NEW_NEARMV, { GOLDEN_FRAME, ALTREF2_FRAME } },
  { NEAR_NEWMV, { GOLDEN_FRAME, ALTREF2_FRAME } },
  { NEW_NEWMV, { GOLDEN_FRAME, ALTREF2_FRAME } },
  { GLOBAL_GLOBALMV, { GOLDEN_FRAME, ALTREF2_FRAME } },

  { NEAR_NEARMV, { LAST_FRAME, LAST2_FRAME } },
  { NEW_NEARESTMV, { LAST_FRAME, LAST2_FRAME } },
  { NEAREST_NEWMV, { LAST_FRAME, LAST2_FRAME } },
  { NEW_NEARMV, { LAST_FRAME, LAST2_FRAME } },
  { NEAR_NEWMV, { LAST_FRAME, LAST2_FRAME } },
  { NEW_NEWMV, { LAST_FRAME, LAST2_FRAME } },
  { GLOBAL_GLOBALMV, { LAST_FRAME, LAST2_FRAME } },

  { NEAR_NEARMV, { LAST_FRAME, LAST3_FRAME } },
  { NEW_NEARESTMV, { LAST_FRAME, LAST3_FRAME } },
  { NEAREST_NEWMV, { LAST_FRAME, LAST3_FRAME } },
  { NEW_NEARMV, { LAST_FRAME, LAST3_FRAME } },
  { NEAR_NEWMV, { LAST_FRAME, LAST3_FRAME } },
  { NEW_NEWMV, { LAST_FRAME, LAST3_FRAME } },
  { GLOBAL_GLOBALMV, { LAST_FRAME, LAST3_FRAME } },

  { NEAR_NEARMV, { LAST_FRAME, GOLDEN_FRAME } },
  { NEW_NEARESTMV, { LAST_FRAME, GOLDEN_FRAME } },
  { NEAREST_NEWMV, { LAST_FRAME, GOLDEN_FRAME } },
  { NEW_NEARMV, { LAST_FRAME, GOLDEN_FRAME } },
  { NEAR_NEWMV, { LAST_FRAME, GOLDEN_FRAME } },
  { NEW_NEWMV, { LAST_FRAME, GOLDEN_FRAME } },
  { GLOBAL_GLOBALMV, { LAST_FRAME, GOLDEN_FRAME } },

  { NEAR_NEARMV, { BWDREF_FRAME, ALTREF_FRAME } },
  { NEW_NEARESTMV, { BWDREF_FRAME, ALTREF_FRAME } },
  { NEAREST_NEWMV, { BWDREF_FRAME, ALTREF_FRAME } },
  { NEW_NEARMV, { BWDREF_FRAME, ALTREF_FRAME } },
  { NEAR_NEWMV, { BWDREF_FRAME, ALTREF_FRAME } },
  { NEW_NEWMV, { BWDREF_FRAME, ALTREF_FRAME } },
  { GLOBAL_GLOBALMV, { BWDREF_FRAME, ALTREF_FRAME } },

  // intra modes
  { DC_PRED, { INTRA_FRAME, NONE_FRAME } },
  { PAETH_PRED, { INTRA_FRAME, NONE_FRAME } },
  { SMOOTH_PRED, { INTRA_FRAME, NONE_FRAME } },
  { SMOOTH_V_PRED, { INTRA_FRAME, NONE_FRAME } },
  { SMOOTH_H_PRED, { INTRA_FRAME, NONE_FRAME } },
  { H_PRED, { INTRA_FRAME, NONE_FRAME } },
  { V_PRED, { INTRA_FRAME, NONE_FRAME } },
  { D135_PRED, { INTRA_FRAME, NONE_FRAME } },
  { D203_PRED, { INTRA_FRAME, NONE_FRAME } },
  { D157_PRED, { INTRA_FRAME, NONE_FRAME } },
  { D67_PRED, { INTRA_FRAME, NONE_FRAME } },
  { D113_PRED, { INTRA_FRAME, NONE_FRAME } },
  { D45_PRED, { INTRA_FRAME, NONE_FRAME } },
};

typedef struct RD_OPT {
  // Thresh_mult is used to set a threshold for the rd score. A higher value
  // means that we will accept the best mode so far more often. This number
  // is used in combination with the current block size, and thresh_freq_fact
  // to pick a threshold.
  int thresh_mult[MAX_MODES];

  int threshes[MAX_SEGMENTS][BLOCK_SIZES_ALL][MAX_MODES];

  int64_t prediction_type_threshes[REF_FRAMES][REFERENCE_MODES];

  int RDMULT;

  double r0, arf_r0;
  double mc_saved_base, mc_count_base;
} RD_OPT;

static INLINE void av1_init_rd_stats(RD_STATS *rd_stats) {
#if CONFIG_RD_DEBUG
  int plane;
#endif
  rd_stats->rate = 0;
  rd_stats->dist = 0;
  rd_stats->rdcost = 0;
  rd_stats->sse = 0;
  rd_stats->skip = 1;
  rd_stats->zero_rate = 0;
#if CONFIG_RD_DEBUG
  // This may run into problems when monochrome video is
  // encoded, as there will only be 1 plane
  for (plane = 0; plane < MAX_MB_PLANE; ++plane) {
    rd_stats->txb_coeff_cost[plane] = 0;
    {
      int r, c;
      for (r = 0; r < TXB_COEFF_COST_MAP_SIZE; ++r)
        for (c = 0; c < TXB_COEFF_COST_MAP_SIZE; ++c)
          rd_stats->txb_coeff_cost_map[plane][r][c] = 0;
    }
  }
#endif
}

static INLINE void av1_invalid_rd_stats(RD_STATS *rd_stats) {
#if CONFIG_RD_DEBUG
  int plane;
#endif
  rd_stats->rate = INT_MAX;
  rd_stats->dist = INT64_MAX;
  rd_stats->rdcost = INT64_MAX;
  rd_stats->sse = INT64_MAX;
  rd_stats->skip = 0;
  rd_stats->zero_rate = 0;
#if CONFIG_RD_DEBUG
  // This may run into problems when monochrome video is
  // encoded, as there will only be 1 plane
  for (plane = 0; plane < MAX_MB_PLANE; ++plane) {
    rd_stats->txb_coeff_cost[plane] = INT_MAX;
    {
      int r, c;
      for (r = 0; r < TXB_COEFF_COST_MAP_SIZE; ++r)
        for (c = 0; c < TXB_COEFF_COST_MAP_SIZE; ++c)
          rd_stats->txb_coeff_cost_map[plane][r][c] = INT_MAX;
    }
  }
#endif
}

static INLINE void av1_merge_rd_stats(RD_STATS *rd_stats_dst,
                                      const RD_STATS *rd_stats_src) {
  assert(rd_stats_dst->rate != INT_MAX && rd_stats_src->rate != INT_MAX);
  rd_stats_dst->rate = (int)AOMMIN(
      ((int64_t)rd_stats_dst->rate + (int64_t)rd_stats_src->rate), INT_MAX);
  if (!rd_stats_dst->zero_rate)
    rd_stats_dst->zero_rate = rd_stats_src->zero_rate;
  rd_stats_dst->dist += rd_stats_src->dist;
  rd_stats_dst->sse += rd_stats_src->sse;
  rd_stats_dst->skip &= rd_stats_src->skip;
#if CONFIG_RD_DEBUG
  // This may run into problems when monochrome video is
  // encoded, as there will only be 1 plane
  for (int plane = 0; plane < MAX_MB_PLANE; ++plane) {
    rd_stats_dst->txb_coeff_cost[plane] += rd_stats_src->txb_coeff_cost[plane];
    {
      // TODO(angiebird): optimize this part
      int r, c;
      int ref_txb_coeff_cost = 0;
      for (r = 0; r < TXB_COEFF_COST_MAP_SIZE; ++r)
        for (c = 0; c < TXB_COEFF_COST_MAP_SIZE; ++c) {
          rd_stats_dst->txb_coeff_cost_map[plane][r][c] +=
              rd_stats_src->txb_coeff_cost_map[plane][r][c];
          ref_txb_coeff_cost += rd_stats_dst->txb_coeff_cost_map[plane][r][c];
        }
      assert(ref_txb_coeff_cost == rd_stats_dst->txb_coeff_cost[plane]);
    }
  }
#endif
}

static INLINE void av1_accumulate_rd_stats(RD_STATS *rd_stats, int64_t dist,
                                           int rate, int skip, int64_t sse,
                                           int zero_rate) {
  assert(rd_stats->rate != INT_MAX && rate != INT_MAX);
  rd_stats->rate += rate;
  if (!rd_stats->zero_rate) rd_stats->zero_rate = zero_rate;
  rd_stats->dist += dist;
  rd_stats->skip &= skip;
  rd_stats->sse += sse;
}

static INLINE int64_t av1_calculate_rd_cost(int mult, int rate, int64_t dist) {
  assert(mult >= 0);
  if (rate >= 0) {
    return RDCOST(mult, rate, dist);
  }
  return RDCOST_NEG_R(mult, -rate, dist);
}

static INLINE void av1_rd_cost_update(int mult, RD_STATS *rd_cost) {
  if (rd_cost->rate < INT_MAX && rd_cost->dist < INT64_MAX &&
      rd_cost->rdcost < INT64_MAX) {
    rd_cost->rdcost = av1_calculate_rd_cost(mult, rd_cost->rate, rd_cost->dist);
  } else {
    av1_invalid_rd_stats(rd_cost);
  }
}

static INLINE void av1_rd_stats_subtraction(int mult,
                                            const RD_STATS *const left,
                                            const RD_STATS *const right,
                                            RD_STATS *result) {
  if (left->rate == INT_MAX || right->rate == INT_MAX ||
      left->dist == INT64_MAX || right->dist == INT64_MAX ||
      left->rdcost == INT64_MAX || right->rdcost == INT64_MAX) {
    av1_invalid_rd_stats(result);
  } else {
    result->rate = left->rate - right->rate;
    result->dist = left->dist - right->dist;
    result->rdcost = av1_calculate_rd_cost(mult, result->rate, result->dist);
  }
}

struct TileInfo;
struct TileDataEnc;
struct AV1_COMP;
struct macroblock;

int av1_compute_rd_mult_based_on_qindex(const struct AV1_COMP *cpi, int qindex);

int av1_compute_rd_mult(const struct AV1_COMP *cpi, int qindex);

void av1_initialize_rd_consts(struct AV1_COMP *cpi);

void av1_initialize_cost_tables(const AV1_COMMON *const cm, MACROBLOCK *x);

void av1_initialize_me_consts(const struct AV1_COMP *cpi, MACROBLOCK *x,
                              int qindex);

void av1_model_rd_from_var_lapndz(int64_t var, unsigned int n,
                                  unsigned int qstep, int *rate, int64_t *dist);

void av1_model_rd_curvfit(BLOCK_SIZE bsize, double sse_norm, double xqr,
                          double *rate_f, double *distbysse_f);
void av1_model_rd_surffit(BLOCK_SIZE bsize, double sse_norm, double xm,
                          double yl, double *rate_f, double *distbysse_f);

int av1_get_switchable_rate(const AV1_COMMON *const cm, MACROBLOCK *x,
                            const MACROBLOCKD *xd);

int av1_raster_block_offset(BLOCK_SIZE plane_bsize, int raster_block,
                            int stride);

int16_t *av1_raster_block_offset_int16(BLOCK_SIZE plane_bsize, int raster_block,
                                       int16_t *base);

YV12_BUFFER_CONFIG *av1_get_scaled_ref_frame(const struct AV1_COMP *cpi,
                                             int ref_frame);

void av1_init_me_luts(void);

void av1_set_mvcost(MACROBLOCK *x, int ref, int ref_mv_idx);

void av1_get_entropy_contexts(BLOCK_SIZE bsize,
                              const struct macroblockd_plane *pd,
                              ENTROPY_CONTEXT t_above[MAX_MIB_SIZE],
                              ENTROPY_CONTEXT t_left[MAX_MIB_SIZE]);

void av1_set_rd_speed_thresholds(struct AV1_COMP *cpi);

void av1_update_rd_thresh_fact(const AV1_COMMON *const cm,
                               int (*fact)[MAX_MODES], int rd_thresh, int bsize,
                               int best_mode_index);

static INLINE int rd_less_than_thresh(int64_t best_rd, int thresh,
                                      int thresh_fact) {
  return best_rd < ((int64_t)thresh * thresh_fact >> 5) || thresh == INT_MAX;
}

void av1_mv_pred(const struct AV1_COMP *cpi, MACROBLOCK *x,
                 uint8_t *ref_y_buffer, int ref_y_stride, int ref_frame,
                 BLOCK_SIZE block_size);

static INLINE void set_error_per_bit(MACROBLOCK *x, int rdmult) {
  x->errorperbit = rdmult >> RD_EPB_SHIFT;
  x->errorperbit += (x->errorperbit == 0);
}

void av1_setup_pred_block(const MACROBLOCKD *xd,
                          struct buf_2d dst[MAX_MB_PLANE],
                          const YV12_BUFFER_CONFIG *src, int mi_row, int mi_col,
                          const struct scale_factors *scale,
                          const struct scale_factors *scale_uv,
                          const int num_planes);

int av1_get_intra_cost_penalty(int qindex, int qdelta,
                               aom_bit_depth_t bit_depth);

void av1_fill_mode_rates(AV1_COMMON *const cm, MACROBLOCK *x,
                         FRAME_CONTEXT *fc);

void av1_fill_coeff_costs(MACROBLOCK *x, FRAME_CONTEXT *fc,
                          const int num_planes);

int av1_get_adaptive_rdmult(const struct AV1_COMP *cpi, double beta);

int av1_get_deltaq_offset(const struct AV1_COMP *cpi, int qindex, double beta);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AOM_AV1_ENCODER_RD_H_
