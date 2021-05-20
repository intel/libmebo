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

#include "libvpx_vp9_ratectrl.h"

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

  // Layer context used for rate control in one pass temporal CBR mode or
  // two pass spatial mode.
  LAYER_CONTEXT layer_context[VPX_MAX_LAYERS];
  // Indicates what sort of temporal layering is used.
  // Currently, this only works for CBR mode.
  VP9E_TEMPORAL_LAYERING_MODE temporal_layering_mode;

  int lower_layer_qindex;

  int spatial_layer_sync[VPX_SS_MAX_LAYERS];
  uint8_t set_intra_only_frame;
  uint8_t previous_frame_is_intra_only;
  uint8_t superframe_has_layer_sync;
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

void vp9_svc_adjust_avg_frame_qindex(struct VP9_COMP *const cpi);

#endif  // VPX_VP9_ENCODER_VP9_SVC_LAYERCONTEXT_H_
