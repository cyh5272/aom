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

#ifndef VP10_COMMON_TILE_COMMON_H_
#define VP10_COMMON_TILE_COMMON_H_

#ifdef __cplusplus
extern "C" {
#endif

struct VP10Common;

typedef struct TileInfo {
  int mi_row_start, mi_row_end;
  int mi_col_start, mi_col_end;
} TileInfo;

// initializes 'tile->mi_(row|col)_(start|end)' for (row, col) based on
// 'cm->log2_tile_(rows|cols)' & 'cm->mi_(rows|cols)'
void av1_tile_init(TileInfo *tile, const struct VP10Common *cm, int row,
                    int col);

void av1_tile_set_row(TileInfo *tile, const struct VP10Common *cm, int row);
void av1_tile_set_col(TileInfo *tile, const struct VP10Common *cm, int col);

void av1_get_tile_n_bits(int mi_cols, int *min_log2_tile_cols,
                          int *max_log2_tile_cols);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // VP10_COMMON_TILE_COMMON_H_
