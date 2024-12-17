#include "AV1RateControlHandler.hpp"
#include <dlfcn.h>
#include <iostream>
#include <cstring>

#include "../../aom/av1/ratectrl_rtc.h"

Libmebo_brc_AV1::Libmebo_brc_AV1(LibMeboBrcAlgorithmID algo_id)
    : Libmebo_brc(LIBMEBO_CODEC_AV1, algo_id), handle(nullptr),
      ptrCreateAV1Controller(nullptr) {
  std::cout<<"inside av1 constructor \n";
  enc_params_libmebo.num_sl = 1;
  enc_params_libmebo.num_tl = 1;
  enc_params_libmebo.bitrate = 288; // in kbps.
  enc_params_libmebo.dynamic_rate_change = 0;
  enc_params_libmebo.framecount = 100;
  enc_params_libmebo.framerate = 60;
  enc_params_libmebo.width = 320;
  enc_params_libmebo.height = 160;
  enc_params_libmebo.id = static_cast<unsigned int>(LIBMEBO_CODEC_AV1);
  enc_params_libmebo.preset = 0;
  enc_params_libmebo.buf_optimal_sz = 600;
}

Libmebo_brc_AV1::~Libmebo_brc_AV1() {
  if (handle) {
    dlclose(handle);
  }
}
#define av1aom_zero(dest) memset(&(dest), 0, sizeof(dest))
AomAV1RateControlRtcConfig *config;
void *controller;

int Libmebo_brc_AV1::InitSymbolsFromLiray() {
  char path[] = "/home/pradeep4/Documents/pradeep/libmebo_push/libmebo/aom/build/libaom_av1_rc.so"; // this needs to be at config time.
  handle = dlopen(path, RTLD_LAZY);
  if (!handle) {
    return kMainHandleLibError;
  }

  std::cout<<"lib opened ----"<<std::endl;

 std::cout<<"DEBUG -1  Openinng sym  av1_ratecontrol_rtc_create----"<<std::endl; 
  ptrCreateAV1Controller =
      (createAV1Controller_t)dlsym(handle, "av1_ratecontrol_rtc_create");
  if (!ptrCreateAV1Controller) {
    return kAv1CreateSymbLoadError;
  }
  std::cout<<"DEBUG -2  Openinng sym  av1_ratecontrol_rtc_init_ratecontrol_config----"<<std::endl;  
  ptrInitConfigFunc = (InitRateControlConfigFunc)dlsym(
      handle, "av1_ratecontrol_rtc_init_ratecontrol_config");

  if (!ptrInitConfigFunc) {
    return kAV1RateCtrlInitConfigSymbLoadError;
  }
  std::cout<<"DEBUG -3  Openinng sym  av1_ratecontrol_rtc_update----"<<std::endl; 
  ptrUpdateRateControl_AV1 =
      (UpdateRateControl_AV1_t)dlsym(handle, "av1_ratecontrol_rtc_update");
  if (!ptrUpdateRateControl_AV1) {
    return kUpdateRateControlSymbLoadError;
  }
  std::cout<<"DEBUG -4  Openinng sym  av1_ratecontrol_rtc_compute_qp----"<<std::endl;
  ptrComputeQP_AV1 =
      (ComputeQP_AV1_t)dlsym(handle, "av1_ratecontrol_rtc_compute_qp");
  if (!ptrComputeQP_AV1) {
    return kCompueQPSymbLoadError;
  }
 std::cout<<"DEBUG -5  Openinng sym  av1_ratecontrol_rtc_post_encode_update----"<<std::endl;
  ptrPostEncodeUpdate_AV1 = (PostEncodeUpdate_AV1_t)dlsym(
      handle, "av1_ratecontrol_rtc_post_encode_update");
  if (!ptrPostEncodeUpdate_AV1) {
    return kPostEncodeSymbLoadError;
  } 
  std::cout<<"DEBUG -6  Openinng sym  av1_ratecontrol_rtc_get_qp----"<<std::endl;
  ptrGetQP_AV1 = (GetQP_AV1_t)dlsym(handle, "av1_ratecontrol_rtc_get_qp");
  if (!ptrGetQP_AV1) {
    return kGetQpSymbLoadError;
  }
 std::cout<<"DEBUG -7  Openinng sym  av1_ratecontrol_rtc_get_loop_filter_level----"<<std::endl;
  ptrGetLoopfilterLevel_AV1 = (GetLoopfilterLevel_AV1_t)dlsym(
      handle, "av1_ratecontrol_rtc_get_loop_filter_level");
  if (!ptrGetLoopfilterLevel_AV1) {
    return kGetLoopFilterSymbError;
  }
std::cout<<"DEBUG -8  Openinng  all sym  Success...."<<std::endl;
  return kNoError;
}

