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

#ifndef LIBMEBO_BRC_VP9_RATECTRL_H
#define LIBMEBO_BRC_VP9_RATECTRL_H

#include <assert.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/*! Temporal Scalability: Maximum number of coding layers */
#define VPX_TS_MAX_LAYERS 5

/*! Temporal+Spatial Scalability: Maximum number of coding layers */
#define VPX_MAX_LAYERS 12  // 3 temporal + 4 spatial layers are allowed.

/*! Spatial Scalability: Maximum number of coding layers */
#define VPX_SS_MAX_LAYERS 5

#define REFS_PER_FRAME 3  
#define REF_FRAMES_LOG2 3
#define REF_FRAMES (1 << REF_FRAMES_LOG2)

// Max rate per frame for 1080P and below encodes if no level requirement given.
// For larger formats limit to MAX_MB_RATE bits per MB
// 4Mbits is derived from the level requirement for level 4 (1080P 30) which
// requires that HW can sustain a rate of 16Mbits over a 4 frame group.
// If a lower level requirement is specified then this may over ride this value.
#define MAX_MB_RATE 250
#define MAXRATE_1080P 4000000

#define MIN_BPB_FACTOR 0.005
#define MAX_BPB_FACTOR 50

#define MAXQ 255
#define QINDEX_RANGE 256
#define QINDEX_BITS 8

// Used to control aggressive VBR mode.
// #define AGGRESSIVE_VBR 1
// Bits Per MB at different Q (Multiplied by 512)
#define BPER_MB_NORMBITS 9
#define MIN_GF_INTERVAL 4
#define MAX_GF_INTERVAL 16
#define FIXED_GF_INTERVAL 8  // Used in some testing modes only

#define FRAME_OVERHEAD_BITS 200
// The maximum duration of a GF group that is static (for example a slide show).
#define MAX_STATIC_GF_GROUP_LENGTH 250
#define VP9_LEVELS 14

#define LAYER_IDS_TO_IDX(sl, tl, num_tl) ((sl) * (num_tl) + (tl))

#define VPXMIN(x, y) (((x) < (y)) ? (x) : (y))
#define VPXMAX(x, y) (((x) > (y)) ? (x) : (y))

typedef enum vpx_bit_depth {
  VPX_BITS_8 = 8,   /**<  8 bits */
  VPX_BITS_10 = 10, /**< 10 bits */
  VPX_BITS_12 = 12, /**< 12 bits */
} vpx_bit_depth_t;

typedef enum {
  VP9_LAST_FLAG = 1 << 0,
  VP9_GOLD_FLAG = 1 << 1,
  VP9_ALT_FLAG = 1 << 2,
} VP9_REFFRAME;

// Used to control aggressive VBR mode.
// #define AGGRESSIVE_VBR 1

// Bits Per MB at different Q (Multiplied by 512)
#define BPER_MB_NORMBITS 9

// The maximum duration of a GF group that is static (for example a slide show).
#define MAX_STATIC_GF_GROUP_LENGTH 250

#define VP9_LEVELS 14

typedef enum {
  STATUS_BRC_VP9_SUCCESS,
  STATUS_BRC_VP9_FAILED,
  STATUS_BRC_VP9_UNSUPPORTED_SPATIAL_SVC,
  STATUS_BRC_VP9_UNSUPPORTED_TEMPORAL_SVC,
  STATUS_BRC_VP9_INVALID_PARAM,
} VP9RateControlStatus;

