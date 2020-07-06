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

#include <math.h>

#include "vp9_svc_layercontext.h"
#include "vp9_common.h"

#define SMALL_FRAME_WIDTH 32
#define SMALL_FRAME_HEIGHT 16

void vp9_init_layer_context(VP9_COMP *const cpi) {
  SVC *const svc = &cpi->svc;
  const VP9EncoderConfig *const oxcf = &cpi->oxcf;
  int sl, tl, i;
  int alt_ref_idx = svc->number_spatial_layers;

  svc->spatial_layer_id = 0;
  svc->temporal_layer_id = 0;
  svc->force_zero_mode_spatial_ref = 0;
  svc->use_base_mv = 0;
  svc->use_partition_reuse = 0;
  svc->use_gf_temporal_ref = 1;
  svc->use_gf_temporal_ref_current_layer = 0;
  svc->scaled_temp_is_alloc = 0;
  svc->scaled_one_half = 0;
  svc->current_superframe = 0;
  svc->non_reference_frame = 0;
  svc->skip_enhancement_layer = 0;
  svc->disable_inter_layer_pred = INTER_LAYER_PRED_ON;
  svc->framedrop_mode = CONSTRAINED_LAYER_DROP;
  svc->set_intra_only_frame = 0;
  svc->previous_frame_is_intra_only = 0;
  svc->superframe_has_layer_sync = 0;
  svc->use_set_ref_frame_config = 0;
  svc->num_encoded_top_layer = 0;
  svc->simulcast_mode = 0;
  svc->single_layer_svc = 0;

  for (i = 0; i < REF_FRAMES; ++i) {
    svc->fb_idx_spatial_layer_id[i] = 0xff;
    svc->fb_idx_temporal_layer_id[i] = 0xff;
    svc->fb_idx_base[i] = 0;
  }
  for (sl = 0; sl < oxcf->ss_number_layers; ++sl) {
    svc->last_layer_dropped[sl] = 0;
    svc->drop_spatial_layer[sl] = 0;
    svc->ext_frame_flags[sl] = 0;
    svc->lst_fb_idx[sl] = 0;
    svc->gld_fb_idx[sl] = 1;
    svc->alt_fb_idx[sl] = 2;
    svc->framedrop_thresh[sl] = oxcf->drop_frames_water_mark;
    svc->fb_idx_upd_tl0[sl] = -1;
    svc->drop_count[sl] = 0;
    svc->spatial_layer_sync[sl] = 0;
    svc->force_drop_constrained_from_above[sl] = 0;
  }
  svc->max_consec_drop = INT_MAX;

  svc->buffer_gf_temporal_ref[1].idx = 7;
  svc->buffer_gf_temporal_ref[0].idx = 6;
  svc->buffer_gf_temporal_ref[1].is_used = 0;
  svc->buffer_gf_temporal_ref[0].is_used = 0;

  for (sl = 0; sl < oxcf->ss_number_layers; ++sl) {
    for (tl = 0; tl < oxcf->ts_number_layers; ++tl) {
      int layer = LAYER_IDS_TO_IDX(sl, tl, oxcf->ts_number_layers);
      LAYER_CONTEXT *const lc = &svc->layer_context[layer];
      RATE_CONTROL *const lrc = &lc->rc;
      int i;
      lc->current_video_frame_in_layer = 0;
      lc->layer_size = 0;
      lc->frames_from_key_frame = 0;
      lc->last_frame_type = FRAME_TYPES;
      lrc->ni_av_qi = oxcf->worst_allowed_q;
      lrc->total_actual_bits = 0;
      lrc->total_target_vs_actual = 0;
      lrc->ni_tot_qi = 0;
      lrc->tot_q = 0.0;
      lrc->avg_q = 0.0;
      lrc->ni_frames = 0;
      lrc->decimation_count = 0;
      lrc->decimation_factor = 0;
      lrc->worst_quality = oxcf->worst_allowed_q;
      lrc->best_quality = oxcf->best_allowed_q;

      for (i = 0; i < RATE_FACTOR_LEVELS; ++i) {
        lrc->rate_correction_factors[i] = 1.0;
      }

      if (cpi->oxcf.rc_mode == VPX_CBR) {
        lc->target_bandwidth = oxcf->layer_target_bitrate[layer];
        lrc->last_q[INTER_FRAME] = oxcf->worst_allowed_q;
        lrc->avg_frame_qindex[INTER_FRAME] = oxcf->worst_allowed_q;
        lrc->avg_frame_qindex[KEY_FRAME] = oxcf->worst_allowed_q;
      } else {
        lc->target_bandwidth = oxcf->layer_target_bitrate[layer];
        lrc->last_q[KEY_FRAME] = oxcf->best_allowed_q;
        lrc->last_q[INTER_FRAME] = oxcf->best_allowed_q;
        lrc->avg_frame_qindex[KEY_FRAME] =
            (oxcf->worst_allowed_q + oxcf->best_allowed_q) / 2;
        lrc->avg_frame_qindex[INTER_FRAME] =
            (oxcf->worst_allowed_q + oxcf->best_allowed_q) / 2;
        if (oxcf->ss_enable_auto_arf[sl])
          lc->alt_ref_idx = alt_ref_idx++;
        else
          lc->alt_ref_idx = INVALID_IDX;
        lc->gold_ref_idx = INVALID_IDX;
      }

      lrc->buffer_level =
          oxcf->starting_buffer_level_ms * lc->target_bandwidth / 1000;
      lrc->bits_off_target = lrc->buffer_level;

      // Initialize the cyclic refresh parameters. If spatial layers are used
      // (i.e., ss_number_layers > 1), these need to be updated per spatial
      // layer.
      // Cyclic refresh is only applied on base temporal layer.
      if (oxcf->ss_number_layers > 1 && tl == 0) {
        lc->sb_index = 0;
        lc->actual_num_seg1_blocks = 0;
        lc->actual_num_seg2_blocks = 0;
        lc->counter_encode_maxq_scene_change = 0;
      }
    }
  }

  // Still have extra buffer for base layer golden frame
  if (!(svc->number_temporal_layers > 1 && cpi->oxcf.rc_mode == VPX_CBR) &&
      alt_ref_idx < REF_FRAMES)
    svc->layer_context[0].gold_ref_idx = alt_ref_idx;
}

