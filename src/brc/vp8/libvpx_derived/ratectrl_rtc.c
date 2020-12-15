/*
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
#include "vp8_ratectrl.h"
#define LAYER_IDS_TO_IDX(sl, tl, num_tl) ((sl) * (num_tl) + (tl))

#undef ERROR
#define ERROR(str)                  \
  do {                              \
    fprintf(stderr, "%s \n", str);	    \
    return STATUS_BRC_VP8_INVALID_PARAM; \
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

/* Quant MOD */
static const int q_trans[] = {
  0,  1,  2,  3,  4,  5,  7,   8,   9,   10,  12,  13,  15,  17,  18,  19,
  20, 21, 23, 24, 25, 26, 27,  28,  29,  30,  31,  33,  35,  37,  39,  41,
  43, 45, 47, 49, 51, 53, 55,  57,  59,  61,  64,  67,  70,  73,  76,  79,
  82, 85, 88, 91, 94, 97, 100, 103, 106, 109, 112, 115, 118, 121, 124, 127,
};

int vp8_reverse_trans(int x) {
  int i;

  for (i = 0; i < 64; ++i) {
    if (q_trans[i] >= x) return i;
  }

  return 63;
}

void brc_vp8_post_encode_update(VP8RateControlRTC *rtc,uint64_t encoded_frame_size) {

  VP8_COMP *cpi_ = &rtc->cpi_;
  vp8_rc_postencode_update(cpi_, encoded_frame_size);

  //if (cpi_->svc.number_spatial_layers > 1 ||
  //    cpi_->svc.number_temporal_layers > 1)
  //  vp8_save_layer_context(cpi_);

  cpi_->common.current_video_frame++;

}


int brc_vp8_get_qp(VP8RateControlRTC *rtc) {
  VP8_COMP *cpi_ = &rtc->cpi_;

  return cpi_->common.base_qindex;
}

int brc_vp8_get_loop_filter_level(VP8RateControlRTC *rtc) {
#if 0
  VP8_COMP *cpi_ = &rtc->cpi_;
  struct loopfilter *const lf = &cpi_->common.lf;
  vp8_pick_filter_level(cpi_, LPF_PICK_FROM_Q);
  return lf->filter_level;
#endif
  return 0;
}

