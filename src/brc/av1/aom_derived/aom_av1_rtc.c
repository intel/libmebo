/*
 *  Copyright (c) 2020 Intel Corporation. All Rights Reserved.
 *  Copyright (c) 2020 Sreerenj Balachandran <sreerenj.balachandran@intel.com>
 *
 *  Author: Sreerenj Balachandran <sreerenj.balachandran@intel.com>
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "aom_av1_rtc.h"

#undef ERROR
#define ERROR(str)                  \
  do {                              \
    fprintf(stderr, "%s \n", str);	    \
    return STATUS_BRC_AV1_INVALID_PARAM; \
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

static inline int calc_mi_size(int len) {
  // len is in mi units. Align to a multiple of SBs.
  return ALIGN_POWER_OF_TWO(len, MAX_MIB_SIZE_LOG2);
}

/*===== BRC functions exposed to libs implementation ========= */

LibMeboStatus
brc_av1_post_encode_update(BrcCodecEnginePtr engine_ptr, uint64_t encoded_frame_size) {
  AV1RateControlRTC *rtc = (AV1RateControlRTC *) engine_ptr;
  AV1_COMP *cpi = &rtc->cpi_;
  av1_rc_postencode_update(cpi, encoded_frame_size);

  //ToDo: Make it only for show_frame == true case
  cpi->common.current_frame.frame_number++;

  //Fixme: move to calculate_qp() function?
  //taken from encode_frame_to_data_rate()
  if (cpi->svc.spatial_layer_id == cpi->svc.number_spatial_layers - 1)
    cpi->svc.num_encoded_top_layer++;
  return LIBMEBO_STATUS_SUCCESS;
}

LibMeboStatus
brc_av1_get_qp(BrcCodecEnginePtr engine_ptr, int *qp) {
  AV1RateControlRTC *rtc = (AV1RateControlRTC *) engine_ptr;
  AV1_COMP *cpi = &rtc->cpi_;
  *qp = cpi->common.quant_params.base_qindex;
  return LIBMEBO_STATUS_SUCCESS;
}

LibMeboStatus
brc_av1_get_loop_filter_level(BrcCodecEnginePtr engine_ptr, int *filter_level) {
  *filter_level = 0;
#if 0
  AV1_COMP *cpi_ = &rtc->cpi_;
  struct loopfilter *const lf = &cpi_->common.lf;
  av1_pick_filter_level(cpi_, LPF_PICK_FROM_Q);
  return lf->filter_level;
#endif
  //ToDo:
  //check  loopfilter_frame(cpi, cm);
   return LIBMEBO_STATUS_SUCCESS;
}

static inline void update_keyframe_counters(AV1_COMP *cpi) {
  if (cpi->common.show_frame && cpi->rc.frames_to_key) {
    cpi->rc.frames_since_key++;
    cpi->rc.frames_to_key--;
  }
}

static void adjust_frame_rate(AV1_COMP *cpi/*, int64_t ts_start, int64_t ts_end*/) {
  if (cpi->use_svc && cpi->svc.spatial_layer_id > 0) {
    cpi->framerate = cpi->svc.base_framerate;
    av1_rc_update_framerate(cpi, cpi->common.width, cpi->common.height);
    return;
  }
  //ToDo: Add fine tuning of framrate based on encode time
}

