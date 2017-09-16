#ifndef AOM_DSP_DAALA_TX_H_
#define AOM_DSP_DAALA_TX_H_

#include "aom_dsp/aom_dsp_common.h"
#include "av1/common/odintrin.h"

void daala_fdct4(const tran_low_t *input, tran_low_t *output);
void daala_idct4(const tran_low_t *input, tran_low_t *output);

void od_bin_fdct4(od_coeff y[4], const od_coeff *x, int xstride);
void od_bin_idct4(od_coeff *x, int xstride, const od_coeff y[4]);
void od_bin_fdst4(od_coeff y[4], const od_coeff *x, int xstride);
void od_bin_idst4(od_coeff *x, int xstride, const od_coeff y[4]);
void od_bin_fdct8(od_coeff y[8], const od_coeff *x, int xstride);
void od_bin_idct8(od_coeff *x, int xstride, const od_coeff y[8]);
void od_bin_fdst8(od_coeff y[8], const od_coeff *x, int xstride);
void od_bin_idst8(od_coeff *x, int xstride, const od_coeff y[8]);
void od_bin_fdct16(od_coeff y[16], const od_coeff *x, int xstride);
void od_bin_idct16(od_coeff *x, int xstride, const od_coeff y[16]);
void od_bin_fdst16(od_coeff y[16], const od_coeff *x, int xstride);
void od_bin_idst16(od_coeff *x, int xstride, const od_coeff y[16]);
void od_bin_fdct32(od_coeff y[32], const od_coeff *x, int xstride);
void od_bin_idct32(od_coeff *x, int xstride, const od_coeff y[32]);
#if CONFIG_TX64X64
void od_bin_fdct64(od_coeff y[64], const od_coeff *x, int xstride);
void od_bin_idct64(od_coeff *x, int xstride, const od_coeff y[64]);
#endif
#endif