void brc_vp8_compute_qp (VP8RateControlRTC *rtc, VP8FrameParamsQpRTC *frame_params) {
  VP8_COMP *cpi_ = &rtc->cpi_;
  VP8_COMMON *const cm = &cpi_->common;
  int Q;
  int frame_over_shoot_limit;
  int frame_under_shoot_limit;

  //Fixme: Add rate_correction_factor support based on active_worst_qchanged
  //vp8_update_rate_correction_factors(cpi, 2);
  //int active_worst_qchanged = 0;

  cm->frame_type = frame_params->frame_type;

  vp8_pick_frame_size (cpi_);

  /* This has a knock on effect on active best quality as well.
   * For CBR if the buffer reaches its maximum level then we can no longer
   * save up bits for later frames so we might as well use them up
   * on the current frame.
   */
  if ((cpi_->oxcf.end_usage == USAGE_STREAM_FROM_SERVER) &&
      (cpi_->buffer_level >= cpi_->oxcf.optimal_buffer_level) &&
      cpi_->buffered_mode) {
    /* Max adjustment is 1/4 */
    int Adjustment = cpi_->active_worst_quality / 4;

    if (Adjustment) {
      int buff_lvl_step;
  
      if (cpi_->buffer_level < cpi_->oxcf.maximum_buffer_size) {
        buff_lvl_step = (int)((cpi_->oxcf.maximum_buffer_size -
                               cpi_->oxcf.optimal_buffer_level) /
                              Adjustment);

        if (buff_lvl_step) {
          Adjustment =
              (int)((cpi_->buffer_level - cpi_->oxcf.optimal_buffer_level) /
                    buff_lvl_step);
        } else {
          Adjustment = 0;
        }
      }

      cpi_->active_worst_quality -= Adjustment;

      if (cpi_->active_worst_quality < cpi_->active_best_quality) {
        cpi_->active_worst_quality = cpi_->active_best_quality;
      }
    }
  }

  /* Clip the active best and worst quality values to limits */
  if (cpi_->active_worst_quality > cpi_->worst_quality) {
    cpi_->active_worst_quality = cpi_->worst_quality;
  }

   if (cpi_->active_best_quality < cpi_->best_quality) {
    cpi_->active_best_quality = cpi_->best_quality;
  }

  if (cpi_->active_worst_quality < cpi_->active_best_quality) {
    cpi_->active_worst_quality = cpi_->active_best_quality;
  }

  /* Determine initial Q to try */
  Q = vp8_regulate_q(cpi_, cpi_->this_frame_target);
  printf ("Q (in libmebo, aftre vp8_regulate_q)  = %d \n", Q);

  vp8_compute_frame_size_bounds(cpi_, &frame_under_shoot_limit,
                                &frame_over_shoot_limit);

#if 0
  //Fixme: Add rate_correction_factor() code
  /* Limit Q range for the adaptive loop. */
  bottom_index = cpi_->active_best_quality;
  top_index = cpi_->active_worst_quality;
  q_low = cpi_->active_best_quality;
  q_high = cpi_->active_worst_quality;
#endif

  cm->base_qindex = Q;

  if (cm->frame_type == VP8_KEY_FRAME) {
    cpi_->common.filter_level = cpi_->common.base_qindex * 3 / 8;

     /* Provisional interval before next GF */
    if (cpi_->auto_gold) {
      cpi_->frames_till_gf_update_due = cpi_->baseline_gf_interval;
    } else {
      cpi_->frames_till_gf_update_due = DEFAULT_GF_INTERVAL;
    }

    cpi_->common.refresh_golden_frame = 1;
    cpi_->common.refresh_alt_ref_frame = 1;
  }

  if (cpi_->pass == 0 && cpi_->oxcf.end_usage == USAGE_STREAM_FROM_SERVER) {
      if (vp8_drop_encodedframe_overshoot(cpi_, Q))
        return;
  }

  if (frame_over_shoot_limit == 0)
    frame_over_shoot_limit = 1;

  /* Are we are overshooting and up against the limit of active max Q. */
  if (((cpi_->pass != 2) ||
         (cpi_->oxcf.end_usage == USAGE_STREAM_FROM_SERVER)) &&
        (Q == cpi_->active_worst_quality) &&
        (cpi_->active_worst_quality < cpi_->worst_quality) &&
        (cpi_->projected_frame_size > frame_over_shoot_limit)) {
      int over_size_percent =
          ((cpi_->projected_frame_size - frame_over_shoot_limit) * 100) /
          frame_over_shoot_limit;

      /* If so is there any scope for relaxing it */
      while ((cpi_->active_worst_quality < cpi_->worst_quality) &&
             (over_size_percent > 0)) {
        cpi_->active_worst_quality++;
        /* Assume 1 qstep = about 4% on frame size. */
        over_size_percent = (int)(over_size_percent * 0.96);
      }
#if !CONFIG_REALTIME_ONLY
      //top_index = cpi_->active_worst_quality;
#endif  // !CONFIG_REALTIME_ONLY
      /* If we have updated the active max Q do not call
       * vp8_update_rate_correction_factors() this loop.
       */
      //active_worst_qchanged = 1;
  } else {
      //active_worst_qchanged = 0;
  }

  //Fixme1:
  //Implement   if ((cm->frame_type == KEY_FRAME) && cpi->this_key_frame_forced) {
  //

  //Fixme2
  // /* Is the projected frame size out of range and are we allowed
  //   * to attempt to recode.
  //   */
  //  else if (recode_loop_test(cpi, frame_over_shoot_limit,
  //                            frame_under_shoot_limit, Q, top_index,
  //                            bottom_index)) {

   if (cm->frame_type == VP8_KEY_FRAME) cm->refresh_last_frame = 1;

  //Fixme: Add loop filter calculation  vp8_loopfilter_frame(cpi, cm);


  //This is the next start pos encoder/onyx_if.c vp8_set_quantizer(cpi, Q);
#if 0

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
  vp8_set_mb_mi(cm, cm->width, cm->height);

  cpi_->refresh_golden_frame = (cm->frame_type == KEY_FRAME) ? 1 : 0;

  cpi_->sf.use_nonrd_pick_mode = 1;

  if (cpi_->svc.number_spatial_layers == 1 &&
      cpi_->svc.number_temporal_layers == 1) {
    int target;
    if (frame_is_intra_only(cm)) {
      target = vp8_calc_iframe_target_size_one_pass_cbr(cpi_);
    }
    else {
      target = vp8_calc_pframe_target_size_one_pass_cbr(cpi_);
    }
    vp8_rc_set_frame_target(cpi_, target);
    update_buffer_level_preencode(cpi_);
  } else {
    vp8_update_temporal_layer_framerate(cpi_);
    //Fixme:Important: this is not in the original source code
    // vp8_update_spatial_layer_framerate (cpi_, 30);
    vp8_restore_layer_context(cpi_);
    vp8_rc_get_svc_params(cpi_);
  }

  int bottom_index, top_index;
  cpi_->common.base_qindex =
      vp8_rc_pick_q_and_bounds(cpi_, &bottom_index, &top_index);
#endif
}

