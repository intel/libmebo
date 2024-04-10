#include <iostream>
#include "VP8RateControlHandler.hpp"
#include "../brc/vp8/libvpx_derived/libvpx_vp8_rtc.h"


Libmebo_brc_VP8::Libmebo_brc_VP8(LibMeboBrcAlgorithmID algo_id) : Libmebo_brc(LIBMEBO_CODEC_VP8,  algo_id)
{
	std::cout<<"try-vp8 constructor\n\n";
	enc_params_libmebo.num_sl = 1;
	enc_params_libmebo.num_tl = 1;
	enc_params_libmebo.bitrate = 288; //in kbps.
	enc_params_libmebo.dynamic_rate_change =0;
	enc_params_libmebo.framecount = 100;
	enc_params_libmebo.framerate = 60;
	enc_params_libmebo.width = 320;
	enc_params_libmebo.height = 160;
    enc_params_libmebo.id = static_cast<unsigned int>( LIBMEBO_CODEC_VP8);
	enc_params_libmebo.preset =0;
	enc_params_libmebo.buf_optimal_sz = 600;
}

LibMeboRateController* Libmebo_brc_VP8::init(LibMeboRateController* libmebo_rc,
	LibMeboRateControllerConfig* libmebo_rc_config)
{
  LibMeboStatus status = LIBMEBO_STATUS_UNKNOWN;
  if (!libmebo_rc || !libmebo_rc_config)
    return nullptr;

  status = brc_vp8_rate_control_init (libmebo_rc_config, &brc_codec_handler);
  if (status != LIBMEBO_STATUS_SUCCESS)
    fprintf(stderr, "Failed to Initialize the RateController\n");

  //ToDo: Make it explicit to enforce the algorithm implementor to validate
  //the input params 
  //brc_status = priv->brc_interface.validate (rc_config, &priv->brc_codec_handler);
 
  return libmebo_rc;
}

LibMeboStatus Libmebo_brc_VP8::update_config(LibMeboRateController* rc,
	LibMeboRateControllerConfig* rc_cfg)
{
  LibMeboStatus status = LIBMEBO_STATUS_UNKNOWN;

  if (!rc || !rc_cfg)
    return status;
  status = brc_vp8_update_rate_control(brc_codec_handler, rc_cfg) ;
  if (status != LIBMEBO_STATUS_SUCCESS)
    fprintf(stderr, "Failed to Update the RateController\n");

  return status;
}


LibMeboStatus Libmebo_brc_VP8::post_encode_update(LibMeboRateController* rc, uint64_t encoded_frame_size)
{
  LibMeboStatus status = LIBMEBO_STATUS_UNKNOWN;
  if (!rc)
    return status;

  status = 	brc_vp8_post_encode_update(brc_codec_handler, encoded_frame_size);
  if (status != LIBMEBO_STATUS_SUCCESS)
    fprintf(stderr, "Failed to do the post encode update \n");

  return status;
}

LibMeboStatus Libmebo_brc_VP8::compute_qp(LibMeboRateController *rc, LibMeboRCFrameParams* rc_frame_params)
{
  LibMeboStatus status = LIBMEBO_STATUS_UNKNOWN;

  if (!rc)
    return status;

  status = brc_vp8_compute_qp (brc_codec_handler, rc_frame_params);
  if (status != LIBMEBO_STATUS_SUCCESS)
    fprintf(stderr, "Failed to compute the QP\n");

  return status;
}

LibMeboStatus Libmebo_brc_VP8::get_qp(LibMeboRateController* rc, int* qp)
{
  LibMeboStatus status = LIBMEBO_STATUS_UNKNOWN;
  if (!rc)
    return status;
  status = brc_vp8_get_qp(brc_codec_handler, qp) ;
  if (status != LIBMEBO_STATUS_SUCCESS)
    fprintf(stderr, "Failed to get the QP\n");

  return status;
}

LibMeboStatus Libmebo_brc_VP8::get_loop_filter_level(LibMeboRateController* rc, int* filter_level)
{
  LibMeboStatus status = LIBMEBO_STATUS_UNKNOWN;

  if (!rc)
    return status;

  status = brc_vp8_get_loop_filter_level(brc_codec_handler,filter_level );
  if (status != LIBMEBO_STATUS_SUCCESS)
    fprintf(stderr, "Failed to get the Loop filter level\n");

  return status;
}
