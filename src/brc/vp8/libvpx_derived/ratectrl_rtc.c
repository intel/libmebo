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

static const unsigned char kf_high_motion_minq[VP8_QINDEX_RANGE] = {
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  1,  1,  1,  1,  1,  1,
  1,  1,  2,  2,  2,  2,  3,  3,  3,  3,  3,  3,  3,  3,  4,  4,  4,  4,  5,
  5,  5,  5,  5,  5,  6,  6,  6,  6,  7,  7,  8,  8,  8,  8,  9,  9,  10, 10,
  10, 10, 11, 11, 11, 11, 12, 12, 13, 13, 13, 13, 14, 14, 15, 15, 15, 15, 16,
  16, 16, 16, 17, 17, 18, 18, 18, 18, 19, 19, 20, 20, 20, 20, 21, 21, 21, 21,
  22, 22, 23, 23, 24, 25, 25, 26, 26, 27, 28, 28, 29, 30
};

static const unsigned char inter_minq[VP8_QINDEX_RANGE] = {
  0,  0,  1,  1,  2,  3,  3,  4,  4,  5,  6,  6,  7,  8,  8,  9,  9,  10, 11,
  11, 12, 13, 13, 14, 15, 15, 16, 17, 17, 18, 19, 20, 20, 21, 22, 22, 23, 24,
  24, 25, 26, 27, 27, 28, 29, 30, 30, 31, 32, 33, 33, 34, 35, 36, 36, 37, 38,
  39, 39, 40, 41, 42, 42, 43, 44, 45, 46, 46, 47, 48, 49, 50, 50, 51, 52, 53,
  54, 55, 55, 56, 57, 58, 59, 60, 60, 61, 62, 63, 64, 65, 66, 67, 67, 68, 69,
  70, 71, 72, 73, 74, 75, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 86,
  87, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100
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
}

int brc_vp8_get_qp(VP8RateControlRTC *rtc) {
  VP8_COMP *cpi_ = &rtc->cpi_;

  return cpi_->common.base_qindex;
}

