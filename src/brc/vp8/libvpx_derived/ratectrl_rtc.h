/*
 *  Copyright (c) 2020 Intel corporation. All Rights Reserved.
 *  Copyright (c) 2020 Sreerenj Balachandran<sreerenj.balachandran@intel.com>
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef LIBMEBO_VP8_RATECTRL_RTC_H
#define LIBMEBO_VP8_RATECTRL_RTC_H

#include "vp8_common.h"

typedef struct _VP8RateControlRtcConfig {
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
  int max_quantizers[VP8_MAX_LAYERS];
  int min_quantizers[VP8_MAX_LAYERS];
  int scaling_factor_num[VP8_SS_MAX_LAYERS];
  int scaling_factor_den[VP8_SS_MAX_LAYERS];
  int layer_target_bitrate[VP8_MAX_LAYERS];
  int ts_rate_decimator[VP8_TS_MAX_LAYERS];
} VP8RateControlRtcConfig;

typedef struct _VP8FrameParamsQpRTC {
  VP8_FRAME_TYPE frame_type;
  int spatial_layer_id;
  int temporal_layer_id;
} VP8FrameParamsQpRTC;

// This interface allows using VP8 real-time rate control without initializing
// the encoder.
//
// #include "vp8/ratectrl_rtc.h"
// VP8RateControlRTC *rc_api;
// VP8RateControlRtcConfig cfg;
// VP8FrameParamsQpRTC frame_params;
//
// YourFunctionToInitializeConfig(cfg);
// rc_api = brc_vp8_rate_control_new (&cfg);
// // start encoding
// while (frame_to_encode) {
//   if (config_changed)
//     brc_vp8_update_rate_ontrol (rc_api, &cfg);
//   YourFunctionToFillFrameParams(frame_params);
//   brc_vp8_compute_qp(frame_params);
//   YourFunctionToUseQP(brc_vp8_get_qp(rc_api));
//   YourFunctionToUseLoopfilter(brc_get_loop_filter_evel(rc_api));
//   // After encoding
//   brc_vp8_post_encode_update(rc_api, encoded_frame_size);
// }


typedef struct _VP8RateControlRTC {
  VP8_COMP cpi_;
} VP8RateControlRTC;

VP8RateControlRTC * brc_vp8_rate_control_new (VP8RateControlRtcConfig *cfg);

void brc_vp8_rate_control_free (VP8RateControlRTC *rtc);

void brc_vp8_update_rate_control(VP8RateControlRTC *rtc_api, VP8RateControlRtcConfig *rc_cfg);

void brc_vp8_compute_qp (VP8RateControlRTC *rtc_api, VP8FrameParamsQpRTC *frame_params);

// GetQP() needs to be called after ComputeQP() to get the latest QP
int brc_vp8_get_qp(VP8RateControlRTC *rtc_api);

int brc_vp8_get_loop_filter_level(VP8RateControlRTC *rtc_api);

// Feedback to rate control with the size of current encoded frame
void brc_vp8_post_encode_update(VP8RateControlRTC *rtc_api, uint64_t encoded_frame_size);

VP8RateControlStatus brc_vp8_validate (VP8RateControlRtcConfig *cfg);

#endif  // LIBMEBO_VP8_RATECTRL_H
