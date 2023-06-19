/*
 * Copyright (c) 2023, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <arm_neon.h>
#include <assert.h>

#include "config/aom_config.h"
#include "config/aom_dsp_rtcd.h"

#include "aom_dsp/arm/mem_neon.h"

#include "av1/encoder/reconinter_enc.h"

void aom_upsampled_pred_neon(MACROBLOCKD *xd, const AV1_COMMON *const cm,
                             int mi_row, int mi_col, const MV *const mv,
                             uint8_t *comp_pred, int width, int height,
                             int subpel_x_q3, int subpel_y_q3,
                             const uint8_t *ref, int ref_stride,
                             int subpel_search) {
  // expect xd == NULL only in tests
  if (xd != NULL) {
    const MB_MODE_INFO *mi = xd->mi[0];
    const int ref_num = 0;
    const int is_intrabc = is_intrabc_block(mi);
    const struct scale_factors *const sf =
        is_intrabc ? &cm->sf_identity : xd->block_ref_scale_factors[ref_num];
    const int is_scaled = av1_is_scaled(sf);

    if (is_scaled) {
      int plane = 0;
      const int mi_x = mi_col * MI_SIZE;
      const int mi_y = mi_row * MI_SIZE;
      const struct macroblockd_plane *const pd = &xd->plane[plane];
      const struct buf_2d *const dst_buf = &pd->dst;
      const struct buf_2d *const pre_buf =
          is_intrabc ? dst_buf : &pd->pre[ref_num];

      InterPredParams inter_pred_params;
      inter_pred_params.conv_params = get_conv_params(0, plane, xd->bd);
      const int_interpfilters filters =
          av1_broadcast_interp_filter(EIGHTTAP_REGULAR);
      av1_init_inter_params(
          &inter_pred_params, width, height, mi_y >> pd->subsampling_y,
          mi_x >> pd->subsampling_x, pd->subsampling_x, pd->subsampling_y,
          xd->bd, is_cur_buf_hbd(xd), is_intrabc, sf, pre_buf, filters);
      av1_enc_build_one_inter_predictor(comp_pred, width, mv,
                                        &inter_pred_params);
      return;
    }
  }

  const InterpFilterParams *filter_params = av1_get_filter(subpel_search);

  if (!subpel_x_q3 && !subpel_y_q3) {
    if (width > 8) {
      assert(width % 16 == 0);
      int i = height;
      do {
        int j = 0;
        do {
          uint8x16_t r = vld1q_u8(ref + j);
          vst1q_u8(comp_pred + j, r);
          j += 16;
        } while (j < width);
        ref += ref_stride;
        comp_pred += width;
      } while (--i != 0);
    } else if (width == 8) {
      int i = height;
      do {
        uint8x8_t r = vld1_u8(ref);
        vst1_u8(comp_pred, r);
        ref += ref_stride;
        comp_pred += width;
      } while (--i != 0);
    } else {
      assert(width == 4);
      int i = height / 2;
      do {
        uint8x8_t r = load_unaligned_u8(ref, ref_stride);
        vst1_u8(comp_pred, r);
        ref += 2 * ref_stride;
        comp_pred += 2 * width;
      } while (--i != 0);
    }
  } else if (!subpel_y_q3) {
    const int16_t *const filter_x =
        av1_get_interp_filter_subpel_kernel(filter_params, subpel_x_q3 << 1);
    aom_convolve8_horiz_neon(ref, ref_stride, comp_pred, width, filter_x, 16,
                             NULL, -1, width, height);
  } else if (!subpel_x_q3) {
    const int16_t *const filter_y =
        av1_get_interp_filter_subpel_kernel(filter_params, subpel_y_q3 << 1);
    aom_convolve8_vert_neon(ref, ref_stride, comp_pred, width, NULL, -1,
                            filter_y, 16, width, height);
  } else {
    DECLARE_ALIGNED(16, uint8_t,
                    im_block[((MAX_SB_SIZE * 2 + 16) + 16) * MAX_SB_SIZE]);

    const int16_t *const filter_x =
        av1_get_interp_filter_subpel_kernel(filter_params, subpel_x_q3 << 1);
    const int16_t *const filter_y =
        av1_get_interp_filter_subpel_kernel(filter_params, subpel_y_q3 << 1);

    const int im_stride = MAX_SB_SIZE;
    const int im_height = (((height - 1) * 8 + subpel_y_q3) >> 3) + SUBPEL_TAPS;

    const int ref_vert_offset = ref_stride * ((SUBPEL_TAPS >> 1) - 1);
    const int im_vert_offset = im_stride * ((filter_params->taps >> 1) - 1);

    assert(im_height <= (MAX_SB_SIZE * 2 + 16) + 16);
    aom_convolve8_horiz_neon(ref - ref_vert_offset, ref_stride, im_block,
                             MAX_SB_SIZE, filter_x, 16, NULL, -1, width,
                             im_height);
    aom_convolve8_vert_neon(im_block + im_vert_offset, MAX_SB_SIZE, comp_pred,
                            width, NULL, -1, filter_y, 16, width, height);
  }
}

void aom_comp_avg_upsampled_pred_neon(MACROBLOCKD *xd,
                                      const AV1_COMMON *const cm, int mi_row,
                                      int mi_col, const MV *const mv,
                                      uint8_t *comp_pred, const uint8_t *pred,
                                      int width, int height, int subpel_x_q3,
                                      int subpel_y_q3, const uint8_t *ref,
                                      int ref_stride, int subpel_search) {
  aom_upsampled_pred_neon(xd, cm, mi_row, mi_col, mv, comp_pred, width, height,
                          subpel_x_q3, subpel_y_q3, ref, ref_stride,
                          subpel_search);

  aom_comp_avg_pred_neon(comp_pred, pred, width, height, comp_pred, width);
}

#if CONFIG_AV1_HIGHBITDEPTH
void aom_highbd_upsampled_pred_neon(MACROBLOCKD *xd,
                                    const struct AV1Common *const cm,
                                    int mi_row, int mi_col, const MV *const mv,
                                    uint8_t *comp_pred8, int width, int height,
                                    int subpel_x_q3, int subpel_y_q3,
                                    const uint8_t *ref8, int ref_stride, int bd,
                                    int subpel_search) {
  // expect xd == NULL only in tests
  if (xd != NULL) {
    const MB_MODE_INFO *mi = xd->mi[0];
    const int ref_num = 0;
    const int is_intrabc = is_intrabc_block(mi);
    const struct scale_factors *const sf =
        is_intrabc ? &cm->sf_identity : xd->block_ref_scale_factors[ref_num];
    const int is_scaled = av1_is_scaled(sf);

    if (is_scaled) {
      int plane = 0;
      const int mi_x = mi_col * MI_SIZE;
      const int mi_y = mi_row * MI_SIZE;
      const struct macroblockd_plane *const pd = &xd->plane[plane];
      const struct buf_2d *const dst_buf = &pd->dst;
      const struct buf_2d *const pre_buf =
          is_intrabc ? dst_buf : &pd->pre[ref_num];

      InterPredParams inter_pred_params;
      inter_pred_params.conv_params = get_conv_params(0, plane, xd->bd);
      const int_interpfilters filters =
          av1_broadcast_interp_filter(EIGHTTAP_REGULAR);
      av1_init_inter_params(
          &inter_pred_params, width, height, mi_y >> pd->subsampling_y,
          mi_x >> pd->subsampling_x, pd->subsampling_x, pd->subsampling_y,
          xd->bd, is_cur_buf_hbd(xd), is_intrabc, sf, pre_buf, filters);
      av1_enc_build_one_inter_predictor(comp_pred8, width, mv,
                                        &inter_pred_params);
      return;
    }
  }

  const InterpFilterParams *filter = av1_get_filter(subpel_search);

  if (!subpel_x_q3 && !subpel_y_q3) {
    const uint16_t *ref = CONVERT_TO_SHORTPTR(ref8);
    uint16_t *comp_pred = CONVERT_TO_SHORTPTR(comp_pred8);
    for (int i = 0; i < height; i++) {
      memcpy(comp_pred, ref, width * sizeof(*comp_pred));
      comp_pred += width;
      ref += ref_stride;
    }
  } else if (!subpel_y_q3) {
    const int16_t *const kernel =
        av1_get_interp_filter_subpel_kernel(filter, subpel_x_q3 << 1);
    aom_highbd_convolve8_horiz_neon(ref8, ref_stride, comp_pred8, width, kernel,
                                    16, NULL, -1, width, height, bd);
  } else if (!subpel_x_q3) {
    const int16_t *const kernel =
        av1_get_interp_filter_subpel_kernel(filter, subpel_y_q3 << 1);
    aom_highbd_convolve8_vert_neon(ref8, ref_stride, comp_pred8, width, NULL,
                                   -1, kernel, 16, width, height, bd);
  } else {
    DECLARE_ALIGNED(16, uint16_t,
                    temp[((MAX_SB_SIZE + 16) + 16) * MAX_SB_SIZE]);
    const int16_t *const kernel_x =
        av1_get_interp_filter_subpel_kernel(filter, subpel_x_q3 << 1);
    const int16_t *const kernel_y =
        av1_get_interp_filter_subpel_kernel(filter, subpel_y_q3 << 1);
    const int intermediate_height =
        (((height - 1) * 8 + subpel_y_q3) >> 3) + filter->taps;
    assert(intermediate_height <= (MAX_SB_SIZE * 2 + 16) + 16);
    aom_highbd_convolve8_horiz_neon(
        ref8 - ref_stride * ((filter->taps >> 1) - 1), ref_stride,
        CONVERT_TO_BYTEPTR(temp), MAX_SB_SIZE, kernel_x, 16, NULL, -1, width,
        intermediate_height, bd);
    aom_highbd_convolve8_vert_neon(
        CONVERT_TO_BYTEPTR(temp + MAX_SB_SIZE * ((filter->taps >> 1) - 1)),
        MAX_SB_SIZE, comp_pred8, width, NULL, -1, kernel_y, 16, width, height,
        bd);
  }
}

void aom_highbd_comp_avg_upsampled_pred_neon(
    MACROBLOCKD *xd, const struct AV1Common *const cm, int mi_row, int mi_col,
    const MV *const mv, uint8_t *comp_pred8, const uint8_t *pred8, int width,
    int height, int subpel_x_q3, int subpel_y_q3, const uint8_t *ref8,
    int ref_stride, int bd, int subpel_search) {
  int i;

  const uint16_t *pred = CONVERT_TO_SHORTPTR(pred8);
  uint16_t *comp_pred = CONVERT_TO_SHORTPTR(comp_pred8);
  aom_highbd_upsampled_pred_neon(xd, cm, mi_row, mi_col, mv, comp_pred8, width,
                                 height, subpel_x_q3, subpel_y_q3, ref8,
                                 ref_stride, bd, subpel_search);

  const int16x8_t shift = vdupq_n_s16(-1);
  for (i = 0; i < width * height; i += 8) {
    uint16x8_t p = vld1q_u16(&pred[i]);
    uint16x8_t cp = vld1q_u16(&comp_pred[i]);
    cp = vqrshlq_u16(vaddq_u16(p, cp), shift);
    vst1q_u16(&comp_pred[i], cp);
  }
}

void aom_highbd_dist_wtd_comp_avg_upsampled_pred_neon(
    MACROBLOCKD *xd, const struct AV1Common *const cm, int mi_row, int mi_col,
    const MV *const mv, uint8_t *comp_pred8, const uint8_t *pred8, int width,
    int height, int subpel_x_q3, int subpel_y_q3, const uint8_t *ref8,
    int ref_stride, int bd, const DIST_WTD_COMP_PARAMS *jcp_param,
    int subpel_search) {
  int i;
  const uint16_t *pred = CONVERT_TO_SHORTPTR(pred8);
  uint16_t *comp_pred = CONVERT_TO_SHORTPTR(comp_pred8);
  aom_highbd_upsampled_pred_neon(xd, cm, mi_row, mi_col, mv, comp_pred8, width,
                                 height, subpel_x_q3, subpel_y_q3, ref8,
                                 ref_stride, bd, subpel_search);

  const int32x4_t shift = vdupq_n_s32(-DIST_PRECISION_BITS);
  const uint16x4_t fwd_offset_u16 = vdup_n_u16(jcp_param->fwd_offset);
  const uint16x4_t bck_offset_u16 = vdup_n_u16(jcp_param->bck_offset);
  for (i = 0; i < width * height; i += 8) {
    uint16x8_t p = vld1q_u16(&pred[i]);
    uint16x8_t cp = vld1q_u16(&comp_pred[i]);
    uint32x4_t cp0 = vmull_u16(vget_low_u16(p), bck_offset_u16);
    uint32x4_t cp1 = vmull_u16(vget_high_u16(p), bck_offset_u16);
    cp0 = vmlal_u16(cp0, vget_low_u16(cp), fwd_offset_u16);
    cp1 = vmlal_u16(cp1, vget_high_u16(cp), fwd_offset_u16);
    cp0 = vqrshlq_u32(cp0, shift);
    cp1 = vqrshlq_u32(cp1, shift);
    cp = vcombine_u16(vmovn_u32(cp0), vmovn_u32(cp1));
    vst1q_u16(&comp_pred[i], cp);
  }
}

#endif  // CONFIG_AV1_HIGHBITDEPTH