// Update the layer context from a change_config() call.
void vp9_update_layer_context_change_config(VP9_COMP *const cpi,
                                            const int target_bandwidth) {
  SVC *const svc = &cpi->svc;
  const VP9EncoderConfig *const oxcf = &cpi->oxcf;
  const RATE_CONTROL *const rc = &cpi->rc;
  int sl, tl, layer = 0, spatial_layer_target;
  float bitrate_alloc = 1.0;
  int num_spatial_layers_nonzero_rate = 0;

  cpi->svc.temporal_layering_mode = oxcf->temporal_layering_mode;

  if (svc->temporal_layering_mode != VP9E_TEMPORAL_LAYERING_MODE_NOLAYERING) {
    for (sl = 0; sl < oxcf->ss_number_layers; ++sl) {
      for (tl = 0; tl < oxcf->ts_number_layers; ++tl) {
        layer = LAYER_IDS_TO_IDX(sl, tl, oxcf->ts_number_layers);
        svc->layer_context[layer].target_bandwidth =
            oxcf->layer_target_bitrate[layer];
      }

      layer = LAYER_IDS_TO_IDX(
          sl,
          ((oxcf->ts_number_layers - 1) < 0 ? 0 : (oxcf->ts_number_layers - 1)),
          oxcf->ts_number_layers);
      spatial_layer_target = svc->layer_context[layer].target_bandwidth =
          oxcf->layer_target_bitrate[layer];

      for (tl = 0; tl < oxcf->ts_number_layers; ++tl) {
        LAYER_CONTEXT *const lc =
            &svc->layer_context[sl * oxcf->ts_number_layers + tl];
        RATE_CONTROL *const lrc = &lc->rc;

        lc->spatial_layer_target_bandwidth = spatial_layer_target;
        bitrate_alloc = (float)lc->target_bandwidth / target_bandwidth;
        lrc->starting_buffer_level =
            (int64_t)(rc->starting_buffer_level * bitrate_alloc);
        lrc->optimal_buffer_level =
            (int64_t)(rc->optimal_buffer_level * bitrate_alloc);
        lrc->maximum_buffer_size =
            (int64_t)(rc->maximum_buffer_size * bitrate_alloc);
        lrc->bits_off_target =
            VPXMIN(lrc->bits_off_target, lrc->maximum_buffer_size);
        lrc->buffer_level = VPXMIN(lrc->buffer_level, lrc->maximum_buffer_size);
        lc->framerate = cpi->framerate / oxcf->ts_rate_decimator[tl];
        lrc->avg_frame_bandwidth = (int)(lc->target_bandwidth / lc->framerate);
        lrc->max_frame_bandwidth = rc->max_frame_bandwidth;
        lrc->worst_quality = rc->worst_quality;
        lrc->best_quality = rc->best_quality;
      }
    }
  } else {
    int layer_end;

    if (svc->number_temporal_layers > 1 && cpi->oxcf.rc_mode == VPX_CBR) {
      layer_end = svc->number_temporal_layers;
    } else {
      layer_end = svc->number_spatial_layers;
    }

    for (layer = 0; layer < layer_end; ++layer) {
      LAYER_CONTEXT *const lc = &svc->layer_context[layer];
      RATE_CONTROL *const lrc = &lc->rc;

      lc->target_bandwidth = oxcf->layer_target_bitrate[layer];

      bitrate_alloc = (float)lc->target_bandwidth / target_bandwidth;
      // Update buffer-related quantities.
      lrc->starting_buffer_level =
          (int64_t)(rc->starting_buffer_level * bitrate_alloc);
      lrc->optimal_buffer_level =
          (int64_t)(rc->optimal_buffer_level * bitrate_alloc);
      lrc->maximum_buffer_size =
          (int64_t)(rc->maximum_buffer_size * bitrate_alloc);
      lrc->bits_off_target =
          VPXMIN(lrc->bits_off_target, lrc->maximum_buffer_size);
      lrc->buffer_level = VPXMIN(lrc->buffer_level, lrc->maximum_buffer_size);
      // Update framerate-related quantities.
      if (svc->number_temporal_layers > 1 && cpi->oxcf.rc_mode == VPX_CBR) {
        lc->framerate = cpi->framerate / oxcf->ts_rate_decimator[layer];
      } else {
        lc->framerate = cpi->framerate;
      }
      lrc->avg_frame_bandwidth = (int)(lc->target_bandwidth / lc->framerate);
      lrc->max_frame_bandwidth = rc->max_frame_bandwidth;
      // Update qp-related quantities.
      lrc->worst_quality = rc->worst_quality;
      lrc->best_quality = rc->best_quality;
    }
  }
  for (sl = 0; sl < oxcf->ss_number_layers; ++sl) {
    // Check bitrate of spatia layer.
    layer = LAYER_IDS_TO_IDX(sl, oxcf->ts_number_layers - 1,
                             oxcf->ts_number_layers);
    if (oxcf->layer_target_bitrate[layer] > 0)
      num_spatial_layers_nonzero_rate += 1;
  }
  if (num_spatial_layers_nonzero_rate == 1)
    svc->single_layer_svc = 1;
  else
    svc->single_layer_svc = 0;
}

