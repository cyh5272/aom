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

#include <math.h>

#include "aom_ports/system_state.h"

#include "av1/common/blockd.h"
#include "av1/common/onyxc_int.h"

#if CONFIG_WIENER_NONSEP
// const int wienerns_config[WIENERNS_NUM_PIXEL][3] = {
//   { 1, 0, 0 },  { -1, 0, 0 },
//   { 0, 1, 1 },  { 0, -1, 1 },
//   { 2, 0, 2 },  { -2, 0, 2 },
//   { 0, 2, 3 },  { 0, -2, 3 },
//   { 1, 1, 4 },  { 1, -1, 4 },
//   { -1, 1, 5 }, { -1, -1, 5 },
//   { 2, 2, 6 },  { 2, -2, 6 },
//   { -2, 2, 7 }, { -2, -2, 7 },
//   { 3, 0, 8 },  { -3, 0, 8 },
//   { 0, 3, 9 },  { 0, -3, 9 },
//   { 3, 3, 10 },  { 3, -3, 10 },
//   { -3, 3, 11 }, { -3, -3, 11 },
// };
// const int wienerns_coeff_info[WIENERNS_NUM_COEFF][4] = {
//   { 5, -3, 4, 128 }, { 5, -3, 4, 128 }, { 5, -23, 3, 256 }, { 5, -23, 3, 256
//   }, { 5, -28, 3, 256 }, { 5, -28, 3, 256 },  { 4, -2, 3, 512 },  { 4, -2, 3,
//   512 }, { 4, -5, 3, 256 }, { 4, -5, 3, 256 }, { 3, -5, 3, 512 }, { 3, -6, 3,
//   512 }
// };
// const int wienerns_config[WIENERNS_NUM_PIXEL][3] = {
//   { 1, 0, 0 },  { -1, 0, 0 },
//   { 0, 1, 1 },  { 0, -1, 1 },
//   { 2, 0, 2 },  { -2, 0, 2 },
//   { 0, 2, 3 },  { 0, -2, 3 },
//   { 1, 1, 4 },  { 1, -1, 4 }, { -1, 1, 4 }, { -1, -1, 4 },
//   { 2, 2, 5 },  { 2, -2, 5 }, { -2, 2, 5 }, { -2, -2, 5 },
//   { 3, 0, 6 },  { -3, 0, 6 },
// { 0, 3, 7 },  { 0, -3, 7 },
//   { 3, 3, 8 },  { 3, -3, 8 }, { -3, 3, 8 }, { -3, -3, 8 },
// };
// const int wienerns_coeff_info[WIENERNS_NUM_COEFF][4] = {
//   { 5, -3, 4, 128 }, { 5, -4, 4, 128 }, { 5, -23, 3, 256 }, { 5, -23, 3, 256
//   }, { 5, -27, 3, 256 }, { 4, -2, 3, 512 },  { 4, -5, 3, 256 },  { 4, -5, 3,
//   256 }, { 4, -9, 3, 512 }
// };
const int wienerns_config[WIENERNS_NUM_PIXEL][3] = {
  { 1, 0, 0 },  { -1, 0, 0 },   { 0, 1, 1 },   { 0, -1, 1 },  { 2, 0, 2 },
  { -2, 0, 2 }, { 0, 2, 3 },    { 0, -2, 3 },  { 1, 1, 4 },   { -1, -1, 4 },
  { -1, 1, 5 }, { 1, -1, 5 },   { 2, 2, 6 },   { -2, -2, 6 }, { -2, 2, 7 },
  { 2, -2, 7 }, { 3, 0, 8 },    { -3, 0, 8 },  { 0, 3, 9 },   { 0, -3, 9 },
  { 3, 3, 10 }, { -3, -3, 10 }, { 3, -3, 11 }, { -3, 3, 11 }
};
const int wienerns_coeff_info[WIENERNS_NUM_COEFF][4] = {
  { 5, -6, 4, 128 },  { 5, -6, 4, 128 },  { 4, -10, 3, 128 },
  { 4, -10, 3, 128 }, { 4, -12, 3, 128 }, { 4, -12, 3, 128 },
  { 3, -2, 3, 128 },  { 3, -3, 3, 128 },  { 3, -3, 3, 128 },
  { 3, -3, 3, 128 },  { 3, -4, 3, 128 },  { 3, -4, 3, 128 }
};
#endif  // CONFIG_WIENER_NONSEP

