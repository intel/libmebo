#include "AV1RateControlHandler.hpp"
#include <cstring>
#include <dlfcn.h>

#include "../../../aom/av1/ratectrl_rtc.h"

Libmebo_brc_AV1::Libmebo_brc_AV1(LibMeboBrcAlgorithmID algo_id)
    : Libmebo_brc(LIBMEBO_CODEC_AV1, algo_id), handle(nullptr) {
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

int Libmebo_brc_AV1::InitSymbolsFromLibrary() {

  char path[] = "/usr/local/lib/libaom_av1_rc.so"; // this needs to be at config time.

  handle = dlopen(path, RTLD_LAZY);
  if (!handle) {
    return kMainHandleLibError;
  }

  ptrCreateAV1Controller =
      (createAV1Controller_t)dlsym(handle, "av1_ratecontrol_rtc_create");
  if (!ptrCreateAV1Controller) {
    return kAv1CreateSymbLoadError;
  }

  ptrInitConfigFunc = (InitRateControlConfigFunc)dlsym(
      handle, "av1_ratecontrol_rtc_init_ratecontrol_config");
  if (!ptrInitConfigFunc) {
    return kAV1RateCtrlInitConfigSymbLoadError;
  }

  ptrUpdateRateControl_AV1 =
      (UpdateRateControl_AV1_t)dlsym(handle, "av1_ratecontrol_rtc_update");
  if (!ptrUpdateRateControl_AV1) {
    return kUpdateRateControlSymbLoadError;
  }

  ptrComputeQP_AV1 =
      (ComputeQP_AV1_t)dlsym(handle, "av1_ratecontrol_rtc_compute_qp");
  if (!ptrComputeQP_AV1) {
    return kCompueQPSymbLoadError;
  }

  ptrPostEncodeUpdate_AV1 = (PostEncodeUpdate_AV1_t)dlsym(
      handle, "av1_ratecontrol_rtc_post_encode_update");
  if (!ptrPostEncodeUpdate_AV1) {
    return kPostEncodeSymbLoadError;
  }

  ptrGetQP_AV1 = (GetQP_AV1_t)dlsym(handle, "av1_ratecontrol_rtc_get_qp");
  if (!ptrGetQP_AV1) {
    return kGetQpSymbLoadError;
  }

  ptrGetLoopfilterLevel_AV1 = (GetLoopfilterLevel_AV1_t)dlsym(
      handle, "av1_ratecontrol_rtc_get_loop_filter_level");
  if (!ptrGetLoopfilterLevel_AV1) {
    return kGetLoopFilterSymbError;
  }

  return kNoError;
}

LibMeboRateController *
Libmebo_brc_AV1::init(LibMeboRateController *libmebo_rc,
                      LibMeboRateControllerConfig *libmebo_rc_config) {
  int result = InitSymbolsFromLibrary();
  if (result != kNoError) {
    return nullptr;
  }

  libmebo_rc = Libmebo_brc::init(libmebo_rc, libmebo_rc_config);

  config = (AomAV1RateControlRtcConfig *) aligned_alloc(alignof(AomAV1RateControlRtcConfig), sizeof(AomAV1RateControlRtcConfig));
  memset(config, 0, sizeof(AomAV1RateControlRtcConfig));
  ptrInitConfigFunc(config);

  if (config == nullptr)
    return nullptr;

  controller = ptrCreateAV1Controller(config);
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

  ptrUpdateRateControl_AV1(controller, config);

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
  AomAV1FrameParamsRTC frame_params_aom;

  if (rc_frame_params->frame_type == LIBMEBO_KEY_FRAME)
    frame_params_aom.frame_type = kAomKeyFrame;
  else
    frame_params_aom.frame_type = kAomInterFrame;

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

  AomAV1LoopfilterLevel loop_filter_level;

  loop_filter_level = ptrGetLoopfilterLevel_AV1(controller);

  filter_level[0] = loop_filter_level.filter_level[0];
  filter_level[1] = loop_filter_level.filter_level[1];
  filter_level[2] = loop_filter_level.filter_level_u;
  filter_level[3] = loop_filter_level.filter_level_v;

  return status;
}