static LAYER_CONTEXT *get_layer_context(VP9_COMP *const cpi) {
  if (is_one_pass_cbr_svc(cpi))
    return &cpi->svc.layer_context[cpi->svc.spatial_layer_id *
                                       cpi->svc.number_temporal_layers +
                                   cpi->svc.temporal_layer_id];
  else
    return (cpi->svc.number_temporal_layers > 1 && cpi->oxcf.rc_mode == VPX_CBR)
               ? &cpi->svc.layer_context[cpi->svc.temporal_layer_id]
               : &cpi->svc.layer_context[cpi->svc.spatial_layer_id];
}

void vp9_update_temporal_layer_framerate(VP9_COMP *const cpi) {
  SVC *const svc = &cpi->svc;
  const VP9EncoderConfig *const oxcf = &cpi->oxcf;
  LAYER_CONTEXT *const lc = get_layer_context(cpi);
  RATE_CONTROL *const lrc = &lc->rc;
  // Index into spatial+temporal arrays.
  const int st_idx = svc->spatial_layer_id * svc->number_temporal_layers +
                     svc->temporal_layer_id;
  const int tl = svc->temporal_layer_id;

  lc->framerate = cpi->framerate / oxcf->ts_rate_decimator[tl];
  lrc->avg_frame_bandwidth = (int)(lc->target_bandwidth / lc->framerate);
  lrc->max_frame_bandwidth = cpi->rc.max_frame_bandwidth;
  // Update the average layer frame size (non-cumulative per-frame-bw).
  if (tl == 0) {
    lc->avg_frame_size = lrc->avg_frame_bandwidth;
  } else {
    const double prev_layer_framerate =
        cpi->framerate / oxcf->ts_rate_decimator[tl - 1];
    const int prev_layer_target_bandwidth =
        oxcf->layer_target_bitrate[st_idx - 1];
    lc->avg_frame_size =
        (int)((lc->target_bandwidth - prev_layer_target_bandwidth) /
              (lc->framerate - prev_layer_framerate));
  }
}

