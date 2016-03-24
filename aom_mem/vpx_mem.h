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

#ifndef VPX_MEM_VPX_MEM_H_
#define VPX_MEM_VPX_MEM_H_

#include "vpx_config.h"
#if defined(__uClinux__)
#include <lddk.h>
#endif

#include <stdlib.h>
#include <stddef.h>

#if defined(__cplusplus)
extern "C" {
#endif

void *vpx_memalign(size_t align, size_t size);
void *vpx_malloc(size_t size);
void *vpx_calloc(size_t num, size_t size);
void *vpx_realloc(void *memblk, size_t size);
void vpx_free(void *memblk);

#if CONFIG_VPX_HIGHBITDEPTH
void *vpx_memset16(void *dest, int val, size_t length);
#endif

#include <string.h>

#ifdef VPX_MEM_PLTFRM
#include VPX_MEM_PLTFRM
#endif

#if defined(__cplusplus)
}
#endif

#endif  // VPX_MEM_VPX_MEM_H_
