#include "VP9RateControlHandler.hpp"
#include <iostream>
extern "C" {
#include "../brc/vp9/libvpx_derived/libvpx_vp9_rtc.h"

LibmeboBrc_VP9::LibmeboBrc_VP9(LibMeboBrcAlgorithmID algo_id)
    : LibMeboBrc(LIBMEBO_CODEC_VP9, algo_id) {
  encParamsLibMebo.num_sl = 1;
  encParamsLibMebo.num_tl = 1;
  encParamsLibMebo.bitrate = 288; // in kbps.
  encParamsLibMebo.dynamicRateChange = 0;
  encParamsLibMebo.framecount = 100;
  encParamsLibMebo.framerate = 60;
  encParamsLibMebo.width = 320;
  encParamsLibMebo.height = 160;
  encParamsLibMebo.id = static_cast<unsigned int>(LIBMEBO_CODEC_VP9);
  encParamsLibMebo.preset = 0;
  encParamsLibMebo.bufOptimalSz = 600;
}

LibMeboRateController *
LibmeboBrc_VP9::init(LibMeboRateController *libmebo_rc,
                      LibMeboRateControllerConfig *libmebo_rc_config) {
  LibMeboStatus status = LIBMEBO_STATUS_UNKNOWN;

  if (!libmebo_rc || !libmebo_rc_config)
    return nullptr;
  libmebo_rc = LibMeboBrc::init(libmebo_rc, libmebo_rc_config);
  status = brc_vp9_rate_control_init(libmebo_rc_config, &brc_codec_handler);
  if (status != LIBMEBO_STATUS_SUCCESS)
    fprintf(stderr, "Failed to Initialize the RateController\n");

  return libmebo_rc;
}

LibMeboStatus
LibmeboBrc_VP9::update_config(LibMeboRateController *rc,
                               LibMeboRateControllerConfig *rc_cfg) {
  LibMeboStatus status = LIBMEBO_STATUS_UNKNOWN;

  if (!rc || !rc_cfg)
    return status;

  status = brc_vp9_update_rate_control(brc_codec_handler, rc_cfg);

  if (status != LIBMEBO_STATUS_SUCCESS)
    fprintf(stderr, "Failed to Update the RateController\n");

  return status;
}

LibMeboStatus LibmeboBrc_VP9::post_encode_update(LibMeboRateController *rc,
                                                  uint64_t encoded_frame_size) {
  LibMeboStatus status = LIBMEBO_STATUS_UNKNOWN;

  if (!rc)
    return status;

  status = brc_vp9_post_encode_update(brc_codec_handler, encoded_frame_size);
  if (status != LIBMEBO_STATUS_SUCCESS)
    fprintf(stderr, "Failed to do the post encode update \n");

  return status;
}

LibMeboStatus
LibmeboBrc_VP9::compute_qp(LibMeboRateController *rc,
                            LibMeboRCFrameParams *rc_frame_params) {
  LibMeboStatus status = LIBMEBO_STATUS_UNKNOWN;

  if (!rc)
    return status;

  status = brc_vp9_compute_qp(brc_codec_handler, rc_frame_params);
  if (status != LIBMEBO_STATUS_SUCCESS)
    fprintf(stderr, "Failed to compute the QP\n");

  return status;
}

LibMeboStatus LibmeboBrc_VP9::get_qp(LibMeboRateController *rc, int *qp) {
  LibMeboStatus status = LIBMEBO_STATUS_UNKNOWN;

  if (!rc)
    return status;

  status = brc_vp9_get_qp(brc_codec_handler, qp);
  if (status != LIBMEBO_STATUS_SUCCESS)
    fprintf(stderr, "Failed to get the QP\n");

  return status;
}

LibMeboStatus LibmeboBrc_VP9::get_loop_filter_level(LibMeboRateController *rc,
                                                     int *filter_level) {
  LibMeboStatus status = LIBMEBO_STATUS_UNKNOWN;

  if (!rc)
    return status;

  status = brc_vp9_get_loop_filter_level(brc_codec_handler, filter_level);
  if (status != LIBMEBO_STATUS_SUCCESS)
    fprintf(stderr, "Failed to get the Loop filter level\n");

  return status;
}
} // extern "C"