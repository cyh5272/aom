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

#ifndef AOM_DSP_BITREADER_H_
#define AOM_DSP_BITREADER_H_

#include <execinfo.h>
#include <stddef.h>
#include <limits.h>

#include "./aom_config.h"
#include "aom/aomdx.h"
#include "aom/aom_integer.h"
#if CONFIG_DAALA_EC
#include "aom_dsp/daalaboolreader.h"
#else
#include "aom_dsp/dkboolreader.h"
#endif
#include "aom_dsp/prob.h"
#include "aom_util/debug_util.h"
#include "av1/common/odintrin.h"

#ifdef __cplusplus
extern "C" {
#endif

#if CONFIG_DAALA_EC
typedef struct daala_reader aom_reader;
#else
typedef struct aom_dk_reader aom_reader;
#endif

static INLINE int aom_reader_init(aom_reader *r, const uint8_t *buffer,
                                  size_t size, aom_decrypt_cb decrypt_cb,
                                  void *decrypt_state) {
#if CONFIG_DAALA_EC
  (void)decrypt_cb;
  (void)decrypt_state;
  return aom_daala_reader_init(r, buffer, size);
#else
  return aom_dk_reader_init(r, buffer, size, decrypt_cb, decrypt_state);
#endif
}

static INLINE const uint8_t *aom_reader_find_end(aom_reader *r) {
#if CONFIG_DAALA_EC
  return aom_daala_reader_find_end(r);
#else
  return aom_dk_reader_find_end(r);
#endif
}

static INLINE int aom_reader_has_error(aom_reader *r) {
#if CONFIG_DAALA_EC
  return aom_daala_reader_has_error(r);
#else
  return aom_dk_reader_has_error(r);
#endif
}
static void print_trace(void) {
  void *array[10];
  size_t size;
  char **strings;
  size_t i;

  size = backtrace(array, 10);
  strings = backtrace_symbols(array, size);

  printf("Obtained %zd stack frames.\n", size);

  for (i = 0; i < size; i++) printf("%s\n", strings[i]);

  free(strings);
}

static INLINE int aom_read(aom_reader *r, int prob) {
  int result;
#if CONFIG_BITSTREAM_DEBUG
  int ref_result, ref_prob;
#endif

#if CONFIG_DAALA_EC
  result = aom_daala_read(r, prob);
#else
  result = aom_dk_read(r, prob);
#endif

#if CONFIG_BITSTREAM_DEBUG
  bitstream_queue_pop(&ref_result, &ref_prob);
  if (prob != ref_prob) {
    printf("prob error prob %d ref_prob %d\n", prob, ref_prob);
    assert(0);
  }
  if (result != ref_result) {
    printf("result error result %d ref_result %d\n", result, ref_result);
    assert(0);
  }
#endif
  return result;
}

static INLINE int aom_read_bit(aom_reader *r) {
  return aom_read(r, 128);  // aom_prob_half
}

static INLINE int aom_read_literal(aom_reader *r, int bits) {
  int literal = 0, bit;

  for (bit = bits - 1; bit >= 0; bit--) literal |= aom_read_bit(r) << bit;

  return literal;
}

static INLINE int aom_read_tree_bits(aom_reader *r, const aom_tree_index *tree,
                                     const aom_prob *probs) {
  aom_tree_index i = 0;

  while ((i = tree[i + aom_read(r, probs[i >> 1])]) > 0) continue;

  return -i;
}

static INLINE int aom_read_tree(aom_reader *r, const aom_tree_index *tree,
                                const aom_prob *probs) {
#if CONFIG_DAALA_EC
  return daala_read_tree_bits(r, tree, probs);
#else
  return aom_read_tree_bits(r, tree, probs);
#endif
}

static INLINE int aom_read_tree_cdf(aom_reader *r, const uint16_t *cdf,
                                    int nsymbs) {
#if CONFIG_DAALA_EC
  return daala_read_tree_cdf(r, cdf, nsymbs);
#else
  (void)r;
  (void)cdf;
  (void)nsymbs;
  assert(0 && "Unsupported bitreader operation");
  return -1;
#endif
}

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AOM_DSP_BITREADER_H_
