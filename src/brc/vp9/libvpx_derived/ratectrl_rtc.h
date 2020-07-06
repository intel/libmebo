/*
 *  Copyright (c) 2020 The WebM project authors. All Rights Reserved.
 *  Copyright (c) 2020 Intel corporation. All Rights Reserved.
 *  Copyright (c) 2020 Sreerenj Balachandran<sreerenj.balachandran@intel.com>
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef LIBMEBO_VP9_RATECTRL_RTC_H
#define LIBMEBO_VP9_RATECTRL_RTC_H

#include "vp9_common.h"

typedef struct _VP9RateControlRtcConfig {
  int width;
  int height;
  // 0-63
  int max_quantizer;
  int min_quantizer;
  int64_t target_bandwidth;
  int64_t buf_initial_sz;
  int64_t buf_optimal_sz;
  int64_t buf_sz;
  int undershoot_pct;
  int overshoot_pct;
  int max_intra_bitrate_pct;
  double framerate;
  // Number of spatial layers
  int ss_number_layers;
  // Number of temporal layers
  int ts_number_layers;
  int max_quantizers[VPX_MAX_LAYERS];
  int min_quantizers[VPX_MAX_LAYERS];
  int scaling_factor_num[VPX_SS_MAX_LAYERS];
  int scaling_factor_den[VPX_SS_MAX_LAYERS];
  int layer_target_bitrate[VPX_MAX_LAYERS];
  int ts_rate_decimator[VPX_TS_MAX_LAYERS];
} VP9RateControlRtcConfig;

typedef struct _VP9FrameParamsQpRTC {
  FRAME_TYPE frame_type;
  int spatial_layer_id;
  int temporal_layer_id;
} VP9FrameParamsQpRTC;

// This interface allows using VP9 real-time rate control without initializing
// the encoder.
//
// #include "vp9/ratectrl_rtc.h"
// VP9RateControlRTC *rc_api;
// VP9RateControlRtcConfig cfg;
// VP9FrameParamsQpRTC frame_params;
//
// YourFunctionToInitializeConfig(cfg);
// rc_api = brc_vp9_rate_control_new (&cfg);
// // start encoding
// while (frame_to_encode) {
//   if (config_changed)
//     brc_vp9_update_rate_ontrol (rc_api, &cfg);
//   YourFunctionToFillFrameParams(frame_params);
//   brc_vp9_compute_qp(frame_params);
//   YourFunctionToUseQP(brc_vp9_get_qp(rc_api));
//   YourFunctionToUseLoopfilter(brc_get_loop_filter_evel(rc_api));
//   // After encoding
//   brc_vp9_post_encode_update(rc_api, encoded_frame_size);
// }


typedef struct _VP9RateControlRTC {
  VP9_COMP cpi_;
} VP9RateControlRTC;

VP9RateControlRTC * brc_vp9_rate_control_new (VP9RateControlRtcConfig *cfg);

void brc_vp9_rate_control_free (VP9RateControlRTC *rtc);

void brc_vp9_update_rate_control(VP9RateControlRTC *rtc_api, VP9RateControlRtcConfig *rc_cfg);

void brc_vp9_compute_qp (VP9RateControlRTC *rtc_api, VP9FrameParamsQpRTC *frame_params);

// GetQP() needs to be called after ComputeQP() to get the latest QP
int brc_vp9_get_qp(VP9RateControlRTC *rtc_api);

int brc_vp9_get_loop_filter_level(VP9RateControlRTC *rtc_api);

// Feedback to rate control with the size of current encoded frame
void brc_vp9_post_encode_update(VP9RateControlRTC *rtc_api, uint64_t encoded_frame_size);

VP9RateControlStatus brc_vp9_validate (VP9RateControlRtcConfig *cfg);

#endif  // LIBMEBO_VP9_RATECTRL_H