PREDICTION_MODE av1_left_block_mode(const MB_MODE_INFO *left_mi) {
  if (!left_mi) return DC_PRED;
  assert(!is_inter_block(left_mi) || is_intrabc_block(left_mi));
  return left_mi->mode;
}

PREDICTION_MODE av1_above_block_mode(const MB_MODE_INFO *above_mi) {
  if (!above_mi) return DC_PRED;
  assert(!is_inter_block(above_mi) || is_intrabc_block(above_mi));
  return above_mi->mode;
}

void av1_reset_is_mi_coded_map(MACROBLOCKD *xd, int stride) {
  av1_zero(xd->is_mi_coded);
  xd->is_mi_coded_stride = stride;
}

void av1_mark_block_as_coded(MACROBLOCKD *xd, int mi_row, int mi_col,
                             BLOCK_SIZE bsize, BLOCK_SIZE sb_size) {
  const int sb_mi_size = mi_size_wide[sb_size];
  const int mi_row_offset = mi_row & (sb_mi_size - 1);
  const int mi_col_offset = mi_col & (sb_mi_size - 1);

  for (int r = 0; r < mi_size_high[bsize]; ++r)
    for (int c = 0; c < mi_size_wide[bsize]; ++c) {
      const int pos =
          (mi_row_offset + r) * xd->is_mi_coded_stride + mi_col_offset + c;
      xd->is_mi_coded[pos] = 1;
    }
}

void av1_mark_block_as_not_coded(MACROBLOCKD *xd, int mi_row, int mi_col,
                                 BLOCK_SIZE bsize, BLOCK_SIZE sb_size) {
  const int sb_mi_size = mi_size_wide[sb_size];
  const int mi_row_offset = mi_row & (sb_mi_size - 1);
  const int mi_col_offset = mi_col & (sb_mi_size - 1);

  for (int r = 0; r < mi_size_high[bsize]; ++r) {
    uint8_t *row_ptr =
        &xd->is_mi_coded[(mi_row_offset + r) * xd->is_mi_coded_stride +
                         mi_col_offset];
    memset(row_ptr, 0, mi_size_wide[bsize] * sizeof(xd->is_mi_coded[0]));
  }
}

#if CONFIG_INTRA_ENTROPY
#if !CONFIG_USE_SMALL_MODEL
INLINE static void add_hist_features(const uint64_t *hist, float **features) {
  float total = 0.0f;
  if (hist) {
    for (int i = 0; i < 8; ++i) {
      total += (float)hist[i];
    }
  }

  (*features)[0] = logf(total + 1.0f);
  ++*features;

  float *feature_pt = *features;
  if (total > 0.1f) {
    for (int i = 0; i < 8; ++i) {
      feature_pt[i] = (float)hist[i] / total;
    }
  } else {
    for (int i = 0; i < 8; ++i) {
      feature_pt[i] = 0.125f;
    }
  }
  *features += 8;
}

static void add_onehot(float **features, int num, int n) {
  float *feature_pt = *features;
  memset(feature_pt, 0, sizeof(*feature_pt) * num);
  if (n >= 0 && n < num) feature_pt[n] = 1.0f;
  *features += num;
}

#endif  // !CONFIG_USE_SMALL_MODEL

void av1_get_intra_block_feature(int *sparse_features, float *dense_features,
                                 const MB_MODE_INFO *above_mi,
                                 const MB_MODE_INFO *left_mi,
                                 const MB_MODE_INFO *aboveleft_mi) {
  const MB_MODE_INFO *mbmi_list[3] = { above_mi, left_mi, aboveleft_mi };
  const int num_mbmi = CONFIG_USE_SMALL_MODEL ? 2 : 3;
  for (int i = 0; i < num_mbmi; ++i) {
    const MB_MODE_INFO *mbmi = mbmi_list[i];
    const int data_available = mbmi != NULL;
#if CONFIG_USE_SMALL_MODEL
    *(sparse_features++) = data_available ? mbmi->mode : INTRA_MODES;
#else
    *(dense_features++) = data_available;
    add_onehot(&dense_features, INTRA_MODES, data_available ? mbmi->mode : -1);
    const float var = data_available ? (float)mbmi->y_recon_var : 0.0f;
    *(dense_features++) = logf(var + 1.0f);
    add_hist_features(data_available ? mbmi->y_gradient_hist : NULL,
                      &dense_features);
#endif
  }
#if CONFIG_USE_SMALL_MODEL
  (void)dense_features;
#else
  (void)sparse_features;
#endif
}