//Code derived from encoder_encode() in av1_cx_iface.c
LibMeboStatus
brc_av1_compute_qp (BrcCodecEnginePtr engine_ptr, LibMeboRCFrameParams *frame_params) {
  AV1RateControlRTC *rtc = (AV1RateControlRTC *) engine_ptr;
  AV1_COMP *cpi = &rtc->cpi_;
  AV1EncoderConfig *oxcf = &cpi->oxcf;
  AV1_COMMON *const cm = &cpi->common;
  int top_index = 0, bottom_index = 0, q = 0;

  if (!cpi->initial_dimensions.width){
    cpi->initial_dimensions.width = cm->width;
    cpi->initial_dimensions.height = cm->height;
    cpi->initial_mbs = cm->mi_params.MBs;

    //ToDo: Add speed feature setting
  }

  //Taken from av1_get_compressed_data()
  if (cpi->use_svc && cm->number_spatial_layers > 1) {
    av1_one_pass_cbr_svc_start_layer(cpi);
  }

  //Taken from av1_encode_strategy()
  adjust_frame_rate(cpi /*, source->ts_start, source->ts_end*/);

  av1_get_one_pass_rt_params(cpi, frame_params->frame_type);
  //Not configured for CONFIG_REALTIME_ONLY, so the codepath
  //is derived from av1_get_second_pass_params()
  //Fixme:
  //if (gf_group->index < gf_group->size && !(frame_flags & FRAMEFLAGS_KEY))
  //{
    //begin: setup_target_rate() 
    //Fixme: Very Important: without this the whole algo might not work
    //int target_rate = gf_group->bit_allocation[gf_group->index];
    //int target_rate = 0;
    //av1_rc_set_frame_target(cpi, target_rate, cpi->common.width,
      //                      cpi->common.height);
    //rc->base_frame_target = target_rate;
    //end: setup_target_rate() 
  //}
  //{
    //Fixme: cq_level is usually for CQP, find a prper default???
    //rc->active_worst_quality = 40;//oxcf->rc_cfg.cq_level;
  //}


  //ToDo: Add show_existing_frame support
  //if (!frame_params.show_existing_frame)
  cm->quant_params.using_qmatrix = oxcf->q_cfg.using_qm;

  //We use the real_time mode for encode
  //denoise_and_encode()
  //ToDo: Add lf params: set_default_lf_deltas(&cm->lf);
  cm->current_frame.frame_type = frame_params->frame_type; 

  //=======derived from av1_encode()==========
  if (cm->current_frame.frame_type == AV1_KEY_FRAME) {
    cm->current_frame.frame_number = 0;
    //Fixme: Ensure the settings?
    memset (&cm->prev_frame, 0, sizeof (cm->prev_frame));
    cm->prev_frame.width = cm->width;
    cm->prev_frame.height = cm->height;
  }
  //encode_frame_to_data_rate()
  cpi->last_frame_type = cm->current_frame.frame_type;
  //ToDo:
  //Add support for : encode_show_existing_frame(cm)

  //ToDo:
  // For 1 pass CBR, check if we are dropping this frame.
  // Never drop on key frame.
#if 0
  if (has_no_stats_stage(cpi) && oxcf->rc_cfg.mode == AOM_CBR &&
      current_frame->frame_type != AV1_KEY_FRAME) {
    if (av1_rc_drop_frame(cpi)) {
      av1_setup_frame_size(cpi);
      av1_rc_postencode_update_drop_frame(cpi);
    }
  }
#endif
  //encode_with_recode_loop_and_filter()
  //encode_without_recode()
  //Encode a frame without the recode loop, usually used in one-pass
  //encoding and realtime coding.

  //Derived from av1_set_size_dependent_vars(cpi, &q, &bottom_index, &top_index);
   // Decide q and q bounds.
  q = av1_rc_pick_q_and_bounds(cpi, &cpi->rc, cm->width, cm->height,
                                /* cpi->gf_group.index,*/ &bottom_index, &top_index);

  cm->quant_params.base_qindex = q;
  //Fixme (Important):
  //Check the requirements of below fuctions in the aom code
  //av1_set_quantizer(cm, q_cfg->qm_minlevel, q_cfg->qm_maxlevel, q,
  //                  q_cfg->enable_chroma_deltaq);
  //av1_set_speed_features_qindex_dependent(cpi, cpi->oxcf.speed);
  //if (q_cfg->deltaq_mode != NO_DELTA_Q)
  //  av1_init_quantizer(&cpi->enc_quant_dequant_params, &cm->quant_params,
  //                     cm->seq_params.bit_depth);

  //ToDo:
  //Add support for overshoot detection for slide scene chagne
  //if (cpi->sf.rt_sf.overshoot_detection_cbr == FAST_DETECTION_MAXQ)
  //  av1_encodedframe_overshoot_cbr ()

  //ToDo
  //Add support for CYCLIC_REFRESH_AQ)

  //Derived from update_rc_counts()
  update_keyframe_counters(cpi);
  //ToDo: Add support for
  // update_frames_till_gf_update(cpi);
  // update_gf_group_index(cpi);
  // Fixme: move to post_encode_update() ????
  if (cpi->use_svc)
    av1_save_layer_context(cpi);
  return LIBMEBO_STATUS_SUCCESS;
}