typedef enum {
  LEVEL_UNKNOWN = 0,
  LEVEL_AUTO = 1,
  LEVEL_1 = 10,
  LEVEL_1_1 = 11,
  LEVEL_2 = 20,
  LEVEL_2_1 = 21,
  LEVEL_3 = 30,
  LEVEL_3_1 = 31,
  LEVEL_4 = 40,
  LEVEL_4_1 = 41,
  LEVEL_5 = 50,
  LEVEL_5_1 = 51,
  LEVEL_5_2 = 52,
  LEVEL_6 = 60,
  LEVEL_6_1 = 61,
  LEVEL_6_2 = 62,
  LEVEL_MAX = 255
} VP9_LEVEL;
typedef struct {
  VP9_LEVEL level;
  uint64_t max_luma_sample_rate;
  uint32_t max_luma_picture_size;
  uint32_t max_luma_picture_breadth;
  double average_bitrate;  // in kilobits per second
  double max_cpb_size;     // in kilobits
  double compression_ratio;
  uint8_t max_col_tiles;
  uint32_t min_altref_distance;
  uint8_t max_ref_frame_buffers;
} Vp9LevelSpec;

typedef struct {
  int8_t level_index;
  uint8_t fail_flag;
  int max_frame_size;   // in bits
  double max_cpb_size;  // in bits
} LevelConstraint;

typedef enum {
  INTER_NORMAL = 0,
  INTER_HIGH = 1,
  GF_ARF_LOW = 2,
  GF_ARF_STD = 3,
  KF_STD = 4,
  RATE_FACTOR_LEVELS = 5
} RATE_FACTOR_LEVEL;

// Scale dependent Rate Correction Factor multipliers. Compensates for the
// greater number of bits per pixel generated in down-scaled frames.
static const double rcf_mult[2] = { 1.0, 2.0 };

typedef enum {
  VP9E_CONTENT_DEFAULT,
  VP9E_CONTENT_SCREEN,
  VP9E_CONTENT_FILM,
  VP9E_CONTENT_INVALID
} vp9e_tune_content;

/*!\brief Rate control mode */
enum vpx_rc_mode {
  VPX_VBR, /**< Variable Bit Rate (VBR) mode */
  VPX_CBR, /**< Constant Bit Rate (CBR) mode */
  VPX_CQ,  /**< Constrained Quality (CQ)  mode */
  VPX_Q,   /**< Constant Quality (Q) mode */
};
typedef enum {
  NO_AQ = 0,
  VARIANCE_AQ = 1,
  COMPLEXITY_AQ = 2,
  CYCLIC_REFRESH_AQ = 3,
  EQUATOR360_AQ = 4,
  PERCEPTUAL_AQ = 5,
  PSNR_AQ = 6,
  // AQ based on lookahead temporal
  // variance (only valid for altref frames)
  LOOKAHEAD_AQ = 7,
  AQ_MODE_COUNT  // This should always be the last member of the enum
} AQ_MODE;
typedef enum {
  // Good Quality Fast Encoding. The encoder balances quality with the amount of
  // time it takes to encode the output. Speed setting controls how fast.
  GOOD,

  // The encoder places priority on the quality of the output over encoding
  // speed. The output is compressed at the highest possible quality. This
  // option takes the longest amount of time to encode. Speed setting ignored.
  BEST,

  // Realtime/Live Encoding. This mode is optimized for realtime encoding (for
  // example, capturing a television signal or feed from a live camera). Speed
  // setting controls how fast.
  REALTIME
} MODE;
typedef enum {
  FRAMEFLAGS_KEY = 1 << 0,
  FRAMEFLAGS_GOLDEN = 1 << 1,
  FRAMEFLAGS_ALTREF = 1 << 2,
} FRAMETYPE_FLAGS;


typedef enum {
  KEY_FRAME = 0,
  INTER_FRAME = 1,
  FRAME_TYPES,
} FRAME_TYPE;

// Bitstream profiles indicated by 2-3 bits in the uncompressed header.
// 00: Profile 0.  8-bit 4:2:0 only.
// 10: Profile 1.  8-bit 4:4:4, 4:2:2, and 4:4:0.
// 01: Profile 2.  10-bit and 12-bit color only, with 4:2:0 sampling.
// 110: Profile 3. 10-bit and 12-bit color only, with 4:2:2/4:4:4/4:4:0
//                 sampling.
// 111: Undefined profile.
typedef enum BITSTREAM_PROFILE {
  PROFILE_0,
  PROFILE_1,
  PROFILE_2,
  PROFILE_3,
  MAX_PROFILES
} BITSTREAM_PROFILE;

