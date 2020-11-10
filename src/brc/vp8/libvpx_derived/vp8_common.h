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
#define CONFIG_REALTIME_ONLY 1

#define AF_THRESH 25
#define AF_THRESH2 100
#define ARF_DECAY_THRESH 12

#define MIN_THRESHMULT 32
#define MAX_THRESHMULT 512

#define GF_ZEROMV_ZBIN_BOOST 12
#define LF_ZEROMV_ZBIN_BOOST 6
#define MV_ZBIN_BOOST 4
#define ZBIN_OQ_MAX 192

#define VP8_TEMPORAL_ALT_REF !CONFIG_REALTIME_ONLY

/* vp8 uses 10,000,000 ticks/second as time stamp */
#define TICKS_PER_SEC 10000000

#define VP8_MINQ 0
#define VP8_MAXQ 127
#define VP8_QINDEX_RANGE (VP8_MAXQ + 1)

/*! Temporal Scalability: Maximum length of the sequence defining frame
 * layer membership
 */
#define VPX_TS_MAX_PERIODICITY 16

/*! Temporal Scalability: Maximum number of coding layers */
#define VPX_TS_MAX_LAYERS 5

/*! Temporal+Spatial Scalability: Maximum number of coding layers */
#define VPX_MAX_LAYERS 12  // 3 temporal + 4 spatial layers are allowed.

/*! Spatial Scalability: Maximum number of coding layers */
#define VPX_SS_MAX_LAYERS 5

/*! Spatial Scalability: Default number of coding layers */
#define VPX_SS_DEFAULT_LAYERS 1

typedef enum { NORMAL_LOOPFILTER = 0, SIMPLE_LOOPFILTER = 1 } LOOPFILTERTYPE;

typedef enum {
  INTRA_FRAME = 0,
  LAST_FRAME = 1,
  GOLDEN_FRAME = 2,
  ALTREF_FRAME = 3,
  MAX_REF_FRAMES = 4
} MV_REFERENCE_FRAME;

typedef enum { VP8_KEY_FRAME = 0, VP8_INTER_FRAME = 1 } VP8_FRAME_TYPE;

typedef enum {
  VP8_FRAMEFLAGS_KEY = 1,
  VP8_FRAMEFLAGS_GOLDEN = 2,
  VP8_FRAMEFLAGS_ALTREF = 4
} VP8_FRAMETYPE_FLAGS;

typedef enum {
  USAGE_LOCAL_FILE_PLAYBACK = 0x0,
  USAGE_STREAM_FROM_SERVER = 0x1,
  USAGE_CONSTRAINED_QUALITY = 0x2,
  USAGE_CONSTANT_QUALITY = 0x3
} END_USAGE;

typedef enum {
  STATUS_BRC_VP8_SUCCESS,
  STATUS_BRC_VP8_FAILED,
  STATUS_BRC_VP8_UNSUPPORTED_SPATIAL_SVC,
  STATUS_BRC_VP8_UNSUPPORTED_TEMPORAL_SVC,
  STATUS_BRC_VP8_INVALID_PARAM,
} VP8RateControlStatus;

typedef enum vpx_ref_frame_type {
  VP8_LAST_FRAME = 1,
  VP8_GOLD_FRAME = 2,
  VP8_ALTR_FRAME = 4
} vpx_ref_frame_type_t;

typedef struct {
  int kf_indicated;
  unsigned int frames_since_key;
  unsigned int frames_since_golden;
  int filter_level;
  int frames_till_gf_update_due;
  int recent_ref_frame_usage[MAX_REF_FRAMES];

  int count_mb_ref_frame_usage[MAX_REF_FRAMES];

  int this_frame_percent_intra;
  int last_frame_percent_intra;

} CODING_CONTEXT;

typedef struct {
  double frame;
  double intra_error;
  double coded_error;
  double ssim_weighted_pred_err;
  double pcnt_inter;
  double pcnt_motion;
  double pcnt_second_ref;
  double pcnt_neutral;
  double MVr;
  double mvr_abs;
  double MVc;
  double mvc_abs;
  double MVrv;
  double MVcv;
  double mv_in_out_count;
  double new_mv_count;
  double duration;
  double count;
} FIRSTPASS_STATS;

typedef struct {
  int frames_so_far;
  double frame_intra_error;
  double frame_coded_error;
  double frame_pcnt_inter;
  double frame_pcnt_motion;
  double frame_mvr;
  double frame_mvr_abs;
  double frame_mvc;
  double frame_mvc_abs;

} ONEPASS_FRAMESTATS;

