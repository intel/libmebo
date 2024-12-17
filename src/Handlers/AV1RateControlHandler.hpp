#pragma once
#include <memory>

extern "C" {
#include "../lib/libmebo.hpp"
}
#include "../../aom/av1/ratectrl_rtc.h"
#include "LibMeboControlHandler.hpp"

typedef enum ErrorsLoadingSymbols {
  kNoError = 0,
  kMainHandleLibError = -1,
  kAv1CreateSymbLoadError = -2,
  kAV1RateCtrlInitConfigSymbLoadError = -3,
  kUpdateRateControlSymbLoadError = -4,
  kCompueQPSymbLoadError = -5,
  kPostEncodeSymbLoadError = -6,
  kGetQpSymbLoadError = -7,
  kGetLoopFilterSymbError = -8,
} ErrosLibmeboSymbols;

class Libmebo_brc_AV1 : public Libmebo_brc {
public:
  Libmebo_brc_AV1(LibMeboBrcAlgorithmID algo_id);
  virtual ~Libmebo_brc_AV1() override;
  LibMeboRateController *init(LibMeboRateController *rc,
                              LibMeboRateControllerConfig *rc_config) override;
  LibMeboStatus update_config(LibMeboRateController *rc,
                              LibMeboRateControllerConfig *rc_cfg) override;
  LibMeboStatus post_encode_update(LibMeboRateController *rc,
                                   uint64_t encoded_frame_size) override;
  LibMeboStatus compute_qp(LibMeboRateController *rc,
                           LibMeboRCFrameParams *rc_frame_params) override;
  LibMeboStatus get_qp(LibMeboRateController *rc, int *qp) override;
  LibMeboStatus get_loop_filter_level(LibMeboRateController *rc,
                                      int *filter_level) override;
  int InitSymbolsFromLiray();

private:
  typedef void *BrcCodecEnginePtr;
  BrcCodecEnginePtr brc_codec_handler;
  // std::unique_ptr <aom::AV1RateControlRTC> av1_rc_rtc_;
  void *handle;
  typedef AomAV1RateControlRtcConfig *(*create_av1_rate_control_config_t)();
  typedef void *(*createAV1Controller_t)(const AomAV1RateControlRtcConfig &rc_cfg);
  typedef bool (*UpdateRateControl_AV1_t)(void *controller, const AomAV1RateControlRtcConfig &rc_cfg);
  typedef int (*GetQP_AV1_t)(void *controller);
  typedef FrameDropDecision (*ComputeQP_AV1_t)(void *controller, const AomAV1FrameParamsRTC &frame_params);
  typedef AomAV1LoopfilterLevel (*GetLoopfilterLevel_AV1_t)(void *controller);
  typedef void (*PostEncodeUpdate_AV1_t)(void *controller, uint64_t encoded_frame_size);
  typedef bool (*GetSegmentationData_AV1_t)(void *controller, AomAV1SegmentationData *segmentation_data);
  typedef AomAV1CdefInfo (*GetCdefInfo_AV1_t)(void *controller);


  typedef void (*InitRateControlConfigFunc)(struct AomAV1RateControlRtcConfig *config);
  create_av1_rate_control_config_t create_av1_ratecontrol_config;
  createAV1Controller_t ptrCreateAV1Controller;
  UpdateRateControl_AV1_t ptrUpdateRateControl_AV1;
  GetQP_AV1_t ptrGetQP_AV1;
  ComputeQP_AV1_t ptrComputeQP_AV1;
  GetLoopfilterLevel_AV1_t ptrGetLoopfilterLevel_AV1;
  PostEncodeUpdate_AV1_t ptrPostEncodeUpdate_AV1;
  GetSegmentationData_AV1_t ptrGetSegmentationData_AV1;
  GetCdefInfo_AV1_t ptrGetCdefInfo_AV1;
  InitRateControlConfigFunc ptrInitConfigFunc;
};