static int rescale(int val, int num, int denom) {
  int64_t llnum = num;
  int64_t llden = denom;
  int64_t llval = val;

  return (int)(llval * llnum / llden);
}

void brc_vp8_update_rate_control(VP8RateControlRTC *rtc, VP8RateControlRtcConfig *rc_cfg) {
  VP8_COMP *cpi_ = &rtc->cpi_;
  VP8_COMMON *cm = &cpi_->common;
  VP8_CONFIG *oxcf = &cpi_->oxcf;

  //Fill VP8_COMMON (init_config())
  cm->Width = rc_cfg->width;
  cm->Height = rc_cfg->height;
  cm->mb_rows = cm->Height >> 4;
  cm->mb_cols = cm->Width >> 4;
  cm->MBs = cm->mb_rows * cm->mb_cols;
  
  if (cm->current_video_frame == 0)
    cm->frame_type = VP8_KEY_FRAME;

  cm->show_frame = 1;
  cm->refresh_golden_frame = 0;
  cm->refresh_last_frame = 1;

  cpi_->ref_framerate = cpi_->framerate = rc_cfg->framerate;
  cpi_->ref_frame_flags = VP8_ALTR_FRAME | VP8_GOLD_FRAME | VP8_LAST_FRAME;

  //Fill VP8_CONFIG
 
  oxcf->Mode = 1; 
  oxcf->end_usage = USAGE_STREAM_FROM_SERVER;
  oxcf->auto_key = 0;
  oxcf->cq_level = 10;//Fixme?
  oxcf->number_of_layers = 1;
  oxcf->lag_in_frames = 25;


  oxcf->Width = rc_cfg->width;
  oxcf->Height = rc_cfg->height;

  oxcf->under_shoot_pct = rc_cfg->undershoot_pct;
  oxcf->over_shoot_pct = rc_cfg->overshoot_pct;

  oxcf->fixed_q = -1;
  oxcf->worst_allowed_q = rc_cfg->max_quantizer;
  oxcf->best_allowed_q = rc_cfg->min_quantizer;

  oxcf->two_pass_vbrmin_section = 0;
  oxcf->two_pass_vbrmax_section = 2000;//Fixme: 400 in libvpx?


  if (cpi_->pass == 0) cpi_->auto_worst_q = 1;

  oxcf->worst_allowed_q = q_trans[oxcf->worst_allowed_q];
  oxcf->best_allowed_q = q_trans[oxcf->best_allowed_q];
  oxcf->cq_level = q_trans[cpi_->oxcf.cq_level];

  if (oxcf->fixed_q >= 0) {
    if (oxcf->worst_allowed_q < 0) {
      oxcf->fixed_q = q_trans[0];
    } else {
      oxcf->fixed_q = q_trans[oxcf->worst_allowed_q];
    }

    if (oxcf->alt_q < 0) {
      oxcf->alt_q = q_trans[0];
    } else {
      oxcf->alt_q = q_trans[oxcf->alt_q];
    }

    if (oxcf->key_q < 0) {
      oxcf->key_q = q_trans[0];
    } else {
      oxcf->key_q = q_trans[oxcf->key_q];
    }
    if (oxcf->gold_q < 0) {
      oxcf->gold_q = q_trans[0];
    } else {
      oxcf->gold_q = q_trans[oxcf->gold_q];
    }
  }

  /* At the moment the first order values may not be > MAXQ */
  if (oxcf->fixed_q > VP8_MAXQ) oxcf->fixed_q = VP8_MAXQ;

  oxcf->target_bandwidth = 1000 * rc_cfg->target_bandwidth;

  oxcf->starting_buffer_level_in_ms = rc_cfg->buf_initial_sz;
  oxcf->optimal_buffer_level_in_ms = rc_cfg->buf_optimal_sz;
  oxcf->maximum_buffer_size_in_ms = rc_cfg->buf_sz;

#if 0
  //Fixme: This code is taken from vp9, may be Assign static values as in livpx vp8 code?
  {
    //Calculate the buffer levels from buffer levels in ms */
    const int64_t bandwidth = oxcf->target_bandwidth;
    const int64_t starting = rc_cfg->buf_initial_sz;
    const int64_t optimal = rc_cfg->buf_optimal_sz;
    const int64_t maximum = rc_cfg->buf_sz;

    oxcf->starting_buffer_level = starting * bandwidth / 1000;
    oxcf->optimal_buffer_level =
      (optimal == 0) ? bandwidth / 8 : optimal * bandwidth / 1000;
    oxcf->maximum_buffer_size =
      (maximum == 0) ? bandwidth / 8 : maximum * bandwidth / 1000;
  }
#endif
  
  oxcf->starting_buffer_level = rc_cfg->buf_initial_sz;
  oxcf->optimal_buffer_level = rc_cfg->buf_optimal_sz;
  oxcf->maximum_buffer_size = rc_cfg->buf_sz;

  oxcf->starting_buffer_level = rescale(
      (int)oxcf->starting_buffer_level, oxcf->target_bandwidth, 1000);

  /* Set or reset optimal and maximum buffer levels. */
  if (oxcf->optimal_buffer_level == 0) {
    oxcf->optimal_buffer_level = oxcf->target_bandwidth / 8;
  } else {
    oxcf->optimal_buffer_level = rescale(
        (int)oxcf->optimal_buffer_level, oxcf->target_bandwidth, 1000);
  }
  if (oxcf->maximum_buffer_size == 0) {
    oxcf->maximum_buffer_size = oxcf->target_bandwidth / 8;
  } else {
    oxcf->maximum_buffer_size = rescale((int)oxcf->maximum_buffer_size,
                                            oxcf->target_bandwidth, 1000);
  }

  // Under a configuration change, where maximum_buffer_size may change,
  // keep buffer level clipped to the maximum allowed buffer size.
  if (cpi_->bits_off_target > oxcf->maximum_buffer_size) {
    cpi_->bits_off_target = oxcf->maximum_buffer_size;
    cpi_->buffer_level = cpi_->bits_off_target;
  }
  /* Set up frame rate and related parameters rate control values. */
  vp8_new_framerate(cpi_, cpi_->framerate);

  /* Set absolute upper and lower quality limits */
  cpi_->worst_quality = oxcf->worst_allowed_q;
  cpi_->best_quality = oxcf->best_allowed_q;
  /* active values should only be modified if out of new range */
  if (cpi_->active_worst_quality > oxcf->worst_allowed_q) {
    cpi_->active_worst_quality = oxcf->worst_allowed_q;
  }
  /* less likely */
  else if (cpi_->active_worst_quality < oxcf->best_allowed_q) {
    cpi_->active_worst_quality = oxcf->best_allowed_q;
  }
  if (cpi_->active_best_quality < oxcf->best_allowed_q) {
    cpi_->active_best_quality = oxcf->best_allowed_q;
  }
  /* less likely */
  else if (cpi_->active_best_quality > oxcf->worst_allowed_q) {
    cpi_->active_best_quality = oxcf->worst_allowed_q;
  }

  cpi_->buffered_mode = oxcf->optimal_buffer_level > 0;

  cpi_->cq_target_quality = oxcf->cq_level;

  /* Only allow dropped frames in buffered mode */
  cpi_->drop_frames_allowed = oxcf->allow_df && cpi_->buffered_mode;

  cpi_->target_bandwidth = oxcf->target_bandwidth;

  if (oxcf->fixed_q >= 0) {
    cpi_->last_q[0] = oxcf->fixed_q;
    cpi_->last_q[1] = oxcf->fixed_q;
  }

  /* force to allowlag to 0 if lag_in_frames is 0; */
  if (oxcf->lag_in_frames == 0) {
    oxcf->allow_lag = 0;
  }
#if 0
  /* Limit on lag buffers as these are not currently dynamically allocated */
  else if (oxcf->lag_in_frames > MAX_LAG_BUFFERS) {
    oxcf->lag_in_frames = MAX_LAG_BUFFERS;
  }
#endif


  /* Initialize active best and worst q and average q values. */
  cpi_->active_worst_quality = cpi_->oxcf.worst_allowed_q;
  cpi_->active_best_quality = cpi_->oxcf.best_allowed_q;
  cpi_->avg_frame_qindex = cpi_->oxcf.worst_allowed_q;

   /* Initialise the starting buffer levels */
  cpi_->buffer_level = oxcf->starting_buffer_level;
  cpi_->bits_off_target = oxcf->starting_buffer_level;

  cpi_->rolling_target_bits = cpi_->av_per_frame_bandwidth;
  cpi_->rolling_actual_bits = cpi_->av_per_frame_bandwidth;
  cpi_->long_rolling_target_bits = cpi_->av_per_frame_bandwidth;
  cpi_->long_rolling_actual_bits = cpi_->av_per_frame_bandwidth;

  cpi_->total_actual_bits = 0;
  cpi_->total_target_vs_actual = 0;

#if 0
  /* Temporal scalabilty */
  if (cpi->oxcf.number_of_layers > 1) {
    unsigned int i;
    double prev_layer_framerate = 0;

    for (i = 0; i < cpi->oxcf.number_of_layers; ++i) {
      init_temporal_layer_context(cpi, oxcf, i, prev_layer_framerate);
      prev_layer_framerate =
          cpi->output_framerate / cpi->oxcf.rate_decimator[i];
    }
  }
#endif

  /* Should we use the cyclic refresh method.
   * Currently there is no external control for this.
   * Enable it for error_resilient_mode, or for 1 pass CBR mode.
   */
  cpi_->cyclic_refresh_mode_enabled =
      (cpi_->oxcf.error_resilient_mode ||
       (cpi_->oxcf.end_usage == USAGE_STREAM_FROM_SERVER &&
        cpi_->oxcf.Mode <= 2));
  cpi_->cyclic_refresh_mode_max_mbs_perframe =
      (cpi_->common.mb_rows * cpi_->common.mb_cols) / 7;
  if (cpi_->oxcf.number_of_layers == 1) {
    cpi_->cyclic_refresh_mode_max_mbs_perframe =
        (cpi_->common.mb_rows * cpi_->common.mb_cols) / 20;
  } else if (cpi_->oxcf.number_of_layers == 2) {
    cpi_->cyclic_refresh_mode_max_mbs_perframe =
        (cpi_->common.mb_rows * cpi_->common.mb_cols) / 10;
  }
  cpi_->cyclic_refresh_mode_index = 0;
  cpi_->cyclic_refresh_q = 32;

  cpi_->first_time_stamp_ever = 0x7FFFFFFF;

  cpi_->frames_till_gf_update_due = 0;
  cpi_->key_frame_count = 1;

  cpi_->ni_av_qi = cpi_->oxcf.worst_allowed_q;
  cpi_->ni_tot_qi = 0;
  cpi_->ni_frames = 0;
  cpi_->total_byte_count = 0;

  cpi_->drop_frame = 0;

  cpi_->rate_correction_factor = 1.0;
  cpi_->key_frame_rate_correction_factor = 1.0;
  cpi_->gf_rate_correction_factor = 1.0;
  cpi_->compressor_speed = 1;

}