void av1_pdf2icdf(float *pdf, aom_cdf_prob *cdf, int nsymbs) {
  float accu = 0.0f;  // AOM_ICDF
  for (int i = 0; i < nsymbs - 1; ++i) {
    accu += pdf[i];
    cdf[i] = AOM_ICDF((aom_cdf_prob)(accu * CDF_PROB_TOP));
  }
  cdf[nsymbs - 1] = 0;
}

void av1_get_intra_uv_block_feature(int *sparse_features, float *features,
                                    PREDICTION_MODE cur_y_mode,
                                    int is_cfl_allowed,
                                    const MB_MODE_INFO *above_mi,
                                    const MB_MODE_INFO *left_mi) {
  *(sparse_features++) = cur_y_mode;
  *(sparse_features++) = !is_cfl_allowed;
  (void)features;
  (void)above_mi;
  (void)left_mi;
}

void av1_get_kf_y_mode_cdf_ml(const MACROBLOCKD *xd, aom_cdf_prob *cdf) {
  NN_CONFIG_EM *nn_model = &(xd->tile_ctx->intra_y_mode);
  av1_get_intra_block_feature(nn_model->sparse_features,
                              nn_model->dense_features, xd->above_mbmi,
                              xd->left_mbmi, xd->aboveleft_mbmi);
  av1_nn_predict_em(nn_model);
  av1_pdf2icdf(nn_model->output, cdf, INTRA_MODES);
}

void av1_get_uv_mode_cdf_ml(const MACROBLOCKD *xd, PREDICTION_MODE y_mode,
                            aom_cdf_prob *cdf) {
  NN_CONFIG_EM *nn_model = &(xd->tile_ctx->intra_uv_mode);
  av1_get_intra_uv_block_feature(
      nn_model->sparse_features, nn_model->dense_features, y_mode,
      is_cfl_allowed(xd), xd->above_mbmi, xd->left_mbmi);
  av1_nn_predict_em(nn_model);
  av1_pdf2icdf(nn_model->output, cdf, UV_INTRA_MODES);
}
#endif  // CONFIG_INTRA_ENTROPY

void av1_set_contexts(const MACROBLOCKD *xd, struct macroblockd_plane *pd,
                      int plane, BLOCK_SIZE plane_bsize, TX_SIZE tx_size,
                      int has_eob, int aoff, int loff) {
  ENTROPY_CONTEXT *const a = pd->above_context + aoff;
  ENTROPY_CONTEXT *const l = pd->left_context + loff;
  const int txs_wide = tx_size_wide_unit[tx_size];
  const int txs_high = tx_size_high_unit[tx_size];

  // above
  if (has_eob && xd->mb_to_right_edge < 0) {
    const int blocks_wide = max_block_wide(xd, plane_bsize, plane);
    const int above_contexts = AOMMIN(txs_wide, blocks_wide - aoff);
    memset(a, has_eob, sizeof(*a) * above_contexts);
    memset(a + above_contexts, 0, sizeof(*a) * (txs_wide - above_contexts));
  } else {
    memset(a, has_eob, sizeof(*a) * txs_wide);
  }

  // left
  if (has_eob && xd->mb_to_bottom_edge < 0) {
    const int blocks_high = max_block_high(xd, plane_bsize, plane);
    const int left_contexts = AOMMIN(txs_high, blocks_high - loff);
    memset(l, has_eob, sizeof(*l) * left_contexts);
    memset(l + left_contexts, 0, sizeof(*l) * (txs_high - left_contexts));
  } else {
    memset(l, has_eob, sizeof(*l) * txs_high);
  }
}
void av1_reset_skip_context(MACROBLOCKD *xd, int mi_row, int mi_col,
                            BLOCK_SIZE bsize, const int num_planes) {
  int i;
  int nplanes;
  int chroma_ref;
  assert(bsize < BLOCK_SIZES_ALL);

  chroma_ref =
      is_chroma_reference(mi_row, mi_col, bsize, xd->plane[1].subsampling_x,
                          xd->plane[1].subsampling_y);
  nplanes = 1 + (num_planes - 1) * chroma_ref;
  for (i = 0; i < nplanes; i++) {
    struct macroblockd_plane *const pd = &xd->plane[i];
    const BLOCK_SIZE plane_bsize = get_plane_block_size(
        mi_row, mi_col, bsize, pd->subsampling_x, pd->subsampling_y);
    assert(plane_bsize < BLOCK_SIZES_ALL);
    const int txs_wide = block_size_wide[plane_bsize] >> tx_size_wide_log2[0];
    const int txs_high = block_size_high[plane_bsize] >> tx_size_high_log2[0];
    memset(pd->above_context, 0, sizeof(ENTROPY_CONTEXT) * txs_wide);
    memset(pd->left_context, 0, sizeof(ENTROPY_CONTEXT) * txs_high);
  }
}

