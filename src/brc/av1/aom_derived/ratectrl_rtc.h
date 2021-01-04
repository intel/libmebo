/*
 *  Copyright (c) 2020 Intel corporation. All Rights Reserved.
 *  Copyright (c) 2020 Sreerenj Balachandran<sreerenj.balachandran@intel.com>
 *
 *  Author: Sreerenj Balachandran<sreerenj.balachandran@intel.com>
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef LIBMEBO_AV1_RATECTRL_RTC_H
#define LIBMEBO_AV1_RATECTRL_RTC_H

#include "av1_common.h"

typedef struct _AV1RateControlRtcConfig {
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
  int max_inter_bitrate_pct;
  double framerate;
  // Number of spatial layers
  int ss_number_layers;
  // Number of temporal layers
  int ts_number_layers;
  int max_quantizers[AOM_MAX_LAYERS];
  int min_quantizers[AOM_MAX_LAYERS];
  int scaling_factor_num[AOM_MAX_SS_LAYERS];
  int scaling_factor_den[AOM_MAX_SS_LAYERS];
  int layer_target_bitrate[AOM_MAX_LAYERS];
  int ts_rate_decimator[AOM_MAX_TS_LAYERS];
} AV1RateControlRtcConfig;

typedef struct _AV1FrameParamsQpRTC {
  AV1_FRAME_TYPE frame_type;
  int spatial_layer_id;
  int temporal_layer_id;
} AV1FrameParamsQpRTC;

// This interface allows using AV1 real-time rate control without initializing
// the encoder.
//
// #include "av1/ratectrl_rtc.h"
// AV1RateControlRTC *rc_api;
// AV1RateControlRtcConfig cfg;
// AV1FrameParamsQpRTC frame_params;
//
// YourFunctionToInitializeConfig(cfg);
// rc_api = brc_av1_rate_control_new (&cfg);
// // start encoding
// while (frame_to_encode) {
//   if (config_changed)
//     brc_av1_update_rate_ontrol (rc_api, &cfg);
//   YourFunctionToFillFrameParams(frame_params);
//   brc_av1_compute_qp(frame_params);
//   YourFunctionToUseQP(brc_av1_get_qp(rc_api));
//   YourFunctionToUseLoopfilter(brc_get_loop_filter_evel(rc_api));
//   // After encoding
//   brc_av1_post_encode_update(rc_api, encoded_frame_size);
// }


typedef struct _AV1RateControlRTC {
  AV1_COMP cpi_;
} AV1RateControlRTC;

AV1RateControlRTC * brc_av1_rate_control_new (AV1RateControlRtcConfig *cfg);

void brc_av1_rate_control_free (AV1RateControlRTC *rtc);

void brc_av1_update_rate_control(AV1RateControlRTC *rtc_api, AV1RateControlRtcConfig *rc_cfg);

void brc_av1_compute_qp (AV1RateControlRTC *rtc_api, AV1FrameParamsQpRTC *frame_params);

// GetQP() needs to be called after ComputeQP() to get the latest QP
int brc_av1_get_qp(AV1RateControlRTC *rtc_api);

int brc_av1_get_loop_filter_level(AV1RateControlRTC *rtc_api);

// Feedback to rate control with the size of current encoded frame
void brc_av1_post_encode_update(AV1RateControlRTC *rtc_api, uint64_t encoded_frame_size);

AV1RateControlStatus brc_av1_validate (AV1RateControlRtcConfig *cfg);

#endif  // LIBMEBO_AV1_RATECTRL_H