LibMeboRateController *
Libmebo_brc_AV1::init(LibMeboRateController *libmebo_rc,
                      LibMeboRateControllerConfig *libmebo_rc_config) {

  int result = InitSymbolsFromLiray();
  if (result != kNoError) {
    return nullptr;
  }
  libmebo_rc = Libmebo_brc::init(libmebo_rc, libmebo_rc_config);

  memset (config, 0, sizeof(config));

  ptrInitConfigFunc(config);

 
  if (config == nullptr)
    return nullptr;

  constexpr int kMinQP = 10;
  constexpr int kMaxQP = 56;
  config->width = 640;
  config->height = 480;
  config->is_screen = false;
  // third_party/webrtc/modules/video_coding/codecs/av1/libaom_av1_encoder.cc
  config->max_quantizer = kMaxQP;
  config->min_quantizer = kMinQP;

  config->buf_initial_sz = 600;
  config->buf_optimal_sz = 500;
  config->target_bandwidth = 800000 / 1000;
  config->buf_sz = 1000;
  config->undershoot_pct = 25;
  config->overshoot_pct = 50;
  config->max_intra_bitrate_pct = 300;
  config->max_inter_bitrate_pct = 50;
  config->framerate = 60;
  av1aom_zero(config->layer_target_bitrate);
  config->layer_target_bitrate[0] = (config->target_bandwidth);
  av1aom_zero(config->ts_rate_decimator);
  config->ts_rate_decimator[0] = 1;
  config->aq_mode = 1;
  av1aom_zero(config->max_quantizers);
  av1aom_zero(config->min_quantizers);
  av1aom_zero(config->scaling_factor_num);
  av1aom_zero(config->scaling_factor_den);
  config->ss_number_layers = 1;
  config->ts_number_layers = 1;
  config->max_quantizers[0] = kMaxQP;
  config->min_quantizers[0] = kMinQP;
  config->scaling_factor_num[0] = 1;
  config->scaling_factor_den[0] = 1;
  config->frame_drop_thresh = 30;
  config->max_consec_drop_ms = 8;

  controller = ptrCreateAV1Controller(*config);
  if (controller == nullptr)
    return nullptr;
  return libmebo_rc;
}

LibMeboStatus
Libmebo_brc_AV1::update_config(LibMeboRateController *rc,
                               LibMeboRateControllerConfig *rc_config) {
  LibMeboStatus status = LIBMEBO_STATUS_SUCCESS;

  if (!rc || !rc_config)
    return LIBMEBO_STATUS_ERROR;

  ptrUpdateRateControl_AV1(controller, *config);

  return status;
}

LibMeboStatus Libmebo_brc_AV1::post_encode_update(LibMeboRateController *rc,
                                                  uint64_t encoded_frame_size) {
  LibMeboStatus status = LIBMEBO_STATUS_SUCCESS;

  if (!rc)
    return LIBMEBO_STATUS_ERROR;

  ptrPostEncodeUpdate_AV1(controller, encoded_frame_size);

  return status;
}

LibMeboStatus
Libmebo_brc_AV1::compute_qp(LibMeboRateController *rc,
                            LibMeboRCFrameParams *rc_frame_params) {

  LibMeboStatus status = LIBMEBO_STATUS_SUCCESS;

  if (!rc)
    return LIBMEBO_STATUS_ERROR;
  aom::AV1FrameParamsRTC frame_params_aom;

  if (rc_frame_params->frame_type == LIBMEBO_KEY_FRAME)
    frame_params_aom.frame_type = kKeyFrame;
  else
    frame_params_aom.frame_type = kInterFrame;

  ptrComputeQP_AV1(controller, frame_params_aom);
  return status;
}

LibMeboStatus Libmebo_brc_AV1::get_qp(LibMeboRateController *rc, int *qp) {
  LibMeboStatus status = LIBMEBO_STATUS_SUCCESS;

  if (!rc)
    return LIBMEBO_STATUS_ERROR;

  *qp = ptrGetQP_AV1(controller);
  return status;
}

LibMeboStatus Libmebo_brc_AV1::get_loop_filter_level(LibMeboRateController *rc,
                                                     int *filter_level) {
  LibMeboStatus status = LIBMEBO_STATUS_SUCCESS;
  if (!rc)
    return LIBMEBO_STATUS_ERROR;

  AV1LoopfilterLevel loop_filter_level;

  loop_filter_level = ptrGetLoopfilterLevel_AV1(controller);

  filter_level[0] = loop_filter_level.filter_level[0];
  filter_level[1] = loop_filter_level.filter_level[1];
  filter_level[2] = loop_filter_level.filter_level_u;
  filter_level[3] = loop_filter_level.filter_level_v;

  return status;
}