void av1_reset_loop_filter_delta(MACROBLOCKD *xd, int num_planes) {
  xd->delta_lf_from_base = 0;
  const int frame_lf_count =
      num_planes > 1 ? FRAME_LF_COUNT : FRAME_LF_COUNT - 2;
  for (int lf_id = 0; lf_id < frame_lf_count; ++lf_id) xd->delta_lf[lf_id] = 0;
}

void av1_reset_loop_restoration(MACROBLOCKD *xd, const int num_planes) {
  for (int p = 0; p < num_planes; ++p) {
    set_default_wiener(xd->wiener_info + p);
    set_default_sgrproj(xd->sgrproj_info + p);
#if CONFIG_WIENER_NONSEP
    set_default_wiener_nonsep(xd->wiener_nonsep_info + p);
#endif  // CONFIG_WIENER_NONSEP
  }
}

void av1_setup_block_planes(MACROBLOCKD *xd, int ss_x, int ss_y,
                            const int num_planes) {
  int i;

  for (i = 0; i < num_planes; i++) {
    xd->plane[i].plane_type = get_plane_type(i);
    xd->plane[i].subsampling_x = i ? ss_x : 0;
    xd->plane[i].subsampling_y = i ? ss_y : 0;
  }
  for (i = num_planes; i < MAX_MB_PLANE; i++) {
    xd->plane[i].subsampling_x = 1;
    xd->plane[i].subsampling_y = 1;
  }
}

void av1_get_unit_width_height_coeff(const MACROBLOCKD *const xd, int plane,
                                     int mi_row, int mi_col,
                                     BLOCK_SIZE plane_bsize, int row_plane,
                                     int col_plane, int *unit_width,
                                     int *unit_height) {
  const int is_inter = is_inter_block(xd->mi[0]);
  const int max_blocks_wide_plane =
      is_inter ? block_size_wide[plane_bsize] >> tx_size_wide_log2[0]
               : max_block_wide(xd, plane_bsize, plane);
  const int max_blocks_high_plane =
      is_inter ? block_size_high[plane_bsize] >> tx_size_high_log2[0]
               : max_block_high(xd, plane_bsize, plane);

  const struct macroblockd_plane *const pd = &xd->plane[plane];
  const BLOCK_SIZE max_unit_bsize_plane = get_plane_block_size(
      mi_row, mi_col, BLOCK_64X64, pd->subsampling_x, pd->subsampling_y);
  int mu_blocks_wide_plane =
      block_size_wide[max_unit_bsize_plane] >> tx_size_wide_log2[0];
  int mu_blocks_high_plane =
      block_size_high[max_unit_bsize_plane] >> tx_size_high_log2[0];
  mu_blocks_wide_plane = AOMMIN(max_blocks_wide_plane, mu_blocks_wide_plane);
  mu_blocks_high_plane = AOMMIN(max_blocks_high_plane, mu_blocks_high_plane);
  assert(mu_blocks_wide_plane > 0);
  assert(mu_blocks_high_plane > 0);

  *unit_height =
      AOMMIN(mu_blocks_high_plane + row_plane, max_blocks_high_plane);
  *unit_width = AOMMIN(mu_blocks_wide_plane + col_plane, max_blocks_wide_plane);
  assert(*unit_height > 0);
  assert(*unit_width > 0);
}

#if CONFIG_INTRA_ENTROPY && !CONFIG_USE_SMALL_MODEL
static const int16_t cos_angle[8] = {
  // 45 degrees
  45,
  // 22.5 degrees
  59,
  // 0 degrees
  64,
  // -22.5 degrees,
  59,
  // -45 degrees
  45,
  // -67.5 degrees
  24,
  // -90 degrees
  0,
  // 67.5 degrees
  24,
};

