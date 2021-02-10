/*
 *  Copyright (c) 2014 The WebM project authors. All Rights Reserved.
 *  Copyright (c) 2020 Intel Corporation
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VPX_VP9_ENCODER_VP9_SVC_LAYERCONTEXT_H_
#define VPX_VP9_ENCODER_VP9_SVC_LAYERCONTEXT_H_

#include "vp9_ratectrl.h"

typedef enum {
  // Inter-layer prediction is on on all frames.
  INTER_LAYER_PRED_ON,
  // Inter-layer prediction is off on all frames.
  INTER_LAYER_PRED_OFF,
  // Inter-layer prediction is off on non-key frames and non-sync frames.
  INTER_LAYER_PRED_OFF_NONKEY,
  // Inter-layer prediction is on on all frames, but constrained such
  // that any layer S (> 0) can only predict from previous spatial
  // layer S-1, from the same superframe.
  INTER_LAYER_PRED_ON_CONSTRAINED
} INTER_LAYER_PRED;

typedef struct BUFFER_LONGTERM_REF {
  int idx;
  int is_used;
} BUFFER_LONGTERM_REF;

typedef struct {
  RATE_CONTROL rc;
  int target_bandwidth;
  int spatial_layer_target_bandwidth;  // Target for the spatial layer.
  double framerate;
  int avg_frame_size;
  int max_q;
  int min_q;
  int scaling_factor_num;
  int scaling_factor_den;
  // Scaling factors used for internal resize scaling for single layer SVC.
  int scaling_factor_num_resize;
  int scaling_factor_den_resize;
  //TWO_PASS twopass;
  //vpx_fixed_buf_t rc_twopass_stats_in;
  unsigned int current_video_frame_in_layer;
  int is_key_frame;
  int frames_from_key_frame;
  FRAME_TYPE last_frame_type;
  int alt_ref_idx;
  int gold_ref_idx;
  int has_alt_frame;
  size_t layer_size;
  uint8_t speed;
} LAYER_CONTEXT;

typedef struct SVC {
  int spatial_layer_id;
  int temporal_layer_id;
  int number_spatial_layers;
  int number_temporal_layers;

  int spatial_layer_to_encode;

  // Workaround for multiple frame contexts
  enum { ENCODED = 0, ENCODING, NEED_TO_ENCODE } encode_empty_frame_state;

  // Layer context used for rate control in one pass temporal CBR mode or
  // two pass spatial mode.
  LAYER_CONTEXT layer_context[VPX_MAX_LAYERS];
  // Indicates what sort of temporal layering is used.
  // Currently, this only works for CBR mode.
  VP9E_TEMPORAL_LAYERING_MODE temporal_layering_mode;
  // Frame flags and buffer indexes for each spatial layer, set by the
  // application (external settings).
  // Sequence level flag to enable second (long term) temporal reference.
  int use_gf_temporal_ref;
  // Frame level flag to enable second (long term) temporal reference.
  int use_gf_temporal_ref_current_layer;

  int current_superframe;

  int first_layer_denoise;

  int skip_enhancement_layer;

  int lower_layer_qindex;

  int framedrop_thresh[VPX_MAX_LAYERS];
  int drop_count[VPX_MAX_LAYERS];

  SVC_LAYER_DROP_MODE framedrop_mode;

  INTER_LAYER_PRED disable_inter_layer_pred;

  // Flag to indicate scene change and high num of motion blocks at current
  // superframe, scene detection is currently checked for each superframe prior
  // to encoding, on the full resolution source.
  int high_source_sad_superframe;
  int high_num_blocks_with_motion;

  // Flags used to get SVC pattern info.
  int update_buffer_slot[VPX_SS_MAX_LAYERS];
  uint8_t reference_last[VPX_SS_MAX_LAYERS];
  uint8_t reference_golden[VPX_SS_MAX_LAYERS];
  uint8_t reference_altref[VPX_SS_MAX_LAYERS];
  // TODO(jianj): Remove these last 3, deprecated.
  uint8_t update_last[VPX_SS_MAX_LAYERS];
  uint8_t update_golden[VPX_SS_MAX_LAYERS];
  uint8_t update_altref[VPX_SS_MAX_LAYERS];

  int spatial_layer_sync[VPX_SS_MAX_LAYERS];
  uint8_t set_intra_only_frame;
  uint8_t previous_frame_is_intra_only;
  uint8_t superframe_has_layer_sync;

  int temporal_layer_id_per_spatial[VPX_SS_MAX_LAYERS];

  int first_spatial_layer_to_encode;

  int num_encoded_top_layer;

  // Flag to indicate SVC is dynamically switched to a single layer.
  int single_layer_svc;
} SVC;

struct VP9_COMP;

// Initialize layer context data from init_config().
void vp9_init_layer_context(struct VP9_COMP *const cpi);

// Update the layer context from a change_config() call.
void vp9_update_layer_context_change_config(struct VP9_COMP *const cpi,
                                            const int target_bandwidth);

// Prior to encoding the frame, update framerate-related quantities
// for the current temporal layer.
void vp9_update_temporal_layer_framerate(struct VP9_COMP *const cpi);

// Update framerate-related quantities for the current spatial layer.
void vp9_update_spatial_layer_framerate(struct VP9_COMP *const cpi,
                                        double framerate);

// Prior to encoding the frame, set the layer context, for the current layer
// to be encoded, to the cpi struct.
void vp9_restore_layer_context(struct VP9_COMP *const cpi);

// Save the layer context after encoding the frame.
void vp9_save_layer_context(struct VP9_COMP *const cpi);

void get_layer_resolution(const int width_org, const int height_org,
                          const int num, const int den, int *width_out,
                          int *height_out);

void vp9_svc_reset_temporal_layers(struct VP9_COMP *const cpi, int is_key);

void vp9_svc_check_reset_layer_rc_flag(struct VP9_COMP *cpi);

void vp9_svc_check_spatial_layer_sync(struct VP9_COMP *const cpi);

void vp9_svc_adjust_frame_rate(struct VP9_COMP *const cpi);

void vp9_svc_adjust_avg_frame_qindex(struct VP9_COMP *const cpi);

#endif  // VPX_VP9_ENCODER_VP9_SVC_LAYERCONTEXT_H_