static void set_rc_buffer_sizes(AV1_RATE_CONTROL *rc,
    const RateControlCfg *rc_cfg) {
  const int64_t bandwidth = rc_cfg->target_bandwidth;
  const int64_t starting = rc_cfg->starting_buffer_level_ms;
  const int64_t optimal = rc_cfg->optimal_buffer_level_ms;
  const int64_t maximum = rc_cfg->maximum_buffer_size_ms;

  rc->starting_buffer_level = starting * bandwidth / 1000;
  rc->optimal_buffer_level =
      (optimal == 0) ? bandwidth / 8 : optimal * bandwidth / 1000;
  rc->maximum_buffer_size =
      (maximum == 0) ? bandwidth / 8 : maximum * bandwidth / 1000;
}

void av1_new_framerate(AV1_COMP *cpi, double framerate) {
  cpi->framerate = framerate < 0.1 ? 30 : framerate;
  av1_rc_update_framerate(cpi, cpi->common.width, cpi->common.height);
}

static inline void init_frame_info(FRAME_INFO *frame_info,
                                   const AV1_COMMON *const cm) {
  const CommonModeInfoParams *const mi_params = &cm->mi_params;
  const SequenceHeader *const seq_params = &cm->seq_params;
  frame_info->frame_width = cm->width;
  frame_info->frame_height = cm->height;
  frame_info->mi_cols = mi_params->mi_cols;
  frame_info->mi_rows = mi_params->mi_rows;
  frame_info->mb_cols = mi_params->mb_cols;
  frame_info->mb_rows = mi_params->mb_rows;
  frame_info->num_mbs = mi_params->MBs;
  frame_info->bit_depth = seq_params->bit_depth;
}

static inline TX_SIZE av1_get_adjusted_tx_size(TX_SIZE tx_size) {
  switch (tx_size) {
    case TX_64X64:
    case TX_64X32:
    case TX_32X64: return TX_32X32;
    case TX_64X16: return TX_32X16;
    case TX_16X64: return TX_16X32;
    default: return tx_size;
  }
}

static const qm_val_t wt_matrix_ref[AV1_NUM_QM_LEVELS - 1][2][QM_TOTAL_SIZE];
static const qm_val_t iwt_matrix_ref[AV1_NUM_QM_LEVELS - 1][2][QM_TOTAL_SIZE];
static const int tx_size_2d[TX_SIZES_ALL + 1] = { 
  16,  64,   256,  1024, 4096, 32,  32,  128,  128,  512,
  512, 2048, 2048, 64,   64,   256, 256, 1024, 1024,
};

void av1_qm_init(CommonQuantParams *quant_params, int num_planes) {
  for (int q = 0; q < AV1_NUM_QM_LEVELS; ++q) {
    for (int c = 0; c < num_planes; ++c) {
      int current = 0;
      for (int t = 0; t < TX_SIZES_ALL; ++t) {
        const int size = tx_size_2d[t];
        const int qm_tx_size = av1_get_adjusted_tx_size(t);
        if (q == AV1_NUM_QM_LEVELS - 1) {
          quant_params->gqmatrix[q][c][t] = NULL;
          quant_params->giqmatrix[q][c][t] = NULL;
        } else if (t != qm_tx_size) {  // Reuse matrices for 'qm_tx_size'
          assert(t > qm_tx_size);
          quant_params->gqmatrix[q][c][t] =
              quant_params->gqmatrix[q][c][qm_tx_size];
          quant_params->giqmatrix[q][c][t] =
              quant_params->giqmatrix[q][c][qm_tx_size];
        } else {
          assert(current + size <= QM_TOTAL_SIZE);
          quant_params->gqmatrix[q][c][t] = &wt_matrix_ref[q][c >= 1][current];
          quant_params->giqmatrix[q][c][t] =
              &iwt_matrix_ref[q][c >= 1][current];
          current += size;
        }
      }
    }
  }
}

