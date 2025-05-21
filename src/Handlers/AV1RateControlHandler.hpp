#pragma once
#include <memory>

extern "C" {
#include "../lib/libmebo.hpp"
}
#include "../../git_submodules/aom/av1/ratectrl_rtc.h"
#include "LibMeboControlHandler.hpp"

typedef enum ErrorsLoadingSymbols_AV1 {
  kNoError_AV1 = 0,
  kMainHandleLibError_AV1 = -1,
  kAv1CreateSymbLoadError_AV1 = -2,
  kAV1RateCtrlInitConfigSymbLoadError_AV1 = -3,
  kUpdateRateControlSymbLoadError_AV1 = -4,
  kCompueQPSymbLoadError_AV1 = -5,
  kPostEncodeSymbLoadError_AV1 = -6,
  kGetQpSymbLoadError_AV1 = -7,
  kGetLoopFilterSymbError_AV1 = -8,
} ErrosLibmeboSymbols_AV1;

class LibmeboBrc_AV1 : public LibMeboBrc {
public:
  LibmeboBrc_AV1(LibMeboBrcAlgorithmID algo_id);
  virtual ~LibmeboBrc_AV1() override;
  LibMeboRateController *init(LibMeboRateController *rc,
                              LibMeboRateControllerConfig *rcConfig) override;
  LibMeboStatus update_config(LibMeboRateController *rc,
                              LibMeboRateControllerConfig *rcConfig) override;
  LibMeboStatus post_encode_update(LibMeboRateController *rc,
                                   uint64_t encodedFrameSize) override;
  LibMeboStatus compute_qp(LibMeboRateController *rc,
                           LibMeboRCFrameParams *rcFrameParams) override;
  LibMeboStatus get_qp(LibMeboRateController *rc, int *qp) override;
  LibMeboStatus get_loop_filter_level(LibMeboRateController *rc,
                                      int *filterLevel) override;
  int InitSymbolsFromLibrary();

private:
  void *handle;
  typedef void (*InitRateControlConfigFunc_AV1_t)(
      struct AomAV1RateControlRtcConfig *rcConfig);

  typedef AomAV1RateControlRTC *(*CreateRateControl_AV1_t)(
      const struct AomAV1RateControlRtcConfig *rcConfig);

  typedef bool (*UpdateRateControl_AV1_t)(
      void *controller, struct AomAV1RateControlRtcConfig *rcConfig);

  typedef AomFrameDropDecision (*ComputeQP_AV1_t)(
      void *controller, const AomAV1FrameParamsRTC &frameParams);

  typedef int (*GetQP_AV1_t)(void *controller);

  typedef AomAV1LoopfilterLevel (*GetLoopfilterLevel_AV1_t)(void *controller);

  typedef void (*PostEncodeUpdate_AV1_t)(void *controller,
                                         uint64_t encodedFrameSize);

  typedef bool (*GetSegmentationData_AV1_t)(
      void *controller, AomAV1SegmentationData *segmentation_data);

  typedef AomAV1CdefInfo (*GetCdefInfo_AV1_t)(void *controller);

  InitRateControlConfigFunc_AV1_t ptrInitConfigFunc;
  CreateRateControl_AV1_t ptrCreateAV1Controller;
  UpdateRateControl_AV1_t ptrUpdateRateControl_AV1;
  ComputeQP_AV1_t ptrComputeQP_AV1;
  GetQP_AV1_t ptrGetQP_AV1;
  GetLoopfilterLevel_AV1_t ptrGetLoopfilterLevel_AV1;
  PostEncodeUpdate_AV1_t ptrPostEncodeUpdate_AV1;
  GetSegmentationData_AV1_t ptrGetSegmentationData_AV1;
  GetCdefInfo_AV1_t ptrGetCdefInfo_AV1;
  AomAV1RateControlRtcConfig *rcConfig;
};