static const int16_t sin_angle[8] = {
  // 45 degrees
  45,
  // 22.5 degrees
  24,
  // 0 degrees
  0,
  // -22.5 degrees,
  -24,
  // -45 degrees
  -45,
  // -67.5 degrees
  -59,
  // -90 degrees
  64,
  // 67.5 degrees
  59,
};

static INLINE uint8_t get_angle_idx(int dx, int dy) {
  int max_response = 0;
  int max_angle = 0;
  for (int angle_idx = 0; angle_idx < 8; angle_idx++) {
    const int cos = cos_angle[angle_idx];
    const int sin = sin_angle[angle_idx];
    const int64_t dot_prod = cos * dx + sin * dy;
    const int64_t response = labs(dot_prod);
    if (response > max_response) {
      max_response = response;
      max_angle = angle_idx;
    }
  }

  return max_angle;
}

void av1_get_gradient_hist_lbd_c(const uint8_t *dst, int stride, int rows,
                                 int cols, uint64_t *hist) {
  dst += stride;
  for (int r = 1; r < rows; ++r) {
    int c;
    for (c = 1; c < cols; c += 1) {
      const int dx = dst[c] - dst[c - 1];
      const int dy = dst[c] - dst[c - stride];
      const int mag = dx * dx + dy * dy;
      const uint8_t index = get_angle_idx(dx, dy);
      hist[index] += mag;
    }
    dst += stride;
  }
}

static void get_highbd_gradient_hist(const uint8_t *dst8, int stride, int rows,
                                     int cols, uint64_t *hist) {
  uint16_t *dst = CONVERT_TO_SHORTPTR(dst8);
  dst += stride;
  for (int r = 1; r < rows; ++r) {
    for (int c = 1; c < cols; ++c) {
      const int dx = dst[c] - dst[c - 1];
      const int dy = dst[c] - dst[c - stride];
      const int mag = dx * dx + dy * dy;
      const uint8_t index = get_angle_idx(dx, dy);
      hist[index] += mag;
    }
    dst += stride;
  }
}

void av1_get_gradient_hist(const MACROBLOCKD *const xd,
                           MB_MODE_INFO *const mbmi, BLOCK_SIZE bsize) {
  const int y_stride = xd->plane[0].dst.stride;
  const uint8_t *y_dst = xd->plane[0].dst.buf;
  const int rows = block_size_high[bsize];
  const int cols = block_size_wide[bsize];
  const int y_block_rows =
      xd->mb_to_bottom_edge >= 0 ? rows : (xd->mb_to_bottom_edge >> 3) + rows;
  const int y_block_cols =
      xd->mb_to_right_edge >= 0 ? cols : (xd->mb_to_right_edge >> 3) + cols;

  av1_zero(mbmi->y_gradient_hist);
  if (is_cur_buf_hbd(xd)) {
    get_highbd_gradient_hist(y_dst, y_stride, y_block_rows, y_block_cols,
                             mbmi->y_gradient_hist);
  } else {
    av1_get_gradient_hist_lbd(y_dst, y_stride, y_block_rows, y_block_cols,
                              mbmi->y_gradient_hist);
  }
}

static int64_t variance(const uint8_t *dst, int stride, int w, int h) {
  int64_t sum = 0;
  int64_t sum_square = 0;
  for (int i = 0; i < h; ++i) {
    for (int j = 0; j < w; ++j) {
      const int v = dst[j];
      sum += v;
      sum_square += v * v;
    }
    dst += stride;
  }
  const int n = w * h;
  const int64_t var = (n * sum_square - sum * sum) / n / n;
  return var < 0 ? 0 : var;
}

static int64_t highbd_variance(const uint8_t *dst8, int stride, int w, int h) {
  const uint16_t *dst = CONVERT_TO_SHORTPTR(dst8);
  int64_t sum = 0;
  int64_t sum_square = 0;
  for (int i = 0; i < h; ++i) {
    for (int j = 0; j < w; ++j) {
      const int v = dst[j];
      sum += v;
      sum_square += v * v;
    }
    dst += stride;
  }
  const int n = w * h;
  const int64_t var = (n * sum_square - sum * sum) / n / n;
  return var < 0 ? 0 : var;
}

