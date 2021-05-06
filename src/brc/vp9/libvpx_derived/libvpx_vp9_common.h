/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *  Copyright (c) 2020 Intel Corporation
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VPX_VP9_ENCODER_VP9_ENCODER_H_
#define VPX_VP9_ENCODER_VP9_ENCODER_H_

#include <stdio.h>
#include "libvpx_vp9_ratectrl.h"
#include "libvpx_vp9_svc_layercontext.h"
#include "libvpx_vp9_picklpf.h"

#define INVALID_IDX (-1)  // Invalid buffer index.

typedef struct VP9EncoderConfig {
  // maximum allowed bitrate for any intra frame in % of bitrate target.
  unsigned int rc_max_intra_bitrate_pct;
  // maximum allowed bitrate for any inter frame in % of bitrate target.
  unsigned int rc_max_inter_bitrate_pct;
  vp9e_tune_content content; //NotReq
  // Frame drop threshold.
  int drop_frames_water_mark; //NotReq
  enum vpx_rc_mode rc_mode;
  vpx_bit_depth_t bit_depth;

  int width;                     // width of data passed to the compressor
  int height;                    // height of data passed to the compressor

  double init_framerate;         // set to passed in framerate

  MODE mode;
  int pass;

  int worst_allowed_q;
  int best_allowed_q;
  AQ_MODE aq_mode;  // Adaptive Quantization mode

  // Internal frame size scaling.
  RESIZE_TYPE resize_mode;
  int scaled_frame_width;
  int scaled_frame_height;

  // Key Framing Operations
  int auto_key;  // autodetect cut scenes and set the keyframes
  int key_freq;  // maximum distance to key frame.

  int lag_in_frames;  // how many frames lag before we start encoding

  int enable_auto_arf;
  // buffer targeting aggressiveness
  int under_shoot_pct;
  int over_shoot_pct;

  int min_gf_interval;
  int max_gf_interval;

  unsigned int target_level;

  int64_t target_bandwidth;  // bandwidth to be used in bits per second

  // two pass datarate control
  int two_pass_vbrbias;  // two pass datarate control tweaks
  int two_pass_vbrmin_section;
  int two_pass_vbrmax_section;
  int vbr_corpus_complexity;  // 0 indicates corpus vbr disabled

  // Spatial and temporal scalability.
  int ss_number_layers;  // Number of spatial layers.
  int ts_number_layers;  // Number of temporal layers.
  // Bitrate allocation for spatial layers.
  int layer_target_bitrate[VPX_MAX_LAYERS];
  int ss_target_bitrate[VPX_SS_MAX_LAYERS];
  int ss_enable_auto_arf[VPX_SS_MAX_LAYERS];
  // Bitrate allocation (CBR mode) and framerate factor, for temporal layers.
  int ts_rate_decimator[VPX_TS_MAX_LAYERS];

  VP9E_TEMPORAL_LAYERING_MODE temporal_layering_mode;

  int noise_sensitivity;  // pre processing blur: recommendation 0
  int speed;
  // buffering parameters
  int64_t starting_buffer_level_ms;
  int64_t optimal_buffer_level_ms;
  int64_t maximum_buffer_size_ms;

} VP9EncoderConfig;

typedef struct VP9_COMMON {
  int width;
  int height;

  // Flag signaling that the frame is encoded using only INTRA modes.
  uint8_t intra_only;

  FRAME_TYPE frame_type;

  int show_frame;

  int base_qindex;

  // MBs, mb_rows/cols is in 16-pixel units; mi_rows/cols is in
  // MODE_INFO (8-pixel) units.
  int MBs;
  int mb_rows, mi_rows;
  int mb_cols, mi_cols;
  int mi_stride;

  // VPX_BITS_8 in profile 0 or 1, VPX_BITS_10 or VPX_BITS_12 in profile 2 or 3.
  vpx_bit_depth_t bit_depth;

  unsigned int current_video_frame;

  BITSTREAM_PROFILE profile;

  //LoopFilter params
  struct loopfilter lf;
} VP9_COMMON;

typedef struct SPEED_FEATURES {
  // This flag controls the use of non-RD mode decision.
  int use_nonrd_pick_mode;
} SPEED_FEATURES;

typedef struct VP9_COMP {
  VP9_COMMON common;
  VP9EncoderConfig oxcf;
  RATE_CONTROL rc;

  LevelConstraint level_constraint;
  unsigned int target_level;

  int refresh_last_frame;
  int refresh_golden_frame;
  int refresh_alt_ref_frame;

  // If the last frame is dropped, we don't copy partition.
  uint8_t last_frame_dropped;

  int ext_refresh_frame_flags_pending;

  uint8_t *count_arf_frame_usage;
  uint8_t *count_lastgolden_frame_usage;

  double framerate;
  int frame_flags;

  int lst_fb_idx;
  int gld_fb_idx;
  int alt_fb_idx;

  int use_svc;
  SVC svc;

  SPEED_FEATURES sf;

  int external_resize;
} VP9_COMP;
#endif
