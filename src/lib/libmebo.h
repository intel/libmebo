/*
 *  Copyright (c) 2020 Intel Corporation. All Rights Reserved.
 *  Copyright (c) 2020 Sreerenj Balachandran <sreerenj.balachandran@intel.com>
 *  Author: Sreerenj Balachandran <sreerenj.balachandran@intel.com>
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef __LIBMEBO_H__
#define __LIBMEBO_H__

#include <assert.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

typedef enum {
 LIBMEBO_STATUS_SUCCESS,
 LIBMEBO_STATUS_FAILED,
 LIBMEBO_STATUS_INVALID_PARAM,
 LIBMEBO_STATUS_UNSUPPORTED_CODEC,

} LibMeboStatus;

typedef enum {  
  LIBMEBO_CODEC_VP8,
  LIBMEBO_CODEC_VP9,
  LIBMEBO_CODEC_AV1,
} LibMeboCodecType;

typedef enum {
  LIBMEBO_RC_CQP,
  LIBMEBO_RC_CBR,
  LIBMEBO_RC_VBR
} LibMeboRateControlMode;

typedef enum {
  LIBMEBO_KEY_FRAME = 0,
  LIBMEBO_INTER_FRAME = 1,
  LIBMEBO_FRAME_TYPES,
} LibMeboFrameType;

typedef struct _LibMeboRCFrameParams {
  LibMeboFrameType frame_type;
  int spatial_layer_id;
  int temporal_layer_id;
} LibMeboRCFrameParams;

/* Temporal Scalability: Maximum number of coding layers */
#define LIBMEBO_TS_MAX_LAYERS 5

/* Spatial Scalability: Maximum number of Spatial coding layers */
#define LIBMEBO_SS_MAX_LAYERS 5

/* Temporal+Spatial Scalability: Maximum number of coding layers */
#define LIBMEBO_MAX_LAYERS 12  // 3 temporal + 4 spatial layers are allowed.

typedef struct _LibMeboRateControllerConfig {
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
  int max_quantizers[LIBMEBO_MAX_LAYERS];
  int min_quantizers[LIBMEBO_MAX_LAYERS];
  int scaling_factor_num[LIBMEBO_SS_MAX_LAYERS];
  int scaling_factor_den[LIBMEBO_SS_MAX_LAYERS];
  int layer_target_bitrate[LIBMEBO_MAX_LAYERS];
  int ts_rate_decimator[LIBMEBO_TS_MAX_LAYERS];

} LibMeboRateControllerConfig;

typedef struct _LibMeboRateController {
  void *priv;

  LibMeboCodecType codec_type;
  LibMeboRateControllerConfig rc_config;

  /* Reserved bytes for future use, must be zero */
  uint32_t _libmebo_reserved[32];
} LibMeboRateController;

LibMeboRateController * libmebo_rate_controller_new (LibMeboCodecType codec_type);

LibMeboStatus libmebo_rate_controller_init (LibMeboRateController *rc,
                                            LibMeboRateControllerConfig *rc_config);

void libmebo_rate_controller_free (LibMeboRateController *rc);

LibMeboStatus libmebo_rate_controller_update_config (LibMeboRateController *rc,
                                                     LibMeboRateControllerConfig*rc_cfg);

void libmebo_rate_controller_update_frame_size (LibMeboRateController *rc,
                                                uint64_t encoded_frame_size);

void libmebo_rate_controller_compute_qp (LibMeboRateController *rc,
                                         LibMeboRCFrameParams rc_frame_params);

int libmebo_rate_controller_get_qp(LibMeboRateController *rc);

int libmebo_rate_contoller_get_loop_filter_level(LibMeboRateController *rc);

#endif
