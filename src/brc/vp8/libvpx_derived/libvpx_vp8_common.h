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

#ifndef VPX_ENCODER_VP8_ENCODER_H_
#define VPX_ENCODER_VP8_ENCODER_H_

#include <assert.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define MIN_GF_INTERVAL 4
#define DEFAULT_GF_INTERVAL 7

#define KEY_FRAME_CONTEXT 5

#define VP8_MINQ 0
#define VP8_MAXQ 127
#define VP8_QINDEX_RANGE (VP8_MAXQ + 1)

/*! Temporal Scalability: Maximum length of the sequence defining frame
 * layer membership
 */
#define VP8_TS_MAX_PERIODICITY 16

/*! Temporal Scalability: Maximum number of coding layers */
#define VP8_TS_MAX_LAYERS 1

/*! Temporal+Spatial Scalability: Maximum number of coding layers */
#define VP8_MAX_LAYERS 1  // 3 temporal + 4 spatial layers are allowed.

/*! Spatial Scalability: Maximum number of coding layers */
#define VP8_SS_MAX_LAYERS 1

/*! Spatial Scalability: Default number of coding layers */
#define VP8_SS_DEFAULT_LAYERS 1

typedef enum {
  INTRA_FRAME = 0,
  LAST_FRAME = 1,
  GOLDEN_FRAME = 2,
  ALTREF_FRAME = 3,
  MAX_REF_FRAMES = 4
} MV_REFERENCE_FRAME;

typedef enum { VP8_KEY_FRAME = 0, VP8_INTER_FRAME = 1 } VP8_FRAME_TYPE;

typedef enum {
  STATUS_BRC_VP8_SUCCESS,
  STATUS_BRC_VP8_FAILED,
  STATUS_BRC_VP8_UNSUPPORTED_SPATIAL_SVC,
  STATUS_BRC_VP8_UNSUPPORTED_TEMPORAL_SVC,
  STATUS_BRC_VP8_INVALID_PARAM,
} VP8RateControlStatus;

typedef struct VP8Common {
  VP8_FRAME_TYPE last_frame_type; /* Save last frame's frame type for motion search. */
  VP8_FRAME_TYPE frame_type;
  unsigned int current_video_frame;
  int frame_flags;

  int Width;
  int Height;

  int show_frame;

  int MBs;
  int mb_rows;
  int mb_cols;

  int base_qindex;

  int filter_level;

  /* Fixme: Should we provide the application level control for this? */
  int refresh_last_frame;    /* Two state 0 = NO, 1 = YES */
  int refresh_golden_frame;  /* Two state 0 = NO, 1 = YES */
  int refresh_alt_ref_frame; /* Two state 0 = NO, 1 = YES */

} VP8_COMMON;

typedef struct {
  /* buffer targeting aggressiveness */
  int under_shoot_pct;
  int over_shoot_pct;

  /* buffering parameters */
  int64_t starting_buffer_level;
  int64_t optimal_buffer_level;
  int64_t maximum_buffer_size;

  int64_t starting_buffer_level_in_ms;
  int64_t optimal_buffer_level_in_ms;
  int64_t maximum_buffer_size_in_ms;

  /* controlling quality */
  int fixed_q;
  int worst_allowed_q;
  int best_allowed_q;
  int cq_level;

  /* two pass datarate control */
  int two_pass_vbrbias;
  int two_pass_vbrmin_section;
  int two_pass_vbrmax_section;

  /* Temporal scaling parameters */
  unsigned int number_of_layers;
  unsigned int target_bitrate[VP8_TS_MAX_PERIODICITY];
  unsigned int rate_decimator[VP8_TS_MAX_PERIODICITY];
  unsigned int periodicity;
  unsigned int layer_id[VP8_TS_MAX_PERIODICITY];

  unsigned int rc_max_intra_bitrate_pct;

  int Width;
  int Height;
  unsigned int target_bandwidth; /* kilobits per second */

  /* percent of rate boost for golden frame in CBR mode. */
  unsigned int gf_cbr_boost_pct;
  unsigned int screen_content_mode;

  /* maximum distance to key frame. */
  int key_freq;

} VP8_CONFIG;

typedef struct VP8_COMP {
  VP8_COMMON common;
  VP8_CONFIG oxcf;

  int ni_av_qi;
  int ni_tot_qi;
  int ni_frames;
  int avg_frame_qindex;

  int64_t key_frame_count;
  int prior_key_frame_distance[KEY_FRAME_CONTEXT];
  /* Current section per frame bandwidth target */
  int per_frame_bandwidth;
  /* Average frame size target for clip */
  int av_per_frame_bandwidth;
  /* Minimum allocation that should be used for any frame */
  int min_frame_bandwidth;
  int inter_frame_target;
  double output_framerate;

  unsigned int frames_since_key;
  unsigned int key_frame_frequency;

  int this_frame_target;
  int projected_frame_size;
  int last_q[2]; /* Separate values for Intra/Inter */
 
  unsigned int frames_till_alt_ref_frame;
  /* an alt ref frame has been encoded and is usable */
  int source_alt_ref_active;

  double rate_correction_factor;
  double key_frame_rate_correction_factor;
  double gf_rate_correction_factor;

  int frames_since_golden;
  /* Count down till next GF */
  int frames_till_gf_update_due;

  /* GF interval chosen when we coded the last GF */
  int current_gf_interval;

  /* Total bits overspent becasue of GF boost (cumulative) */
  int gf_overspend_bits;

  /* Used in the few frames following a GF to recover the extra bits
   * spent in that GF
   */
  int non_gf_bitrate_adjustment;

  /* Extra bits spent on key frames that need to be recovered */
  int kf_overspend_bits;

  /* Current number of bit s to try and recover on each inter frame. */
  int kf_bitrate_adjustment;
  int max_gf_interval;
  int baseline_gf_interval;
  int active_arnr_frames;

  int64_t total_byte_count;

  int buffered_mode;

  double framerate;
  double ref_framerate;
  int64_t buffer_level;
  int64_t bits_off_target;

  int rolling_target_bits;
  int rolling_actual_bits;

  int long_rolling_target_bits;
  int long_rolling_actual_bits;

  int64_t total_actual_bits;
  int total_target_vs_actual; /* debug stats */

  int worst_quality;
  int active_worst_quality;
  int best_quality;
  int active_best_quality;

  int cq_target_quality;

  int drop_frames_allowed; /* Are we permitted to drop frames? */
  int drop_frame;          /* Drop this frame? */

  int gfu_boost;
  int kf_boost;
  int last_boost;

  int target_bandwidth;

  int decimation_factor;
  int decimation_count;

  int compressor_speed;

  int auto_gold;
  int auto_adjust_gold_quantizer;
  int auto_worst_q;
  int pass;

  int recent_ref_frame_usage[MAX_REF_FRAMES];

  int this_frame_percent_intra;
  int last_frame_percent_intra;

  int ref_frame_flags;

  /* Count ZEROMV on all reference frames. */
  int zeromv_count;

  int frames_since_last_drop_overshoot;
  int last_pred_err_mb;

  // GF update for 1 pass cbr.
  int gf_update_onepass_cbr;
  int gf_interval_onepass_cbr;
  int gf_noboost_onepass_cbr;

  /* Record of which MBs still refer to last golden frame either
   * directly or through 0,0
   */
  int gf_active_count;

  int initial_width;
  int initial_height;

} VP8_COMP;
#endif