static inline int av1_num_planes(const AV1_COMMON *cm) {
  return cm->seq_params.monochrome ? 1 : MAX_MB_PLANE;
}

LibMeboStatus
brc_av1_update_rate_control(BrcCodecEnginePtr engine_ptr,
    LibMeboRateControllerConfig *input_rc_cfg) {
  AV1RateControlRTC *rtc = (AV1RateControlRTC *) engine_ptr;
  AV1_COMP *cpi = &rtc->cpi_;
  AV1_COMMON *cm = &cpi->common;
  AV1EncoderConfig *oxcf = &cpi->oxcf;
  AV1_RATE_CONTROL *const rc = &cpi->rc;

  RateControlCfg *const rc_cfg = &oxcf->rc_cfg;
  QuantizationCfg *const q_cfg = &oxcf->q_cfg;
  CommonModeInfoParams *mi_params = &cm->mi_params;

  oxcf->frm_dim_cfg.width = input_rc_cfg->width;
  oxcf->frm_dim_cfg.height = input_rc_cfg->height;

  if (oxcf->input_cfg.init_framerate > 180)
    oxcf->input_cfg.init_framerate = 30;
  else
    oxcf->input_cfg.init_framerate = input_rc_cfg->framerate;

  oxcf->kf_cfg.key_freq_max = (int)input_rc_cfg->framerate;
  oxcf->kf_cfg.key_freq_min = oxcf->kf_cfg.key_freq_max;

  // Set Rate Control configuration.
  rc_cfg->max_intra_bitrate_pct = input_rc_cfg->max_intra_bitrate_pct;
  rc_cfg->max_inter_bitrate_pct = input_rc_cfg->max_inter_bitrate_pct;

  //Fixme: Find a proper default or add global attribute
  //rc_cfg->gf_cbr_boost_pct = extra_cfg->gf_cbr_boost_pct;

  rc_cfg->mode = AOM_CBR;//ToDo: Add dynamic control
  //rc_cfg->min_cr = extra_cfg->min_cr;
  rc_cfg->best_allowed_q = av1_quantizer_to_qindex(input_rc_cfg->min_quantizer);
  rc_cfg->worst_allowed_q = av1_quantizer_to_qindex(input_rc_cfg->max_quantizer);
  rc_cfg->under_shoot_pct = input_rc_cfg->undershoot_pct;
  rc_cfg->over_shoot_pct = input_rc_cfg->overshoot_pct;
  rc_cfg->maximum_buffer_size_ms = input_rc_cfg->buf_sz;
  rc_cfg->starting_buffer_level_ms = input_rc_cfg->buf_initial_sz;
  rc_cfg->optimal_buffer_level_ms = input_rc_cfg->buf_optimal_sz;
  rc_cfg->target_bandwidth = 1000 * input_rc_cfg->target_bandwidth;
  rc_cfg->drop_frames_water_mark = 0;
  rc_cfg->vbr_corpus_complexity_lap = 0;// default
  rc_cfg->vbrbias = 50; //default
  rc_cfg->vbrmin_section = 0; //default
  rc_cfg->vbrmax_section = 2000; //default

  // Set Quantization related configuration.
  q_cfg->using_qm = 0;
  q_cfg->qm_minlevel = AV1_DEFAULT_QM_FIRST;
  q_cfg->qm_maxlevel = AV1_DEFAULT_QM_LAST;
  q_cfg->quant_b_adapt = 0;
  q_cfg->enable_chroma_deltaq = 0;
  q_cfg->aq_mode = 0;
  /*
   * 0:no deltaq
   * 1: Modulation to improve objective quality
   * 2: Modulation to improve perceptual quality
   */
  q_cfg->deltaq_mode = 1;
  q_cfg->use_fixed_qp_offsets = 0;//NO CQP
  for (int i = 0; i < FIXED_QP_OFFSET_COUNT; ++i) {
      q_cfg->fixed_qp_offsets[i] = -1.0;
  }

  //Fill AV1_COMP structure
  cpi->oxcf = *oxcf;
  cpi->framerate = oxcf->input_cfg.init_framerate; 
  cpi->superres_mode = AOM_SUPERRES_NONE;
  cpi->svc.number_spatial_layers = input_rc_cfg->ss_number_layers;
  cpi->svc.number_temporal_layers = input_rc_cfg->ts_number_layers;

  //Fill AV1_COMMON structure
  cm->prev_frame.width = cm->width;
  cm->prev_frame.height= cm->height;
  if (!cm->prev_frame.height || !cm->prev_frame.width)
    cm->prev_frame.has_prev_frame= 0;
  cm->width = input_rc_cfg->width;
  cm->height = input_rc_cfg->height;
  cm->seq_params.bit_depth= AOM_BITS_8;
  cm->seq_params.monochrome = 0;
  cm->number_spatial_layers = cpi->svc.number_spatial_layers;
  cm->number_temporal_layers = cpi->svc.number_temporal_layers;
  set_rc_buffer_sizes(rc, rc_cfg);

  // Under a configuration change, where maximum_buffer_size may change,
  // keep buffer level clipped to the maximum allowed buffer size.
  rc->bits_off_target = AOMMIN(rc->bits_off_target, rc->maximum_buffer_size);
  rc->buffer_level = AOMMIN(rc->buffer_level, rc->maximum_buffer_size);

  // Set up frame rate and related parameters rate control values.
  av1_new_framerate(cpi, cpi->framerate);

  // Set absolute upper and lower quality limits
  rc->worst_quality = rc_cfg->worst_allowed_q;
  rc->best_quality = rc_cfg->best_allowed_q;

  //Fixme: Check the requirement of
  // if (initial_dimensions->width || sb_size != seq_params->sb_size)
  //

  //We only need some part of update_frame_size(cpi)
  {
    const int aligned_width = ALIGN_POWER_OF_TWO(input_rc_cfg->width, 3);
    const int aligned_height = ALIGN_POWER_OF_TWO(input_rc_cfg->height, 3);

    mi_params->mi_cols = aligned_width >> MI_SIZE_LOG2;
    mi_params->mi_rows = aligned_height >> MI_SIZE_LOG2;

    mi_params->mb_cols = (mi_params->mi_cols + 2) >> 2;
    mi_params->mb_rows = (mi_params->mi_rows + 2) >> 2;
    mi_params->MBs = mi_params->mb_rows * mi_params->mb_cols;
  }

  if (cpi->use_svc)
    av1_update_layer_context_change_config(cpi, input_rc_cfg->target_bandwidth);

  av1_rc_init(oxcf, oxcf->pass, rc);

  init_frame_info(&cpi->frame_info, cm);

  //TakenFrom:
  //av1_set_speed_features_framesize_independent();
  //init_hl_sf();
  // Recode loop tolerance %.
  cpi->sf.hl_sf.recode_tolerance = 25;
  //No recode or trellis for 1 pass.
  cpi->sf.hl_sf.recode_loop = 0;
  if (oxcf->mode == 1) //REALTIME
  {
    cpi->sf.rt_sf.check_scene_detection = 0;
    cpi->sf.rt_sf.overshoot_detection_cbr = NO_DETECTION;

    if (cpi->speed >= 5) //Currently we only use 0
    {
      cpi->sf.rt_sf.check_scene_detection = 1;
      if (cm->current_frame.frame_type != AV1_KEY_FRAME &&
        cpi->oxcf.rc_cfg.mode == AOM_CBR)
        cpi->sf.rt_sf.overshoot_detection_cbr = FAST_DETECTION_MAXQ;
    }
  }
  return LIBMEBO_STATUS_SUCCESS;
}

