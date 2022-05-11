/*
 * Copyright (c) 2022, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#ifndef AOM_AV1_DUCKY_ENCODE_H_
#define AOM_AV1_DUCKY_ENCODE_H_

#include <string>
#include <vector>
#include <memory>

#include "av1/encoder/firstpass.h"
#include "aom/aom_encoder.h"

namespace aom {
struct VideoInfo {
  int frame_width;
  int frame_height;
  aom_rational_t frame_rate;
  aom_img_fmt_t img_fmt;
  int frame_count;
  std::string file_path;
};

struct EncodeFrameResult {
  std::vector<uint8_t> bitstream_buf;
  int global_coding_idx;
  int global_order_idx;
};

// DuckyEncode is an experimental encoder c++ interface for two-pass mode.
class DuckyEncode {
 public:
  explicit DuckyEncode(const VideoInfo &video_info);
  ~DuckyEncode();
  std::vector<FIRSTPASS_STATS> ComputeFirstPassStats();
  void StartEncode(const std::vector<FIRSTPASS_STATS> &stats_list);
  EncodeFrameResult EncodeFrame();
  void EndEncode();

 private:
  class EncodeImpl;
  std::unique_ptr<EncodeImpl> impl_ptr_;
};
}  // namespace aom

#endif  // AOM_AV1_DUCKY_ENCODE_H_