void vp9_update_spatial_layer_framerate(VP9_COMP *const cpi, double framerate) {
  const VP9EncoderConfig *const oxcf = &cpi->oxcf;
  LAYER_CONTEXT *const lc = get_layer_context(cpi);
  RATE_CONTROL *const lrc = &lc->rc;

  lc->framerate = framerate;
  lrc->avg_frame_bandwidth = (int)(lc->target_bandwidth / lc->framerate);
  lrc->min_frame_bandwidth =
      (int)(lrc->avg_frame_bandwidth * oxcf->two_pass_vbrmin_section / 100);
  lrc->max_frame_bandwidth = (int)(((int64_t)lrc->avg_frame_bandwidth *
                                    oxcf->two_pass_vbrmax_section) /
                                   100);
  vp9_rc_set_gf_interval_range(cpi, lrc);
}

void vp9_restore_layer_context(VP9_COMP *const cpi) {
  LAYER_CONTEXT *const lc = get_layer_context(cpi);
  const int old_frame_since_key = cpi->rc.frames_since_key;
  const int old_frame_to_key = cpi->rc.frames_to_key;
  const int old_ext_use_post_encode_drop = cpi->rc.ext_use_post_encode_drop;

  cpi->rc = lc->rc;
  //cpi->twopass = lc->twopass;
  cpi->oxcf.target_bandwidth = lc->target_bandwidth;
  // Check if it is one_pass_cbr_svc mode and lc->speed > 0 (real-time mode
  // does not use speed = 0).
  if (is_one_pass_cbr_svc(cpi) && lc->speed > 0) {
    cpi->oxcf.speed = lc->speed;
  }
  // Reset the frames_since_key and frames_to_key counters to their values
  // before the layer restore. Keep these defined for the stream (not layer).
  if (cpi->svc.number_temporal_layers > 1 ||
      cpi->svc.number_spatial_layers > 1) {
    cpi->rc.frames_since_key = old_frame_since_key;
    cpi->rc.frames_to_key = old_frame_to_key;
  }
  cpi->rc.ext_use_post_encode_drop = old_ext_use_post_encode_drop;
}

void vp9_save_layer_context(VP9_COMP *const cpi) {
  const VP9EncoderConfig *const oxcf = &cpi->oxcf;
  LAYER_CONTEXT *const lc = get_layer_context(cpi);

  lc->rc = cpi->rc;
  //Fixme: one one pass supported
  //lc->twopass = cpi->twopass;
  lc->target_bandwidth = (int)oxcf->target_bandwidth;
}

void vp9_inc_frame_in_layer(VP9_COMP *const cpi) {
  LAYER_CONTEXT *const lc =
      &cpi->svc.layer_context[cpi->svc.spatial_layer_id *
                              cpi->svc.number_temporal_layers];
  ++lc->current_video_frame_in_layer;
  ++lc->frames_from_key_frame;
  if (cpi->svc.spatial_layer_id == cpi->svc.number_spatial_layers - 1)
    ++cpi->svc.current_superframe;
}