int brc_vp8_get_loop_filter_level(VP8RateControlRTC *rtc) {
  fprintf(stderr, "%s \n", "Warning: Not supported");
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

  /* Reduce active_worst_allowed_q for CBR if our buffer is getting too full.
   * This has a knock on effect on active best quality as well.
   * For CBR if the buffer reaches its maximum level then we can no longer
   * save up bits for later frames so we might as well use them up
   * on the current frame.
   */
  if ((cpi_->buffer_level >= cpi_->oxcf.optimal_buffer_level) &&
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

   /* Set an active best quality and if necessary active worst quality
   * There is some odd behavior for one pass here that needs attention.
   */
  if (cpi_->ni_frames > 150) {
    Q = cpi_->active_worst_quality;

    /* One pass more conservative */
    if (cm->frame_type == VP8_KEY_FRAME) {
      cpi_->active_best_quality = kf_high_motion_minq[Q];
    } else {
      cpi_->active_best_quality = inter_minq[Q];
    }
    /* If CBR and the buffer is as full then it is reasonable to allow
     * higher quality on the frames to prevent bits just going to waste.
     */
    /* Note that the use of >= here elliminates the risk of a devide
     * by 0 error in the else if clause
     */
    if (cpi_->buffer_level >= cpi_->oxcf.maximum_buffer_size) {
      cpi_->active_best_quality = cpi_->best_quality;

    } else if (cpi_->buffer_level > cpi_->oxcf.optimal_buffer_level) {
      int Fraction =
          (int)(((cpi_->buffer_level - cpi_->oxcf.optimal_buffer_level) * 128) /
                (cpi_->oxcf.maximum_buffer_size -
                 cpi_->oxcf.optimal_buffer_level));
      int min_qadjustment =
          ((cpi_->active_best_quality - cpi_->best_quality) * Fraction) / 128;

      cpi_->active_best_quality -= min_qadjustment;
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

  vp8_compute_frame_size_bounds(cpi_, &frame_under_shoot_limit,
                                &frame_over_shoot_limit);

  cm->base_qindex = Q;

  //vp8_setup_key_frame
  if (cm->frame_type == VP8_KEY_FRAME) {
    cpi_->common.filter_level = cpi_->common.base_qindex * 3 / 8;

    /* Provisional interval before next GF */
    cpi_->frames_till_gf_update_due = DEFAULT_GF_INTERVAL;
  }

  if (cpi_->pass == 0) {
      if (vp8_drop_encodedframe_overshoot(cpi_, Q))
        return;
  }

  if (frame_over_shoot_limit == 0)
    frame_over_shoot_limit = 1;

  /* Are we are overshooting and up against the limit of active max Q. */
  if ((cpi_->pass != 2) &&
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
  }
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

  cpi_->ref_framerate = cpi_->framerate = rc_cfg->framerate;

  //Fill VP8_CONFIG
 
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

  /* At the moment the first order values may not be > MAXQ */
  if (oxcf->fixed_q > VP8_MAXQ) oxcf->fixed_q = VP8_MAXQ;

  oxcf->target_bandwidth = 1000 * rc_cfg->target_bandwidth;

  oxcf->starting_buffer_level_in_ms = rc_cfg->buf_initial_sz;
  oxcf->optimal_buffer_level_in_ms = rc_cfg->buf_optimal_sz;
  oxcf->maximum_buffer_size_in_ms = rc_cfg->buf_sz;

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

  cpi_->target_bandwidth = oxcf->target_bandwidth;

  if (oxcf->fixed_q >= 0) {
    cpi_->last_q[0] = oxcf->fixed_q;
    cpi_->last_q[1] = oxcf->fixed_q;
  }

  if (!cpi_->initial_width) {
    cpi_->initial_width = cpi_->oxcf.Width;
    cpi_->initial_height = cpi_->oxcf.Height;
  }

  assert(cm->Width <= cpi_->initial_width);
  assert(cm->Height <= cpi_->initial_height);

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

  /* Give a sensible default for the first frame. */
  cpi_->frames_since_key = 8;
  oxcf->key_freq = 30;//Custom: Not taken from vpxenc
  cpi_->key_frame_frequency = oxcf->key_freq;

  cpi_->frames_till_gf_update_due = 0;
  cpi_->key_frame_count = 1;

  /* Prime the recent reference frame usage counters.
   * Hereafter they will be maintained as a sort of moving average
   */
  cpi_->recent_ref_frame_usage[INTRA_FRAME] = 1;
  cpi_->recent_ref_frame_usage[LAST_FRAME] = 1;
  cpi_->recent_ref_frame_usage[GOLDEN_FRAME] = 1;
  cpi_->recent_ref_frame_usage[ALTREF_FRAME] = 1;

  cpi_->ni_av_qi = cpi_->oxcf.worst_allowed_q;
  cpi_->ni_tot_qi = 0;
  cpi_->ni_frames = 0;
  cpi_->total_byte_count = 0;

  cpi_->drop_frame = 0;

  cpi_->rate_correction_factor = 1.0;
  cpi_->key_frame_rate_correction_factor = 1.0;
  cpi_->gf_rate_correction_factor = 1.0;
  cpi_->compressor_speed = 1;

  for (int i = 0; i < KEY_FRAME_CONTEXT; ++i) {
    cpi_->prior_key_frame_distance[i] = (int)cpi_->output_framerate;
  }
}

void brc_vp8_init_rate_control(VP8RateControlRTC *rtc, VP8RateControlRtcConfig *rc_cfg) {
  VP8_COMP *cpi_ = &rtc->cpi_;
  VP8_CONFIG *oxcf = &cpi_->oxcf;

  cpi_->baseline_gf_interval = DEFAULT_GF_INTERVAL;

  oxcf->cq_level = 10;//Fixme?
  oxcf->number_of_layers = 1;

  brc_vp8_update_rate_control(rtc, rc_cfg);
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

  RANGE_CHECK(cfg, width, 1, 16383);
  RANGE_CHECK(cfg, height, 1, 16383);
  RANGE_CHECK_HI(cfg, max_quantizer, 63);
  RANGE_CHECK_HI(cfg, min_quantizer, cfg->max_quantizer);
  RANGE_CHECK_HI(cfg, undershoot_pct, 1000);
  RANGE_CHECK_HI(cfg, overshoot_pct, 1000);
  RANGE_CHECK(cfg, ss_number_layers, 1, 1);
  RANGE_CHECK(cfg, ts_number_layers, 1, 1);

  if (cfg->ss_number_layers * cfg->ts_number_layers > VP8_MAX_LAYERS)
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
