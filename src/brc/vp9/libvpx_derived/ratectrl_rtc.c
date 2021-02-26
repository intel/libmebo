/*
 *  Copyright (c) 2020 The WebM project authors. All Rights Reserved.
 *  Copyright (c) 2020 Intel Corporation. All Rights Reserved.
 *  Copyright (c) 2020 Sreerenj Balachandran <sreerenj.balachandran@intel.com>
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "ratectrl_rtc.h"

#undef ERROR
#define ERROR(str)                  \
  do {                              \
    fprintf(stderr, "%s \n", str);	    \
    return STATUS_BRC_VP9_INVALID_PARAM; \
  } while (0)

#define RANGE_CHECK(p, memb, lo, hi)                                     \
  do {                                                                   \
    if (!(((p)->memb == (lo) || (p)->memb > (lo)) && (p)->memb <= (hi))) \
      ERROR(#memb " out of range [" #lo ".." #hi "]");                   \
  } while (0)

#define RANGE_CHECK_HI(p, memb, hi)                                     \
  do {                                                                  \
    if (!((p)->memb <= (hi))) ERROR(#memb " out of range [.." #hi "]"); \
  } while (0)

#define RANGE_CHECK_LO(p, memb, lo)                                     \
  do {                                                                  \
    if (!((p)->memb >= (lo))) ERROR(#memb " out of range [" #lo "..]"); \
  } while (0)

#define RANGE_CHECK_BOOL(p, memb)                                     \
  do {                                                                \
    if (!!((p)->memb) != (p)->memb) ERROR(#memb " expected boolean"); \
  } while (0)

/*===== BRC functions exposed to libs implementation ========= */

LibMeboStatus
brc_vp9_post_encode_update(BrcCodecEnginePtr engine_ptr, uint64_t encoded_frame_size) {
  VP9RateControlRTC *rtc = (VP9RateControlRTC *) engine_ptr;
  VP9_COMP *cpi_ = &rtc->cpi_;
  vp9_rc_postencode_update(cpi_, encoded_frame_size);

  if (cpi_->svc.number_spatial_layers > 1 ||
      cpi_->svc.number_temporal_layers > 1)
    vp9_save_layer_context(cpi_);

  cpi_->common.current_video_frame++;

  return LIBMEBO_STATUS_SUCCESS;
}

LibMeboStatus
brc_vp9_get_qp(BrcCodecEnginePtr engine_ptr, int *qp) {
  VP9RateControlRTC *rtc = (VP9RateControlRTC *) engine_ptr;	
  VP9_COMP *cpi_ = &rtc->cpi_;
  *qp = cpi_->common.base_qindex;
  return LIBMEBO_STATUS_SUCCESS;
}

LibMeboStatus
brc_vp9_get_loop_filter_level(BrcCodecEnginePtr engine_ptr, int *filter_level) {
  VP9RateControlRTC *rtc = (VP9RateControlRTC *) engine_ptr;
  VP9_COMP *cpi_ = &rtc->cpi_;
  struct loopfilter *const lf = &cpi_->common.lf;
  vp9_pick_filter_level(cpi_, LPF_PICK_FROM_Q);
  *filter_level = lf->filter_level;
  return LIBMEBO_STATUS_SUCCESS;
}

LibMeboStatus
brc_vp9_compute_qp (BrcCodecEnginePtr engine_ptr, LibMeboRCFrameParams *frame_params) {
  VP9RateControlRTC *rtc = (VP9RateControlRTC *) engine_ptr;
  VP9_COMP *cpi_ = &rtc->cpi_;
  VP9_COMMON *const cm = &cpi_->common;
  int width, height;

  cpi_->svc.spatial_layer_id = frame_params->spatial_layer_id;
  cpi_->svc.temporal_layer_id = frame_params->temporal_layer_id;
  if (cpi_->svc.number_spatial_layers > 1) {
    const int layer = LAYER_IDS_TO_IDX(cpi_->svc.spatial_layer_id,
                                       cpi_->svc.temporal_layer_id,
                                       cpi_->svc.number_temporal_layers);
    LAYER_CONTEXT *lc = &cpi_->svc.layer_context[layer];
    get_layer_resolution(cpi_->oxcf.width, cpi_->oxcf.height,
                         lc->scaling_factor_num, lc->scaling_factor_den, &width,
                         &height);
    cm->width = width;
    cm->height = height;
  }
  vp9_set_mb_mi(cm, cm->width, cm->height);

  cm->frame_type = frame_params->frame_type;
  cpi_->refresh_golden_frame = (cm->frame_type == KEY_FRAME) ? 1 : 0;

  cpi_->sf.use_nonrd_pick_mode = 1;

  if (cpi_->svc.number_spatial_layers == 1 &&
      cpi_->svc.number_temporal_layers == 1) {
    int target;
    if (frame_is_intra_only(cm)) {
      target = vp9_calc_iframe_target_size_one_pass_cbr(cpi_);
    }
    else {
      target = vp9_calc_pframe_target_size_one_pass_cbr(cpi_);
    }
    vp9_rc_set_frame_target(cpi_, target);
    update_buffer_level_preencode(cpi_);
  } else {
    vp9_update_temporal_layer_framerate(cpi_);
    //Clarify: seems to be a dead code in libvpx
    // vp9_update_spatial_layer_framerate (cpi_, 30);
    vp9_restore_layer_context(cpi_);
    vp9_rc_get_svc_params(cpi_);
  }

  int bottom_index, top_index;
  cpi_->common.base_qindex =
      vp9_rc_pick_q_and_bounds(cpi_, &bottom_index, &top_index);
  return LIBMEBO_STATUS_SUCCESS;
}