void get_layer_resolution(const int width_org, const int height_org,
                          const int num, const int den, int *width_out,
                          int *height_out) {
  int w, h;

  if (width_out == NULL || height_out == NULL || den == 0) return;

  w = width_org * num / den;
  h = height_org * num / den;

  // make height and width even to make chrome player happy
  w += w % 2;
  h += h % 2;

  *width_out = w;
  *height_out = h;
}

int vp9_one_pass_cbr_svc_start_layer(VP9_COMP *const cpi) {
  int width = 0, height = 0;
  SVC *const svc = &cpi->svc;
  LAYER_CONTEXT *lc = NULL;
  int scaling_factor_num = 1;
  int scaling_factor_den = 1;
  svc->skip_enhancement_layer = 0;

  if (svc->disable_inter_layer_pred == INTER_LAYER_PRED_OFF &&
      svc->number_spatial_layers > 1 && svc->number_spatial_layers <= 3 &&
      svc->number_temporal_layers <= 3)
    svc->simulcast_mode = 1;
  else
    svc->simulcast_mode = 0;

  if (svc->number_spatial_layers > 1) {
    svc->use_base_mv = 1;
    svc->use_partition_reuse = 1;
  }
  svc->force_zero_mode_spatial_ref = 1;

  // For constrained_from_above drop mode: before encoding superframe (i.e.,
  // at SL0 frame) check all spatial layers (starting from top) for possible
  // drop, and if so, set a flag to force drop of that layer and all its lower
  // layers.
  if (svc->spatial_layer_to_encode == svc->first_spatial_layer_to_encode) {
    int sl;
    for (sl = 0; sl < svc->number_spatial_layers; sl++)
      svc->force_drop_constrained_from_above[sl] = 0;
    if (svc->framedrop_mode == CONSTRAINED_FROM_ABOVE_DROP) {
      for (sl = svc->number_spatial_layers - 1;
           sl >= svc->first_spatial_layer_to_encode; sl--) {
        int layer = sl * svc->number_temporal_layers + svc->temporal_layer_id;
        LAYER_CONTEXT *const lc = &svc->layer_context[layer];
        cpi->rc = lc->rc;
        cpi->oxcf.target_bandwidth = lc->target_bandwidth;
        if (vp9_test_drop(cpi)) {
          int sl2;
          // Set flag to force drop in encoding for this mode.
          for (sl2 = sl; sl2 >= svc->first_spatial_layer_to_encode; sl2--)
            svc->force_drop_constrained_from_above[sl2] = 1;
          break;
        }
      }
    }
  }

  lc = &svc->layer_context[svc->spatial_layer_id * svc->number_temporal_layers +
                           svc->temporal_layer_id];

  // Setting the worst/best_quality via the encoder control: SET_SVC_PARAMETERS,
  // only for non-BYPASS mode for now.
  if (svc->temporal_layering_mode != VP9E_TEMPORAL_LAYERING_MODE_BYPASS ||
      svc->use_set_ref_frame_config) {
    RATE_CONTROL *const lrc = &lc->rc;
    lrc->worst_quality = vp9_quantizer_to_qindex(lc->max_q);
    lrc->best_quality = vp9_quantizer_to_qindex(lc->min_q);
  }

  if (cpi->oxcf.resize_mode == RESIZE_DYNAMIC && svc->single_layer_svc == 1 &&
      svc->spatial_layer_id == svc->first_spatial_layer_to_encode /* &&
      cpi->resize_state != ORIG */) {
    scaling_factor_num = lc->scaling_factor_num_resize;
    scaling_factor_den = lc->scaling_factor_den_resize;
  } else {
    scaling_factor_num = lc->scaling_factor_num;
    scaling_factor_den = lc->scaling_factor_den;
  }

  get_layer_resolution(cpi->oxcf.width, cpi->oxcf.height, scaling_factor_num,
                       scaling_factor_den, &width, &height);

  // The usage of use_base_mv or partition_reuse assumes down-scale of 2x2.
  // For now, turn off use of base motion vectors and partition reuse if the
  // spatial scale factors for any layers are not 2,
  // keep the case of 3 spatial layers with scale factor of 4x4 for base layer.
  // TODO(marpan): Fix this to allow for use_base_mv for scale factors != 2.
  if (svc->number_spatial_layers > 1) {
    int sl;
    for (sl = 0; sl < svc->number_spatial_layers - 1; ++sl) {
      lc = &svc->layer_context[sl * svc->number_temporal_layers +
                               svc->temporal_layer_id];
      if ((lc->scaling_factor_num != lc->scaling_factor_den >> 1) &&
          !(lc->scaling_factor_num == lc->scaling_factor_den >> 2 && sl == 0 &&
            svc->number_spatial_layers == 3)) {
        svc->use_base_mv = 0;
        svc->use_partition_reuse = 0;
        break;
      }
    }
    // For non-zero spatial layers: if the previous spatial layer was dropped
    // disable the base_mv and partition_reuse features.
    if (svc->spatial_layer_id > 0 &&
        svc->drop_spatial_layer[svc->spatial_layer_id - 1]) {
      svc->use_base_mv = 0;
      svc->use_partition_reuse = 0;
    }
  }

  svc->non_reference_frame = 0;
  if (cpi->common.frame_type != KEY_FRAME && !cpi->ext_refresh_last_frame &&
      !cpi->ext_refresh_golden_frame && !cpi->ext_refresh_alt_ref_frame)
    svc->non_reference_frame = 1;
  // For non-flexible mode, where update_buffer_slot is used, need to check if
  // all buffer slots are not refreshed.
  if (svc->temporal_layering_mode == VP9E_TEMPORAL_LAYERING_MODE_BYPASS) {
    if (svc->update_buffer_slot[svc->spatial_layer_id] != 0)
      svc->non_reference_frame = 0;
  }

  if (svc->spatial_layer_id == 0) {
    svc->high_source_sad_superframe = 0;
    svc->high_num_blocks_with_motion = 0;
  }

  if (svc->temporal_layering_mode != VP9E_TEMPORAL_LAYERING_MODE_BYPASS &&
      svc->last_layer_dropped[svc->spatial_layer_id] &&
      svc->fb_idx_upd_tl0[svc->spatial_layer_id] != -1 &&
      !svc->layer_context[svc->temporal_layer_id].is_key_frame) {
    // For fixed/non-flexible mode, if the previous frame (same spatial layer
    // from previous superframe) was dropped, make sure the lst_fb_idx
    // for this frame corresponds to the buffer index updated on (last) encoded
    // TL0 frame (with same spatial layer).
    cpi->lst_fb_idx = svc->fb_idx_upd_tl0[svc->spatial_layer_id];
  }
  //Fixme: Important: check size
#if 0
  if (vp9_set_size_literal(cpi, width, height) != 0)
    return VPX_CODEC_INVALID_PARAM;
#endif
  return 0;
}