void av1_get_recon_var(const MACROBLOCKD *const xd, MB_MODE_INFO *const mbmi,
                       BLOCK_SIZE bsize) {
  const int y_stride = xd->plane[0].dst.stride;
  const uint8_t *y_dst = xd->plane[0].dst.buf;
  const int rows = block_size_high[bsize];
  const int cols = block_size_wide[bsize];
  const int y_block_rows =
      xd->mb_to_bottom_edge >= 0 ? rows : (xd->mb_to_bottom_edge >> 3) + rows;
  const int y_block_cols =
      xd->mb_to_right_edge >= 0 ? cols : (xd->mb_to_right_edge >> 3) + cols;

  if (is_cur_buf_hbd(xd)) {
    mbmi->y_recon_var =
        highbd_variance(y_dst, y_stride, y_block_cols, y_block_rows);
  } else {
    mbmi->y_recon_var = variance(y_dst, y_stride, y_block_cols, y_block_rows);
  }
}
#endif  // CONFIG_INTRA_ENTROPY && !CONFIG_USE_SMALL_MODEL

#if CONFIG_DERIVED_INTRA_MODE
#define BINS 56

static const int angle_to_bin[180] = {
  0, 0, 1, 1, 1, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4,
  5, 5, 5, 6, 6, 6, 7, 7, 7, 8, 8, 8, 9, 9, 9, 10,
  10, 10, 10, 11, 11, 11, 12, 12, 12, 13, 13, 13, 14, 14, 14, 15,
  15, 15, 16, 16, 16, 17, 17, 17, 17, 18, 18, 18, 19, 19, 19, 20,
  20, 20, 21, 21, 21, 22, 22, 22, 23, 23, 23, 24, 24, 24, 24, 25,
  25, 25, 25, 26, 26, 26, 27, 27, 27, 28, 28, 28, 29, 29, 29, 30,
  30, 30, 31, 31, 31, 31, 32, 32, 32, 32, 33, 33, 33, 34, 34, 34,
  35, 35, 35, 36, 36, 36, 37, 37, 37, 38, 38, 38, 38, 39, 39, 39,
  40, 40, 40, 41, 41, 41, 42, 42, 42, 43, 43, 43, 44, 44, 44, 45,
  45, 45, 45, 46, 46, 46, 47, 47, 47, 48, 48, 48, 49, 49, 49, 50,
  50, 50, 51, 51, 51, 52, 52, 52, 52, 53, 53, 53, 53, 54, 54, 54,
  55, 55, 55, 55,
};

static float get_gradient_hist(const uint8_t *src, int src_stride, int rows,
                               int cols, float *hist) {
  float total = 0.0f;
  float angle;

  src += src_stride;
  for (int r = 1; r < rows - 1; ++r) {
    for (int c = 1; c < cols - 1; ++c) {
      const uint8_t *above = &src[c - src_stride];
      const uint8_t *below = &src[c + src_stride];
      const uint8_t *left = &src[c - 1];
      const uint8_t *right = &src[c + 1];

      const int dx =
          (right[-src_stride] + 2 * right[0] + right[src_stride]) -
          (left[-src_stride] + 2 * left[0] + left[src_stride]);
      const int dy =
          (below[-1] + 2 * below[0] + below[1]) -
          (above[-1] + 2 * above[0] + above[1]);
      if (dx == 0 && dy == 0) continue;
      const int temp = (int)round(sqrt(dx * dx + dy * dy));
      total += temp;
      if (dx == 0) {
        angle = 0.0f;
      } else {
        angle = atanf(dy * 1.0f / dx);
      }
      int int_angle = 90 - (int)roundf(180 * angle / (float)PI);
      if (int_angle >= 180) int_angle = 0;
      int_angle = AOMMAX(int_angle, 0);
      hist[angle_to_bin[int_angle]] += temp;
    }
    src += src_stride;
  }

  return total;
}

static float get_highbd_gradient_hist(const uint8_t *src8, int src_stride,
                                      int rows, int cols, float *hist) {
  float total = 0.0f;
  float angle;
  uint16_t *src = CONVERT_TO_SHORTPTR(src8);
  src += src_stride;
  for (int r = 1; r < rows - 1; ++r) {
    for (int c = 1; c < cols - 1; ++c) {
      const uint16_t *above = &src[c - src_stride];
      const uint16_t *below = &src[c + src_stride];
      const uint16_t *left = &src[c - 1];
      const uint16_t *right = &src[c + 1];

      const int dx =
          (right[-src_stride] + 2 * right[0] + right[src_stride]) -
          (left[-src_stride] + 2 * left[0] + left[src_stride]);
      const int dy =
          (below[-1] + 2 * below[0] + below[1]) -
          (above[-1] + 2 * above[0] + above[1]);
      if (dx == 0 && dy == 0) continue;
      const int temp = (int)round(sqrt(dx * dx + dy * dy));
      total += temp;
      if (dx == 0) {
        angle = 0.0f;
      } else {
        angle = atanf(dy * 1.0f / dx);
      }
      int int_angle = 90 - (int)roundf(180 * angle / (float)PI);
      if (int_angle >= 180) int_angle = 0;
      int_angle = AOMMAX(int_angle, 0);
      hist[angle_to_bin[int_angle]] += temp;
    }
    src += src_stride;
  }

  return total;
}