typedef enum vp9e_temporal_layering_mode {
  /*!\brief No temporal layering.
   * Used when only spatial layering is used.
   */
  VP9E_TEMPORAL_LAYERING_MODE_NOLAYERING = 0,

  /*!\brief Bypass mode.
   * Used when application needs to control temporal layering.
   * This will only work when the number of spatial layers equals 1.
   */
  VP9E_TEMPORAL_LAYERING_MODE_BYPASS = 1,

  /*!\brief 0-1-0-1... temporal layering scheme with two temporal layers.
   */
  VP9E_TEMPORAL_LAYERING_MODE_0101 = 2,

  /*!\brief 0-2-1-2... temporal layering scheme with three temporal layers.
   */
  VP9E_TEMPORAL_LAYERING_MODE_0212 = 3
} VP9E_TEMPORAL_LAYERING_MODE;

typedef struct {
  // Rate targetting variables
  int base_frame_target;  // A baseline frame target before adjustment
                          // for previous under or over shoot.
  int this_frame_target;  // Actual frame target after rc adjustment.
  int projected_frame_size;
  int sb64_target_rate;
  int last_q[FRAME_TYPES];  // Separate values for Intra/Inter
  int last_boosted_qindex;  // Last boosted GF/KF/ARF q
  int last_kf_qindex;       // Q index of the last key frame coded.

  int gfu_boost;
  int last_boost;
  int kf_boost;
  double rate_correction_factors[RATE_FACTOR_LEVELS];

  int frames_since_golden;
  int frames_till_gf_update_due;
  int min_gf_interval;
  int max_gf_interval;
  int static_scene_max_gf_interval;
  int baseline_gf_interval;
  int constrained_gf_group;
  int frames_to_key;
  int frames_since_key;
  int this_key_frame_forced;
  int next_key_frame_forced;
  int source_alt_ref_pending;
  int source_alt_ref_active;
  int is_src_frame_alt_ref;

  int avg_frame_bandwidth;  // Average frame size target for clip
  int min_frame_bandwidth;  // Minimum allocation used for any frame
  int max_frame_bandwidth;  // Maximum burst rate allowed for a frame.

  int ni_av_qi;
  int ni_tot_qi;
  int ni_frames;
  int avg_frame_qindex[FRAME_TYPES];
  double tot_q;
  double avg_q;

  int64_t buffer_level;
  int64_t bits_off_target;
  int64_t vbr_bits_off_target;
  int64_t vbr_bits_off_target_fast;

  int decimation_factor;
  int decimation_count;

  int rolling_target_bits;
  int rolling_actual_bits;

  int long_rolling_target_bits;
  int long_rolling_actual_bits;

  int rate_error_estimate;

  int64_t total_actual_bits;
  int64_t total_target_bits;
  int64_t total_target_vs_actual;

  int worst_quality;
  int best_quality;

  int64_t starting_buffer_level;
  int64_t optimal_buffer_level;
  int64_t maximum_buffer_size;

  // rate control history for last frame(1) and the frame before(2).
  // -1: undershot
  //  1: overshoot
  //  0: not initialized.
  int rc_1_frame;
  int rc_2_frame;
  int q_1_frame;
  int q_2_frame;
  // Keep track of the last target average frame bandwidth.
  int last_avg_frame_bandwidth;

  int fac_active_worst_inter;
  int fac_active_worst_gf;
  int alt_ref_gf_group;
  int last_frame_is_src_altref;
  int count_last_scene_change;
  int hybrid_intra_scene_change;
  int re_encode_maxq_scene_change;
  int avg_frame_low_motion;
  int af_ratio_onepass_vbr;
  int force_qpmin;
  double perc_arf_usage;
  int force_max_q;
  // Last frame was dropped post encode on scene change.
  int last_post_encode_dropped_scene_change;

  int damped_adjustment[RATE_FACTOR_LEVELS];
  double arf_active_best_quality_adjustment_factor;
  int arf_increase_active_best_quality;

  int preserve_arf_as_gld;
  int preserve_next_arf_as_gld;
  int show_arf_as_gld;
} RATE_CONTROL;

