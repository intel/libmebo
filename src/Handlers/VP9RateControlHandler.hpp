#pragma once
extern "C" {
#include "../lib/libmebo.hpp"
}

#include "LibMeboControlHandler.hpp"

class LibmeboBrc_VP9 : public LibMeboBrc {
public:
  LibmeboBrc_VP9(LibMeboBrcAlgorithmID algo_id);
  virtual ~LibmeboBrc_VP9() override = default;
  [[nodiscard]] LibMeboRateController *init(LibMeboRateController *rc,
                              LibMeboRateControllerConfig *rc_config) override;
  [[nodiscard]]LibMeboStatus update_config(LibMeboRateController *rc,
                              LibMeboRateControllerConfig *rc_cfg) override;
  [[nodiscard]] LibMeboStatus post_encode_update(LibMeboRateController *rc,
                                   uint64_t encoded_frame_size) override;
  [[nodiscard]] LibMeboStatus compute_qp(LibMeboRateController *rc,
                           LibMeboRCFrameParams *rc_frame_params) override;
  [[nodiscard]] LibMeboStatus get_qp(LibMeboRateController *rc, int *qp) override;
  [[nodiscard]] LibMeboStatus get_loop_filter_level(LibMeboRateController *rc,
                                      int *filter_level) override;

private:
  typedef void *BrcCodecEnginePtr;
  BrcCodecEnginePtr brc_codec_handler;
};