typedef enum {
  THR_ZERO1 = 0,
  THR_DC = 1,

  THR_NEAREST1 = 2,
  THR_NEAR1 = 3,

  THR_ZERO2 = 4,
  THR_NEAREST2 = 5,

  THR_ZERO3 = 6,
  THR_NEAREST3 = 7,

  THR_NEAR2 = 8,
  THR_NEAR3 = 9,

  THR_V_PRED = 10,
  THR_H_PRED = 11,
  THR_TM = 12,

  THR_NEW1 = 13,
  THR_NEW2 = 14,
  THR_NEW3 = 15,

  THR_SPLIT1 = 16,
  THR_SPLIT2 = 17,
  THR_SPLIT3 = 18,

  THR_B_PRED = 19
} THR_MODES;

typedef enum { DIAMOND = 0, NSTEP = 1, HEX = 2 } SEARCH_METHODS;

typedef struct {
  /* Layer configuration */
  double framerate;
  int target_bandwidth;

  /* Layer specific coding parameters */
  int64_t starting_buffer_level;
  int64_t optimal_buffer_level;
  int64_t maximum_buffer_size;
  int64_t starting_buffer_level_in_ms;
  int64_t optimal_buffer_level_in_ms;
  int64_t maximum_buffer_size_in_ms;

  int avg_frame_size_for_layer;

  int64_t buffer_level;
  int64_t bits_off_target;

  int64_t total_actual_bits;
  int total_target_vs_actual;

  int worst_quality;
  int active_worst_quality;
  int best_quality;
  int active_best_quality;

  int ni_av_qi;
  int ni_tot_qi;
  int ni_frames;
  int avg_frame_qindex;

  double rate_correction_factor;
  double key_frame_rate_correction_factor;
  double gf_rate_correction_factor;

  int zbin_over_quant;

  int inter_frame_target;
  int64_t total_byte_count;

  int filter_level;

  int frames_since_last_drop_overshoot;

  int force_maxqp;

  int last_frame_percent_intra;

  int count_mb_ref_frame_usage[MAX_REF_FRAMES];

  int last_q[2];
} VP8_LAYER_CONTEXT;

typedef struct VP8Common {
  VP8_FRAME_TYPE last_frame_type; /* Save last frame's frame type for motion search. */
  VP8_FRAME_TYPE frame_type;
  unsigned int current_video_frame;
  int frame_flags;

  int Width;
  int Height;
  int horiz_scale;
  int vert_scale;

  int show_frame;

  int MBs;
  int mb_rows;
  int mb_cols;

  int base_qindex;

  int y1dc_delta_q;
  int y2dc_delta_q;
  int y2ac_delta_q;
  int uvdc_delta_q;
  int uvac_delta_q;

  LOOPFILTERTYPE filter_type;

  //Fixme: Enable with dynamic lf strength finding algorithm
  // loop_filter_info_n lf_info;

  int filter_level;
  int last_sharpness_level;
  int sharpness_level;

  /* Fixme: Should we provide the application level control for this? */
  int refresh_last_frame;    /* Two state 0 = NO, 1 = YES */
  int refresh_golden_frame;  /* Two state 0 = NO, 1 = YES */
  int refresh_alt_ref_frame; /* Two state 0 = NO, 1 = YES */

} VP8_COMMON;