void brc_vp8_init_rate_control(VP8RateControlRTC *rtc, VP8RateControlRtcConfig *rc_cfg) {
  VP8_COMP *cpi_ = &rtc->cpi_;

  cpi_->baseline_gf_interval = DEFAULT_GF_INTERVAL;

  cpi_->gold_is_last = 0;
  cpi_->alt_is_last = 0;
  cpi_->gold_is_alt = 0;

  cpi_->temporal_layer_id = -1;

  brc_vp8_update_rate_control(rtc, rc_cfg);
  
#if 0
  RATE_CONTROL *const rc = &cpi_->rc;
  cm->profile = PROFILE_0;
  cm->bit_depth = VPX_BITS_8;
  cm->show_frame = 1;
  oxcf->rc_mode = VPX_CBR;
  oxcf->pass = 0;
  oxcf->aq_mode = NO_AQ;
  oxcf->content = VP8E_CONTENT_DEFAULT;
  oxcf->drop_frames_water_mark = 0;

  cpi_->use_svc = (cpi_->svc.number_spatial_layers > 1 ||
                   cpi_->svc.number_temporal_layers > 1)
                      ? 1
                      : 0;
  rc->rc_1_frame = 0;
  rc->rc_2_frame = 0;
  //vp8_change_config (cpi_, oxcf);
  vp8_rc_init_minq_luts();
  vp8_rc_init(oxcf, 0, rc);

  cpi_->sf.use_nonrd_pick_mode = 1;
  cm->current_video_frame = 0;
#endif
}

VP8RateControlRTC *
brc_vp8_rate_control_new (VP8RateControlRtcConfig *cfg) {
  VP8RateControlRTC *rtc = (VP8RateControlRTC*) malloc (sizeof (VP8RateControlRTC));
  if (!rtc)
    return NULL;
  memset (&rtc->cpi_, 0, sizeof (rtc->cpi_));
  brc_vp8_init_rate_control (rtc, cfg);
  return rtc;
}

VP8RateControlStatus
brc_vp8_validate (VP8RateControlRtcConfig *cfg)
{
  VP8RateControlStatus status = STATUS_BRC_VP8_SUCCESS;

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

void
brc_vp8_rate_control_free (VP8RateControlRTC *rtc) {
  if (rtc)
    free(rtc);
}