// Reset on key frame: reset counters, references and buffer updates.
void vp9_svc_reset_temporal_layers(VP9_COMP *const cpi, int is_key) {
  int sl, tl;
  SVC *const svc = &cpi->svc;
  LAYER_CONTEXT *lc = NULL;
  for (sl = 0; sl < svc->number_spatial_layers; ++sl) {
    for (tl = 0; tl < svc->number_temporal_layers; ++tl) {
      lc = &cpi->svc.layer_context[sl * svc->number_temporal_layers + tl];
      lc->current_video_frame_in_layer = 0;
      if (is_key) lc->frames_from_key_frame = 0;
    }
  }
  vp9_update_temporal_layer_framerate(cpi);
  vp9_restore_layer_context(cpi);
}

void vp9_svc_check_reset_layer_rc_flag(VP9_COMP *cpi) {
  SVC *svc = &cpi->svc;
  int sl, tl;
  for (sl = 0; sl < svc->number_spatial_layers; ++sl) {
    // Check for reset based on avg_frame_bandwidth for spatial layer sl.
    int layer = LAYER_IDS_TO_IDX(sl, svc->number_temporal_layers - 1,
                                 svc->number_temporal_layers);
    LAYER_CONTEXT *lc = &svc->layer_context[layer];
    RATE_CONTROL *lrc = &lc->rc;
    if (lrc->avg_frame_bandwidth > (3 * lrc->last_avg_frame_bandwidth >> 1) ||
        lrc->avg_frame_bandwidth < (lrc->last_avg_frame_bandwidth >> 1)) {
      // Reset for all temporal layers with spatial layer sl.
      for (tl = 0; tl < svc->number_temporal_layers; ++tl) {
        int layer = LAYER_IDS_TO_IDX(sl, tl, svc->number_temporal_layers);
        LAYER_CONTEXT *lc = &svc->layer_context[layer];
        RATE_CONTROL *lrc = &lc->rc;
        lrc->rc_1_frame = 0;
        lrc->rc_2_frame = 0;
        lrc->bits_off_target = lrc->optimal_buffer_level;
        lrc->buffer_level = lrc->optimal_buffer_level;
      }
    }
  }
}