typedef struct {

  /*
   * DATARATE CONTROL OPTIONS
   */

  int end_usage; /* vbr or cbr */

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

  /* allow internal resizing */
  int allow_spatial_resampling;
  int resample_down_water_mark;
  int resample_up_water_mark;

  /* allow internal frame rate alterations */
  int allow_df;
  int drop_frames_water_mark;

  /* two pass datarate control */
  int two_pass_vbrbias;
  int two_pass_vbrmin_section;
  int two_pass_vbrmax_section;

  /*
   * END DATARATE CONTROL OPTIONS
   */

  /* Temporal scaling parameters */
  unsigned int number_of_layers;
  unsigned int target_bitrate[VPX_TS_MAX_PERIODICITY];
  unsigned int rate_decimator[VPX_TS_MAX_PERIODICITY];
  unsigned int periodicity;
  unsigned int layer_id[VPX_TS_MAX_PERIODICITY];

  unsigned int rc_max_intra_bitrate_pct;

  ////////
  //
  /* 4 versions of bitstream defined:
   *   0 best quality/slowest decode, 3 lowest quality/fastest decode
   */
  int Version;
  int Width;
  int Height;
  unsigned int target_bandwidth; /* kilobits per second */

  /* Parameter used for applying denoiser.
   * For temporal denoiser: noise_sensitivity = 0 means off,
   * noise_sensitivity = 1 means temporal denoiser on for Y channel only,
   * noise_sensitivity = 2 means temporal denoiser on for all channels.
   * noise_sensitivity = 3 means aggressive denoising mode.
   * noise_sensitivity >= 4 means adaptive denoising mode.
   * Temporal denoiser is enabled via the configuration option:
   * CONFIG_TEMPORAL_DENOISING.
  * For spatial denoiser: noise_sensitivity controls the amount of
   * pre-processing blur: noise_sensitivity = 0 means off.
   * Spatial denoiser invoked under !CONFIG_TEMPORAL_DENOISING.
   */
  int noise_sensitivity;

  /* parameter used for sharpening output: recommendation 0: */
  int Sharpness;
  int cpu_used;
  /* percent of rate boost for golden frame in CBR mode. */
  unsigned int gf_cbr_boost_pct;
  unsigned int screen_content_mode;
   /* mode ->
   *(0)=Realtime/Live Encoding. This mode is optimized for realtim
   *    encoding (for example, capturing a television signal or feed
   *    from a live camera). ( speed setting controls how fast )
   *(1)=Good Quality Fast Encoding. The encoder balances quality with
   *    the amount of time it takes to encode the output. ( speed
   *    setting controls how fast )
   *(2)=One Pass - Best Quality. The encoder places priority on the
   *    quality of the output over encoding speed. The output is
   *    compressed at the highest possible quality. This option takes
   *    the longest amount of time to encode. ( speed setting ignored
   *    )
   *(3)=Two Pass - First Pass. The encoder generates a file of
   *    statistics for use in the second encoding pass. ( speed
   *    setting controls how fast )
   *(4)=Two Pass - Second Pass. The encoder uses the statistics that
   *    were generated in the first encoding pass to create the
   *    compressed output. ( speed setting controls how fast )
   *(5)=Two Pass - Second Pass Best.  The encoder uses the statistics
   *    compressed output using the highest possible quality, and
   *    taking a longer amount of time to encode.. ( speed setting
   *    ignored )
   */
  int Mode;
  /* Key Framing Operations */
  int auto_key; /* automatically detect cut scenes */
  int key_freq; /* maximum distance to key frame. */

  /* lagged compression (if allow_lag == 0 lag_in_frames is ignored) */
  int allow_lag;
  int lag_in_frames; /* how many frames lag before we start encoding */

  /* these parameters aren't to be used in final build don't use!!! */
  int play_alternate;
  int alt_freq;
  int alt_q;
  int key_q;
  int gold_q;

  int multi_threaded;   /* how many threads to run the encoder on */
  int token_partitions; /* how many token partitions to create */

  /* early breakout threshold: for video conf recommend 800 */
  int encode_breakout;

  /* Bitfield defining the error resiliency features to enable.
   * Can provide decodable frames after losses in previous
   * frames and decodable partitions after losses in the same frame.
   */
  unsigned int error_resilient_mode;

  int arnr_max_frames;
  int arnr_strength;
  int arnr_type;

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
  int64_t last_time_stamp_seen;
  int64_t last_end_time_stamp_seen;
  int64_t first_time_stamp_ever;

  unsigned int frames_since_key;
  unsigned int key_frame_frequency;
  unsigned int this_key_frame_forced;
  unsigned int next_key_frame_forced;

  int this_frame_target;
  int projected_frame_size;
  int last_q[2]; /* Separate values for Intra/Inter */
 
  unsigned int frames_till_alt_ref_frame;
  /* frame in src_buffers has been identified to be encoded as an alt ref */
  int source_alt_ref_pending;
  /* an alt ref frame has been encoded and is usable */
  int source_alt_ref_active;
  /* source of frame to encode is an exact copy of an alt ref frame */
  int is_src_frame_alt_ref;

  /* golden frame same as last frame ( short circuit gold searches) */
  int gold_is_last;
  /* Alt reference frame same as last ( short circuit altref search) */
  int alt_is_last;
  /* don't do both alt and gold search ( just do gold). */
  int gold_is_alt;

  CODING_CONTEXT coding_context;

  /* Rate targeting variables */
  int64_t last_prediction_error;
  int64_t last_intra_error;

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

  /* for real time encoding */
  int avg_encode_time;    /* microsecond */
  int avg_pick_mode_time; /* microsecond */
  int Speed;
  int compressor_speed;

  int auto_gold;
  int auto_adjust_gold_quantizer;
  int auto_worst_q;
  int cpu_used;
  int pass;

  int prob_intra_coded;
  int prob_last_coded;
  int prob_gf_coded;
  int prob_skip_false;
  int last_skip_false_probs[3];
  int last_skip_probs_q[3];
  int recent_ref_frame_usage[MAX_REF_FRAMES];

  int this_frame_percent_intra;
  int last_frame_percent_intra;

  int ref_frame_flags;

  //SPEED_FEATURES sf;

  /* Count ZEROMV on all reference frames. */
  int zeromv_count;
  int lf_zeromv_pct;

  // Frame counter for the temporal pattern. Counter is rest when the temporal
  // layers are changed dynamically (run-time change).
  unsigned int temporal_pattern_counter;
  // Temporal layer id.
  int temporal_layer_id;

  int force_maxqp;
  int frames_since_last_drop_overshoot;
  int last_pred_err_mb;

  // GF update for 1 pass cbr.
  int gf_update_onepass_cbr;
  int gf_interval_onepass_cbr;
  int gf_noboost_onepass_cbr;

  struct twopass_rc {
    unsigned int section_intra_rating;
    double section_max_qfactor;
    unsigned int next_iiratio;
    unsigned int this_iiratio;
    FIRSTPASS_STATS total_stats;
    FIRSTPASS_STATS this_frame_stats;
    FIRSTPASS_STATS *stats_in, *stats_in_end, *stats_in_start;
    FIRSTPASS_STATS total_left_stats;
    int first_pass_done;
    int64_t bits_left;
    int64_t clip_bits_total;
    double avg_iiratio;
    double modified_error_total;
    double modified_error_used;
    double modified_error_left;
    double kf_intra_err_min;
    double gf_intra_err_min;
    int frames_to_key;
    int maxq_max_limit;
    int maxq_min_limit;
    int gf_decay_rate;
    int static_scene_max_gf_interval;
    int kf_bits;
    /* Remaining error from uncoded frames in a gf group. */
    int gf_group_error_left;
    /* Projected total bits available for a key frame group of frames */
    int64_t kf_group_bits;
    /* Error score of frames still to be coded in kf group */
    int64_t kf_group_error_left;
    /* Projected Bits available for a group including 1 GF or ARF */
    int64_t gf_group_bits;
    /* Bits for the golden frame or ARF */
    int gf_bits;
    int alt_extra_bits;
    double est_max_qcorrection_factor;
  } twopass;

  int b_calculate_psnr;

  /* Per MB activity measurement */
  unsigned int activity_avg;
  unsigned int *mb_activity_map;

  /* Record of which MBs still refer to last golden frame either
   * directly or through 0,0
   */
  unsigned char *gf_active_flags;
  int gf_active_count;

  int output_partition;

  /* Store last frame's MV info for next frame MV prediction */
  int *lf_ref_frame_sign_bias;
  int *lf_ref_frame;

  /* force next frame to intra when kf_auto says so */
  int force_next_frame_intra;

  int droppable;

  int initial_width;
  int initial_height;

  /* Coding layer state variables */
  unsigned int current_layer;
  VP8_LAYER_CONTEXT layer_context[VPX_TS_MAX_LAYERS];

  int64_t frames_in_layer[VPX_TS_MAX_LAYERS];
  int64_t bytes_in_layer[VPX_TS_MAX_LAYERS];
  double sum_psnr[VPX_TS_MAX_LAYERS];
  double sum_psnr_p[VPX_TS_MAX_LAYERS];
  double total_error2[VPX_TS_MAX_LAYERS];
  double total_error2_p[VPX_TS_MAX_LAYERS];
  double sum_ssim[VPX_TS_MAX_LAYERS];
  double sum_weights[VPX_TS_MAX_LAYERS];

  double total_ssimg_y_in_layer[VPX_TS_MAX_LAYERS];
  double total_ssimg_u_in_layer[VPX_TS_MAX_LAYERS];
  double total_ssimg_v_in_layer[VPX_TS_MAX_LAYERS];
  double total_ssimg_all_in_layer[VPX_TS_MAX_LAYERS];

  /* The frame number of each reference frames */
  unsigned int current_ref_frames[MAX_REF_FRAMES];
  // Closest reference frame to current frame.
  MV_REFERENCE_FRAME closest_reference_frame;

  /* Video conferencing cyclic refresh mode flags. This is a mode
   * designed to clean up the background over time in live encoding
   * scenarious. It uses segmentation.
   */
  int cyclic_refresh_mode_enabled;
  int cyclic_refresh_mode_max_mbs_perframe;
  int cyclic_refresh_mode_index;
  int cyclic_refresh_q;


} VP8_COMP;
#endif