static void
brc_init_rate_control(AV1RateControlRTC *rtc, LibMeboRateControllerConfig *rc_cfg) {
  AV1_COMP *cpi = &rtc->cpi_;
  AV1_COMMON *cm = &cpi->common;
  AV1EncoderConfig *oxcf = &cpi->oxcf;

  oxcf->profile = AV1_PROFILE_0;//Fixme: dynamic setting or bitdepth based calculation 
  oxcf->pass = 0; //Fixme: Add dynamic configuration option
  oxcf->mode = AOM_USAGE_GOOD_QUALITY;//Fixme: configure for REAL_TIME?
  oxcf->tune_cfg.content = AOM_CONTENT_DEFAULT;

  cm->current_frame.frame_number = 0;

  // init SVC parameters.
  cpi->use_svc = 0;
  cpi->svc.external_ref_frame_config = 0;
  cpi->svc.non_reference_frame = 0;
  cpi->svc.number_spatial_layers = 1;
  cpi->svc.number_temporal_layers = 1;
  cm->number_spatial_layers = 1;
  cm->number_temporal_layers = 1;
  cm->spatial_layer_id = 0;
  cm->temporal_layer_id = 0;
  cm->seq_params.bit_depth = AOM_BITS_8;
  brc_av1_update_rate_control(rtc, rc_cfg);

  av1_rc_init_minq_luts(); //void av1_initialize_enc(void)
  //Fixme: Add if needed, encoder.c
  //av1_init_quantizer(&cpi->enc_quant_dequant_params, &cm->quant_params,
  //                   cm->seq_params.bit_depth); 
  av1_qm_init(&cm->quant_params, av1_num_planes(cm));

  //ToDo: Add loopfilter support
  //av1_loop_filter_init ();//encoder.c
}