static void generate_hog(const MACROBLOCKD *xd, float *hist) {
  const int stride = xd->plane[0].dst.stride;
  const uint8_t *buf = xd->plane[0].dst.buf;
  const int bsize = xd->mi[0]->sb_type;
  const int bh = block_size_high[bsize];
  const int bw = block_size_wide[bsize];
  const int rows =
      (xd->mb_to_bottom_edge >= 0) ? bh : (xd->mb_to_bottom_edge >> 3) + bh;
  const int cols =
      (xd->mb_to_right_edge >= 0) ? bw : (xd->mb_to_right_edge >> 3) + bw;
  const int lines = 4;
  float total = 0.1f;

  if (is_cur_buf_hbd(xd)) {
    if (xd->above_mbmi) {
      total +=
          get_highbd_gradient_hist(buf - lines * stride, stride, lines, cols,
                                   hist);
    }
    if (xd->left_mbmi) {
      total +=
          get_highbd_gradient_hist(buf - lines, stride, rows, lines, hist);
    }
  } else {
    if (xd->above_mbmi) {
      total +=
          get_gradient_hist(buf - lines * stride, stride, lines, cols, hist);
    }
    if (xd->left_mbmi) {
      total += get_gradient_hist(buf - lines, stride, rows, lines, hist);
    }
  }

  for (int i = 0 ; i < BINS; ++i) hist[i] /= total;
}

static int derive_intra_mode_from_hog(const MACROBLOCKD *xd) {
  aom_clear_system_state();

  float hist[BINS] = { 0.0f };
  generate_hog(xd, hist);

  float max_score = -100.0f;
  int best_idx = 0;
  for (int i = 0 ; i < BINS; ++i) {
    const float this_score = hist[i];
    if (this_score > max_score) {
      max_score = this_score;
      best_idx = i;
    }
  }

  aom_clear_system_state();

  return best_idx;
}

int av1_enable_derived_intra_mode(const MACROBLOCKD *xd, int bsize) {
  return bsize >= BLOCK_8X8 && (xd->above_mbmi || xd->left_mbmi);
}

static const int bin_to_angles[BINS] = {
    0, 3, 6, 9, 14, 17, 20, 23, 26, 29, 32, 36, 39, 42, 45, 48, 51, 54, 58, 61,
    64, 67, 70, 73, 76, 81, 84, 87, 90, 93, 96, 99, 104, 107, 110, 113, 116,
    119, 122, 126, 129, 132, 135, 138, 141, 144, 148, 151, 154, 157, 160, 163,
    166, 171, 174, 177,
};

static const int bin_to_mode[BINS] = {
    2, 2, 2, 2, 7, 7, 7, 7, 7, 7, 7, 3, 3, 3, 3, 3,
    3, 3, 8, 8, 8, 8, 8, 8, 8, 1, 1, 1, 1, 1, 1, 1,
    5, 5, 5, 5, 5, 5, 5, 4, 4, 4, 4, 4, 4, 4, 6, 6,
    6, 6, 6, 6, 6, 2, 2, 2,
};
#undef BINS

int av1_get_derived_intra_mode(const MACROBLOCKD *xd, int bsize,
                               int8_t *angle_delta) {
  if (av1_enable_derived_intra_mode(xd, bsize)) {
    const int idx = derive_intra_mode_from_hog(xd);
    int angle = bin_to_angles[idx];
    if (angle < 36) angle += 180;
    const int mode = bin_to_mode[idx];
    *angle_delta = (int8_t)((angle - mode_to_angle_map[mode]) / ANGLE_STEP);
    assert(*angle_delta >= -9 && *angle_delta <= 9);
    return mode;
  }
  return INTRA_MODES;
}
#endif