LibMeboStatus
brc_vp9_update_rate_control(BrcCodecEnginePtr engine_ptr, LibMeboRateControllerConfig *rc_cfg) {
  VP9RateControlRTC *rtc = (VP9RateControlRTC *) engine_ptr;
  VP9_COMP *cpi_ = &rtc->cpi_;
  VP9_COMMON *cm = &cpi_->common;
  VP9EncoderConfig *oxcf = &cpi_->oxcf;
  RATE_CONTROL *const rc = &cpi_->rc;

  cm->width = rc_cfg->width;
  cm->height = rc_cfg->height;
  cm->MBs = (cm->width * cm->height)/ (16 * 16);
  oxcf->width = rc_cfg->width;
  oxcf->height = rc_cfg->height;
  oxcf->bit_depth = VPX_BITS_8;
  if (oxcf->init_framerate > 180)
    oxcf->init_framerate = 30;
  else
    oxcf->init_framerate = rc_cfg->framerate;
  oxcf->worst_allowed_q = vp9_quantizer_to_qindex(rc_cfg->max_quantizer);
  oxcf->best_allowed_q = vp9_quantizer_to_qindex(rc_cfg->min_quantizer);
  rc->worst_quality = oxcf->worst_allowed_q;
  rc->best_quality = oxcf->best_allowed_q;
  oxcf->target_bandwidth = 1000 * rc_cfg->target_bandwidth;
  oxcf->starting_buffer_level_ms = rc_cfg->buf_initial_sz;
  oxcf->optimal_buffer_level_ms = rc_cfg->buf_optimal_sz;
  oxcf->maximum_buffer_size_ms = rc_cfg->buf_sz;
  oxcf->under_shoot_pct = rc_cfg->undershoot_pct;
  oxcf->over_shoot_pct = rc_cfg->overshoot_pct;

  oxcf->ss_number_layers = rc_cfg->ss_number_layers;
  oxcf->ts_number_layers = rc_cfg->ts_number_layers;

  oxcf->temporal_layering_mode = (VP9E_TEMPORAL_LAYERING_MODE)(
      (rc_cfg->ts_number_layers > 1) ? rc_cfg->ts_number_layers : 0);

  cpi_->oxcf.rc_max_intra_bitrate_pct = rc_cfg->max_intra_bitrate_pct;
  cpi_->framerate = rc_cfg->framerate;

  cpi_->svc.number_spatial_layers = rc_cfg->ss_number_layers;
  cpi_->svc.number_temporal_layers = rc_cfg->ts_number_layers;

  cpi_->refresh_golden_frame = 0;
  cpi_->refresh_last_frame = 1;

  for (int sl = 0; sl < (cpi_->svc.number_spatial_layers % VPX_SS_MAX_LAYERS); ++sl) {
    for (int tl = 0; tl < (cpi_->svc.number_temporal_layers % VPX_TS_MAX_LAYERS); ++tl) {
      const int layer =
          LAYER_IDS_TO_IDX(sl, tl, cpi_->svc.number_temporal_layers);
      LAYER_CONTEXT *lc = &cpi_->svc.layer_context[layer];
      RATE_CONTROL *const lrc = &lc->rc;
      oxcf->layer_target_bitrate[layer] =
          1000 * rc_cfg->layer_target_bitrate[layer];
      lrc->worst_quality =
          vp9_quantizer_to_qindex(rc_cfg->max_quantizers[layer]);
      lrc->best_quality = vp9_quantizer_to_qindex(rc_cfg->min_quantizers[layer]);
      lc->scaling_factor_num = rc_cfg->scaling_factor_num[sl];
      lc->scaling_factor_den = rc_cfg->scaling_factor_den[sl];
      oxcf->ts_rate_decimator[tl] = rc_cfg->ts_rate_decimator[tl];
    }
  }

  vp9_set_rc_buffer_sizes(rc, &cpi_->oxcf);
  vp9_new_framerate(cpi_, cpi_->framerate);

  if (cpi_->svc.number_temporal_layers > 1 ||
      cpi_->svc.number_spatial_layers > 1) {
    if (cm->current_video_frame == 0)
      vp9_init_layer_context(cpi_);
    vp9_update_layer_context_change_config(cpi_,
                                           (int)cpi_->oxcf.target_bandwidth);
  }

  vp9_check_reset_rc_flag(cpi_);
  return LIBMEBO_STATUS_SUCCESS;
}

