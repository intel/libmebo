#include "AV1RateControlHandler.hpp"
#include <cstring>
#include <dlfcn.h>
#include <stdlib.h>
#include <iostream>
#include <memory>
#include "../../git_submodules/aom/av1/ratectrl_rtc.h"

LibmeboBrc_AV1::LibmeboBrc_AV1(LibMeboBrcAlgorithmID algoId)
    : LibMeboBrc(LIBMEBO_CODEC_AV1, algoId), 
      handle(nullptr),
      ptrInitConfigFunc(nullptr),
      ptrCreateAV1Controller(nullptr),
      ptrUpdateRateControl_AV1(nullptr),
      ptrComputeQP_AV1(nullptr),
      ptrGetQP_AV1(nullptr),
      ptrGetLoopfilterLevel_AV1(nullptr),
      ptrPostEncodeUpdate_AV1(nullptr),
      ptrGetSegmentationData_AV1(nullptr),
      ptrGetCdefInfo_AV1(nullptr),
      rcConfig(nullptr),
      controller(nullptr) {
    
    encParamsLibMebo.num_sl = 1;
    encParamsLibMebo.num_tl = 1;
    encParamsLibMebo.bitrate = 288;
    encParamsLibMebo.dynamicRateChange = 0;
    encParamsLibMebo.framecount = 100;
    encParamsLibMebo.framerate = 60;
    encParamsLibMebo.width = 320;
    encParamsLibMebo.height = 160;
    encParamsLibMebo.id = static_cast<unsigned int>(LIBMEBO_CODEC_AV1);
    encParamsLibMebo.preset = 0;
    encParamsLibMebo.bufOptimalSz = 600;
}

LibmeboBrc_AV1::~LibmeboBrc_AV1() {
  if (handle) {
    dlclose(handle);
    handle = nullptr;
  }
}

#define av1aom_zero(dest) memset(&(dest), 0, sizeof(dest))

[[nodiscard]]  int LibmeboBrc_AV1::InitSymbolsFromLibrary() {

  char path[] = "/usr/local/lib/libaom_av1_rc.so";

  handle = dlopen(path, RTLD_LAZY);

  if (!handle) {
    return kMainHandleLibError_AV1;
  }

  ptrCreateAV1Controller =
      (CreateRateControl_AV1_t)dlsym(handle, "av1_ratecontrol_rtc_create");
  if (!ptrCreateAV1Controller) {
    return kAv1CreateSymbLoadError_AV1;
  }

  ptrInitConfigFunc = (InitRateControlConfigFunc_AV1_t)dlsym(
      handle, "av1_ratecontrol_rtc_init_ratecontrol_config");
  if (!ptrInitConfigFunc) {
    return kAV1RateCtrlInitConfigSymbLoadError_AV1;
  }

  ptrUpdateRateControl_AV1 =
      (UpdateRateControl_AV1_t)dlsym(handle, "av1_ratecontrol_rtc_update");
  if (!ptrUpdateRateControl_AV1) {
    return kUpdateRateControlSymbLoadError_AV1;
  }

  ptrComputeQP_AV1 =
      (ComputeQP_AV1_t)dlsym(handle, "av1_ratecontrol_rtc_compute_qp");
  if (!ptrComputeQP_AV1) {
    return kCompueQPSymbLoadError_AV1;
  }

  ptrPostEncodeUpdate_AV1 = (PostEncodeUpdate_AV1_t)dlsym(
      handle, "av1_ratecontrol_rtc_post_encode_update");
  if (!ptrPostEncodeUpdate_AV1) {
    return kPostEncodeSymbLoadError_AV1;
  }

  ptrGetQP_AV1 = (GetQP_AV1_t)dlsym(handle, "av1_ratecontrol_rtc_get_qp");
  if (!ptrGetQP_AV1) {
    return kGetQpSymbLoadError_AV1;
  }

  ptrGetLoopfilterLevel_AV1 = (GetLoopfilterLevel_AV1_t)dlsym(
      handle, "av1_ratecontrol_rtc_get_loop_filter_level");
  if (!ptrGetLoopfilterLevel_AV1) {
    return kGetLoopFilterSymbError_AV1;
  }

  return kNoError_AV1;
}

[[nodiscard]] LibMeboRateController *
LibmeboBrc_AV1::init(LibMeboRateController *libMeboRC,
                     LibMeboRateControllerConfig *libMeboRCConfig) {
  std::cout<<"Inside init of AV1 "<<std::endl;
  int result = InitSymbolsFromLibrary();
  if (result != kNoError_AV1) {
    return nullptr;
  }
  std::cout<<"Inside init of AV1 -2 "<<std::endl;
  libMeboRC = LibMeboBrc::init(libMeboRC, libMeboRCConfig);
  std::cout<<"Inside init of AV1 -3"<<std::endl;
  rcConfig = (AomAV1RateControlRtcConfig *)aligned_alloc(
      alignof(AomAV1RateControlRtcConfig), sizeof(AomAV1RateControlRtcConfig));

  if (rcConfig == nullptr) {
    return nullptr;
  }
  
    rcConfig = {0};
  //memset(rcConfig, 0, sizeof(AomAV1RateControlRtcConfig));

  ptrInitConfigFunc(rcConfig);

  controller = ptrCreateAV1Controller(rcConfig);
  if (controller == nullptr)
    return nullptr;
  return libMeboRC;
}

[[nodiscard]] LibMeboStatus
LibmeboBrc_AV1::update_config(LibMeboRateController *rc,
                              LibMeboRateControllerConfig *LibmeboRcConfig) {
  LibMeboStatus status = LIBMEBO_STATUS_SUCCESS;

  if (!rc || !LibmeboRcConfig)
    return LIBMEBO_STATUS_ERROR;

  ptrUpdateRateControl_AV1(controller, rcConfig);

  return status;
}

[[nodiscard]] LibMeboStatus LibmeboBrc_AV1::post_encode_update(
                                             LibMeboRateController *rc,
                                                 uint64_t encodedFrameSize) {
  LibMeboStatus status = LIBMEBO_STATUS_SUCCESS;

  if (!rc)
    return LIBMEBO_STATUS_ERROR;

  ptrPostEncodeUpdate_AV1(controller, encodedFrameSize);

  return status;
}

[[nodiscard]] LibMeboStatus LibmeboBrc_AV1::compute_qp(LibMeboRateController *rc,
                                         LibMeboRCFrameParams *rcFrameParams) {

  LibMeboStatus status = LIBMEBO_STATUS_SUCCESS;

  if (!rc)
    return LIBMEBO_STATUS_ERROR;
  AomAV1FrameParamsRTC frameParamsAom;

  if (rcFrameParams->frame_type == LIBMEBO_KEY_FRAME)
    frameParamsAom.frame_type = kAomKeyFrame;
  else
    frameParamsAom.frame_type = kAomInterFrame;

  frameParamsAom.spatial_layer_id = 0;
  frameParamsAom.temporal_layer_id = 0;

  ptrComputeQP_AV1(controller, *(&frameParamsAom));
  return status;
}

[[nodiscard]] LibMeboStatus LibmeboBrc_AV1::get_qp(LibMeboRateController *rc, int *qp) {
  LibMeboStatus status = LIBMEBO_STATUS_SUCCESS;

  if (!rc)
    return LIBMEBO_STATUS_ERROR;

  *qp = ptrGetQP_AV1(controller);
  return status;
}

[[nodiscard]] LibMeboStatus LibmeboBrc_AV1::get_loop_filter_level(LibMeboRateController *rc,
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