//Fixme: Use av1/av1_cx_iface.c  aom_codec_err_t validate_config
//as reference
LibMeboStatus
brc_av1_validate (LibMeboRateControllerConfig *cfg)
{
  LibMeboStatus status = LIBMEBO_STATUS_SUCCESS;

  RANGE_CHECK(cfg, width, 1, 65535);
  RANGE_CHECK(cfg, height, 1, 65535);
  RANGE_CHECK_HI(cfg, max_quantizer, 63);
  RANGE_CHECK_HI(cfg, min_quantizer, cfg->max_quantizer);
  //RANGE_CHECK(cfg, rc_end_usage, AOM_VBR, AOM_Q);
  RANGE_CHECK_HI(cfg, undershoot_pct, 100);
  RANGE_CHECK_HI(cfg, overshoot_pct, 100);
  RANGE_CHECK(cfg, ss_number_layers, 1, AOM_MAX_SS_LAYERS);
  RANGE_CHECK(cfg, ts_number_layers, 1, AOM_MAX_TS_LAYERS);

  if (cfg->ss_number_layers * cfg->ts_number_layers > AOM_MAX_LAYERS)
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
brc_av1_rate_control_init (LibMeboRateControllerConfig *cfg,
    BrcCodecEnginePtr *brc_codec_handler) {
  LibMeboStatus status = LIBMEBO_STATUS_SUCCESS;
  AV1RateControlRTC *rtc = NULL;

  status = brc_av1_validate (cfg);
  if (status != LIBMEBO_STATUS_SUCCESS)
    return status;

  rtc = (AV1RateControlRTC*) malloc (sizeof (AV1RateControlRTC));
  if (!rtc)
    return LIBMEBO_STATUS_FAILED;

  memset (&rtc->cpi_, 0, sizeof (rtc->cpi_));
  brc_init_rate_control (rtc, (BrcCodecEnginePtr)cfg);

  *brc_codec_handler = (BrcCodecEnginePtr)rtc;
  return LIBMEBO_STATUS_SUCCESS;
}

void
brc_av1_rate_control_free (BrcCodecEnginePtr engine_ptr) {
  AV1RateControlRTC *rtc = (AV1RateControlRTC *) engine_ptr;	
  if (rtc)
    free(rtc);
}
