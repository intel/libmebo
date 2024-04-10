#include <iostream>
#include "AV1RateControlHandler.hpp"

#include "../brc/av1/aom_derived/lib_header/ratectrl_rtc.h"



Libmebo_brc_AV1::Libmebo_brc_AV1(LibMeboBrcAlgorithmID algo_id) : Libmebo_brc( LIBMEBO_CODEC_AV1, algo_id)
{
  enc_params_libmebo.num_sl = 1;
	enc_params_libmebo.num_tl = 1;
	enc_params_libmebo.bitrate = 288; //in kbps.
	enc_params_libmebo.dynamic_rate_change =0;
	enc_params_libmebo.framecount = 100;
	enc_params_libmebo.framerate = 60;
	enc_params_libmebo.width = 320;
	enc_params_libmebo.height = 160;
    enc_params_libmebo.id = static_cast<unsigned int>( LIBMEBO_CODEC_AV1);
	enc_params_libmebo.preset =0;
	enc_params_libmebo.buf_optimal_sz = 600;

}

aom::AV1RateControlRtcConfig rc_config_aom;
LibMeboRateController* Libmebo_brc_AV1::init(LibMeboRateController* libmebo_rc,
	LibMeboRateControllerConfig* libmebo_rc_config)
{
  LibMeboStatus status = LIBMEBO_STATUS_SUCCESS;
  

	libmebo_rc = Libmebo_brc::init (libmebo_rc, libmebo_rc_config);

    
    constexpr int kMinQP = 10;
    constexpr int kMaxQP = 56;
    rc_config_aom.width = 640;
    rc_config_aom.height = 480;
    // third_party/webrtc/modules/video_coding/codecs/av1/libaom_av1_encoder.cc
    rc_config_aom.max_quantizer = kMaxQP;
    rc_config_aom.min_quantizer = kMinQP;

    rc_config_aom.buf_initial_sz = 600;
    rc_config_aom.buf_optimal_sz = 500;
    rc_config_aom.target_bandwidth =800000 /1000;
    rc_config_aom.buf_sz = 1000;
    rc_config_aom.undershoot_pct = 25;
    rc_config_aom.overshoot_pct = 50;
    rc_config_aom.max_intra_bitrate_pct = 300;
    rc_config_aom.max_inter_bitrate_pct = 50;
    rc_config_aom.framerate = 60;
    rc_config_aom.layer_target_bitrate[0] = 800000/1000;
    
    rc_config_aom.ts_rate_decimator[0] = 1;
    rc_config_aom.aq_mode = 0;
    rc_config_aom.ss_number_layers = 1;
    rc_config_aom.ts_number_layers = 1;
    rc_config_aom.max_quantizers[0] = kMaxQP;
    rc_config_aom.min_quantizers[0] = kMinQP;
    rc_config_aom.scaling_factor_num[0] = 1;
    rc_config_aom.scaling_factor_den[0] = 1;

    av1_rc_rtc_ = aom::AV1RateControlRTC::Create(rc_config_aom);
    if(!av1_rc_rtc_)
      std::cout<<"FAiled to create AV1RateControlRTC \n";

	return libmebo_rc;
} 
LibMeboStatus Libmebo_brc_AV1::update_config(LibMeboRateController* rc,
	LibMeboRateControllerConfig* rc_config)
{
  LibMeboStatus status = LIBMEBO_STATUS_SUCCESS;

  if (!rc || !rc_config)
    return status;
  av1_rc_rtc_->UpdateRateControl(rc_config_aom);
  return status;
}



LibMeboStatus Libmebo_brc_AV1::post_encode_update(LibMeboRateController* rc, uint64_t encoded_frame_size)
{
  LibMeboStatus status = LIBMEBO_STATUS_SUCCESS;

  if (!rc)
    return status;
  av1_rc_rtc_->PostEncodeUpdate(encoded_frame_size);

  return status;
}

LibMeboStatus Libmebo_brc_AV1::compute_qp(LibMeboRateController *rc, LibMeboRCFrameParams* rc_frame_params)
{
  LibMeboStatus status = LIBMEBO_STATUS_SUCCESS;

  if (!rc)
    return status;
  
  aom::AV1FrameParamsRTC frame_params_aom;
  
  if(rc_frame_params->frame_type == LIBMEBO_KEY_FRAME)
     frame_params_aom.frame_type = aom::kKeyFrame;
  else
    frame_params_aom.frame_type = aom::kInterFrame;
   av1_rc_rtc_->ComputeQP(frame_params_aom);
  return status;
}

LibMeboStatus Libmebo_brc_AV1::get_qp(LibMeboRateController* rc, int* qp)
{
  LibMeboStatus status = LIBMEBO_STATUS_SUCCESS;
  
  if (!rc)
    return status;
   *qp =   (av1_rc_rtc_->GetQP());

  return status;
}

LibMeboStatus Libmebo_brc_AV1::get_loop_filter_level(LibMeboRateController* rc, int* filter_level)
{
	LibMeboStatus status = LIBMEBO_STATUS_SUCCESS;
	if (!rc)
		return status;

    aom::AV1LoopfilterLevel loop_filter_level = av1_rc_rtc_->GetLoopfilterLevel();
   filter_level[0] = loop_filter_level.filter_level[0];
   filter_level[1] = loop_filter_level.filter_level[1];
   filter_level[2] = loop_filter_level.filter_level_u;
   filter_level[3] = loop_filter_level.filter_level_v;

	return status;
}