typedef struct VP9_COMP VP9_COMP;
typedef struct VP9EncoderConfig VP9EncoderConfig;
typedef struct VP9_COMMON VP9_COMMON;

int brc_libvpx_vp9_frame_is_intra_only(const VP9_COMMON *const cm);

int brc_libvpx_vp9_calc_iframe_target_size_one_pass_cbr(VP9_COMP *cpi);

int brc_libvpx_vp9_calc_pframe_target_size_one_pass_cbr(const VP9_COMP *cpi);

void brc_libvpx_vp9_rc_set_frame_target(VP9_COMP *cpi, int target);

void brc_libvpx_update_buffer_level_preencode(VP9_COMP *cpi);

int brc_libvpx_vp9_rc_pick_q_and_bounds(const VP9_COMP *cpi, int *bottom_index,
                             int *top_index);

int brc_libvpx_vp9_quantizer_to_qindex(int quantizer);

void brc_libvpx_vp9_set_rc_buffer_sizes(RATE_CONTROL *rc,
                                const VP9EncoderConfig *oxcf);

void brc_libvpx_vp9_check_reset_rc_flag(VP9_COMP *cpi);

void brc_libvpx_vp9_change_config(struct VP9_COMP *cpi, const VP9EncoderConfig *oxcf);

void brc_libvpx_vp9_rc_set_gf_interval_range(const VP9_COMP *const cpi,
                                  RATE_CONTROL *const rc);

void
brc_libvpx_vp9_new_framerate (VP9_COMP * cpi, double framerate);

void brc_libvpx_vp9_rc_init(const struct VP9EncoderConfig *oxcf, int pass,
                 RATE_CONTROL *rc);

void brc_libvpx_vp9_rc_init_minq_luts(void);

int brc_libvpx_is_one_pass_cbr_svc(const struct VP9_COMP *const cpi);

// Generally at the high level, the following flow is expected
// to be enforced for rate control:
// First call per frame, one of:
//   vp9_rc_get_one_pass_vbr_params()
//   vp9_rc_get_one_pass_cbr_params()
//   vp9_rc_get_svc_params()
//   vp9_rc_get_first_pass_params()
//   vp9_rc_get_second_pass_params()
// depending on the usage to set the rate control encode parameters desired.
//
// Then, call encode_frame_to_data_rate() to perform the
// actual encode. This function will in turn call encode_frame()
// one or more times, followed by one of:
//   vp9_rc_postencode_update()
//   vp9_rc_postencode_update_drop_frame()
//
// The majority of rate control parameters are only expected
// to be set in the vp9_rc_get_..._params() functions and
// updated during the vp9_rc_postencode_update...() functions.
// The only exceptions are vp9_rc_drop_frame() and
// vp9_rc_update_rate_correction_factors() functions.

void brc_libvpx_vp9_rc_get_svc_params(struct VP9_COMP *cpi);

// Post encode update of the rate control parameters based
// on bytes used
void brc_libvpx_vp9_rc_postencode_update(VP9_COMP *cpi, int64_t bytes_used);

void brc_libvpx_vp9_set_mb_mi(VP9_COMMON *cm, int width, int height);

int16_t brc_libvpx_vp9_ac_quant (int qindex, int delta, int bit_depth);
#endif  // LIBMEBO_BRC_VP9_RATECTRL_H