void vp9_svc_check_spatial_layer_sync(VP9_COMP *const cpi) {
  SVC *const svc = &cpi->svc;
  // Only for superframes whose base is not key, as those are
  // already sync frames.
  if (!svc->layer_context[svc->temporal_layer_id].is_key_frame) {
    if (svc->spatial_layer_id == 0) {
      // On base spatial layer: if the current superframe has a layer sync then
      // reset the pattern counters and reset to base temporal layer.
      if (svc->superframe_has_layer_sync)
        vp9_svc_reset_temporal_layers(cpi, cpi->common.frame_type == KEY_FRAME);
    }
   // If the layer sync is set for this current spatial layer then
    // disable the temporal reference.
    if (svc->spatial_layer_id > 0 &&
        svc->spatial_layer_sync[svc->spatial_layer_id]) {
      if (svc->use_gf_temporal_ref_current_layer) {
        int index = svc->spatial_layer_id;
        // If golden is used as second reference: need to remove it from
        // prediction, reset refresh period to 0, and update the reference.
        svc->use_gf_temporal_ref_current_layer = 0;
        cpi->rc.baseline_gf_interval = 0;
        cpi->rc.frames_till_gf_update_due = 0;
        // On layer sync frame we must update the buffer index used for long
        // term reference. Use the alt_ref since it is not used or updated on
        // sync frames.
        if (svc->number_spatial_layers == 3) index = svc->spatial_layer_id - 1;
        assert(index >= 0);
        cpi->alt_fb_idx = svc->buffer_gf_temporal_ref[index].idx;
        cpi->ext_refresh_alt_ref_frame = 1;
      }
    }
  }
}
void vp9_svc_adjust_avg_frame_qindex(VP9_COMP *const cpi) {
  VP9_COMMON *const cm = &cpi->common;
  SVC *const svc = &cpi->svc;
  RATE_CONTROL *const rc = &cpi->rc;
  // On key frames in CBR mode: reset the avg_frame_index for base layer
  // (to level closer to worst_quality) if the overshoot is significant.
  // Reset it for all temporal layers on base spatial layer.
  if (cm->frame_type == KEY_FRAME && cpi->oxcf.rc_mode == VPX_CBR &&
      !svc->simulcast_mode &&
      rc->projected_frame_size > 3 * rc->avg_frame_bandwidth) {
    int tl;
    rc->avg_frame_qindex[INTER_FRAME] =
        VPXMAX(rc->avg_frame_qindex[INTER_FRAME],
               (cm->base_qindex + rc->worst_quality) >> 1);
    for (tl = 0; tl < svc->number_temporal_layers; ++tl) {
      const int layer = LAYER_IDS_TO_IDX(0, tl, svc->number_temporal_layers);
      LAYER_CONTEXT *lc = &svc->layer_context[layer];
      RATE_CONTROL *lrc = &lc->rc;
      lrc->avg_frame_qindex[INTER_FRAME] = rc->avg_frame_qindex[INTER_FRAME];
    }
  }
}
