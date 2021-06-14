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

#ifndef AOM_AV1_ENCODER_HYBRID_FWD_TXFM_H_
#define AOM_AV1_ENCODER_HYBRID_FWD_TXFM_H_

#include "config/aom_config.h"

#ifdef __cplusplus
extern "C" {
#endif

void av1_fwd_txfm(const int16_t *src_diff, tran_low_t *coeff, int diff_stride,
                  TxfmParam *txfm_param);

void av1_highbd_fwd_txfm(const int16_t *src_diff, tran_low_t *coeff,
                         int diff_stride, TxfmParam *txfm_param);

#if CONFIG_IST
void av1_fwd_stxfm(tran_low_t *coeff, TxfmParam *txfm_param);
#endif

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AOM_AV1_ENCODER_HYBRID_FWD_TXFM_H_
