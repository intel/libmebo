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

#include "libvpx_vp9_common.h"
#include "../../../lib/libmebo.h"

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
// LibMeboRateControllerConfig cfg;
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

VP9RateControlRTC * brc_vp9_rate_control_new (LibMeboRateControllerConfig *cfg);

void
brc_vp9_rate_control_free (BrcCodecEnginePtr rtc);

LibMeboStatus
brc_vp9_update_rate_control(BrcCodecEnginePtr rtc_api, LibMeboRateControllerConfig *rc_cfg);

LibMeboStatus
brc_vp9_compute_qp (BrcCodecEnginePtr rtc_api, LibMeboRCFrameParams *frame_params);

// GetQP() needs to be called after ComputeQP() to get the latest QP
LibMeboStatus
brc_vp9_get_qp(BrcCodecEnginePtr rtc_api, int *qp);

LibMeboStatus
brc_vp9_get_loop_filter_level(BrcCodecEnginePtr rtc_api, int *lf);

// Feedback to rate control with the size of current encoded frame
LibMeboStatus
brc_vp9_post_encode_update(BrcCodecEnginePtr rtc_api, uint64_t encoded_frame_size);

LibMeboStatus
brc_vp9_rate_control_init (LibMeboRateControllerConfig *rc_cfg,
    BrcCodecEnginePtr *brc_codec_handler);

#endif  // LIBMEBO_VP9_RATECTRL_H
