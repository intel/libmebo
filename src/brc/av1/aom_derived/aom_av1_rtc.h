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

#include "aom_av1_common.h"
#include "../../../lib/libmebo.hpp"

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

void
brc_av1_rate_control_free (BrcCodecEnginePtr rtc);

LibMeboStatus
brc_av1_update_rate_control(BrcCodecEnginePtr rtc_api, LibMeboRateControllerConfig *rc_cfg);

LibMeboStatus
brc_av1_compute_qp (BrcCodecEnginePtr rtc_api, LibMeboRCFrameParams *frame_params);

// GetQP() needs to be called after ComputeQP() to get the latest QP
LibMeboStatus
brc_av1_get_qp(BrcCodecEnginePtr rtc_api, int *qp);

LibMeboStatus
brc_av1_get_loop_filter_level(BrcCodecEnginePtr rtc_api, int *lf);

// Feedback to rate control with the size of current encoded frame
LibMeboStatus
brc_av1_post_encode_update(BrcCodecEnginePtr rtc_api, uint64_t encoded_frame_size);

LibMeboStatus
brc_av1_rate_control_init (LibMeboRateControllerConfig *rc_cfg,
    BrcCodecEnginePtr *brc_codec_handler);

#endif  // LIBMEBO_AV1_RATECTRL_H