static void
brc_init_rate_control(VP9RateControlRTC *rtc, LibMeboRateControllerConfig *rc_cfg) {
  VP9_COMP *cpi_ = &rtc->cpi_;
  VP9_COMMON *cm = &cpi_->common;
  VP9EncoderConfig *oxcf = &cpi_->oxcf;
  RATE_CONTROL *const rc = &cpi_->rc;
  cm->profile = PROFILE_0;
  cm->bit_depth = VPX_BITS_8;
  cm->show_frame = 1;
  oxcf->mode = GOOD;
  oxcf->rc_mode = VPX_CBR;
  oxcf->pass = 0;

  /*Defaults derived from vp9_spatial_svc_encoder*/
  oxcf->aq_mode = NO_AQ;
  oxcf->content = VP9E_CONTENT_DEFAULT;
  oxcf->drop_frames_water_mark = 0;
  oxcf->lag_in_frames = 25;
  oxcf->two_pass_vbrmin_section = 0;
  oxcf->two_pass_vbrmax_section = 2000; //It should be okay to reset to zero
  oxcf->enable_auto_arf = 0;//Note, double check???
  oxcf->target_level = 0;

  cpi_->target_level = oxcf->target_level;

  brc_vp9_update_rate_control((BrcCodecEnginePtr)rtc, rc_cfg);

  cpi_->use_svc = (cpi_->svc.number_spatial_layers > 1 ||
                   cpi_->svc.number_temporal_layers > 1)
                      ? 1
                      : 0;
  rc->rc_1_frame = 0;
  rc->rc_2_frame = 0;
  //vp9_change_config (cpi_, oxcf);
  vp9_rc_init_minq_luts();
  vp9_rc_init(oxcf, 0, rc);

  cpi_->sf.use_nonrd_pick_mode = 1;
  cm->current_video_frame = 0;
}

LibMeboStatus
brc_vp9_validate (LibMeboRateControllerConfig *cfg)
{
  LibMeboStatus status = LIBMEBO_STATUS_SUCCESS;

  RANGE_CHECK(cfg, width, 1, 65535);
  RANGE_CHECK(cfg, height, 1, 65535);
  RANGE_CHECK_HI(cfg, max_quantizer, 63);
  RANGE_CHECK_HI(cfg, min_quantizer, cfg->max_quantizer);
  //RANGE_CHECK(cfg, rc_end_usage, VPX_VBR, VPX_Q);
  RANGE_CHECK_HI(cfg, undershoot_pct, 100);
  RANGE_CHECK_HI(cfg, overshoot_pct, 100);
  RANGE_CHECK(cfg, ss_number_layers, 1, VPX_SS_MAX_LAYERS);
  RANGE_CHECK(cfg, ts_number_layers, 1, VPX_TS_MAX_LAYERS);

  if (cfg->ss_number_layers * cfg->ts_number_layers > VPX_MAX_LAYERS)
    ERROR("ss_number_layers * ts_number_layers is out of range");

   if (cfg->ts_number_layers > 1) {
    unsigned int sl, tl;
    for (sl = 1; sl < cfg->ss_number_layers; ++sl) {
      for (tl = 1; tl < cfg->ts_number_layers; ++tl) {
        const int layer = LAYER_IDS_TO_IDX(sl, tl, cfg->ts_number_layers);
        if (cfg->layer_target_bitrate[layer] <
            cfg->layer_target_bitrate[layer - 1])
          ERROR("ts_target_bitrate entries are not increasing");
      }
    }

    RANGE_CHECK(cfg, ts_rate_decimator[cfg->ts_number_layers - 1], 1, 1);
    for (tl = cfg->ts_number_layers - 2; tl > 0; --tl)
      if (cfg->ts_rate_decimator[tl - 1] != 2 * cfg->ts_rate_decimator[tl])
        ERROR("ts_rate_decimator factors are not powers of 2");
  }
  return status;
}


LibMeboStatus
brc_vp9_rate_control_init (LibMeboRateControllerConfig *cfg,
    BrcCodecEnginePtr *brc_codec_handler) {
  LibMeboStatus status = LIBMEBO_STATUS_SUCCESS;
  VP9RateControlRTC *rtc = NULL;

  status = brc_vp9_validate (cfg);
  if (status != LIBMEBO_STATUS_SUCCESS)
    return status;

  rtc = (VP9RateControlRTC*) malloc (sizeof (VP9RateControlRTC));
  if (!rtc)
    return LIBMEBO_STATUS_FAILED;

  memset (&rtc->cpi_, 0, sizeof (rtc->cpi_));
  brc_init_rate_control (rtc, (BrcCodecEnginePtr)cfg);

  *brc_codec_handler = (BrcCodecEnginePtr)rtc;
  return LIBMEBO_STATUS_SUCCESS;
}

void
brc_vp9_rate_control_free (BrcCodecEnginePtr engine_ptr) {
  VP9RateControlRTC *rtc = (VP9RateControlRTC *) engine_ptr;
  if (rtc)
    free(rtc);
}
