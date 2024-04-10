#pragma once
#include <memory>


extern "C"
{
    #include "../lib/libmebo.h"
}
#include "LibMeboControlHandler.hpp"
#include "..//brc/av1/aom_derived/lib_header/ratectrl_rtc.h"

class Libmebo_brc_AV1 : public Libmebo_brc {
public:		
	Libmebo_brc_AV1(LibMeboBrcAlgorithmID algo_id);
	virtual ~Libmebo_brc_AV1() override = default;
	LibMeboRateController* init(LibMeboRateController* rc,
	LibMeboRateControllerConfig* rc_config) override ;
	LibMeboStatus update_config(LibMeboRateController* rc,
		LibMeboRateControllerConfig* rc_cfg) override;
	LibMeboStatus post_encode_update(LibMeboRateController* rc, uint64_t encoded_frame_size) override;
	LibMeboStatus compute_qp(LibMeboRateController *rc, LibMeboRCFrameParams* rc_frame_params) override;
	LibMeboStatus get_qp(LibMeboRateController* rc, int* qp) override;
	LibMeboStatus get_loop_filter_level(LibMeboRateController* rc, int* filter_level) override;
	
private:
     typedef void* BrcCodecEnginePtr;
	 BrcCodecEnginePtr brc_codec_handler;
	 std::unique_ptr <aom::AV1RateControlRTC> av1_rc_rtc_;
};

