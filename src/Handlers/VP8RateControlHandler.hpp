#pragma once
extern "C"
{
    #include "../lib/libmebo.h"
}
#include "LibMeboControlHandler.hpp"


class Libmebo_brc_VP8 : public Libmebo_brc {
public:
	Libmebo_brc_VP8(LibMeboBrcAlgorithmID algo_id);
	virtual ~Libmebo_brc_VP8() override = default;
	LibMeboRateController* init(LibMeboRateController* rc,
	LibMeboRateControllerConfig* rc_config) override;
	LibMeboStatus update_config(LibMeboRateController* rc,
		LibMeboRateControllerConfig* rc_cfg) override;
	LibMeboStatus post_encode_update(LibMeboRateController* rc, uint64_t encoded_frame_size) override;
	LibMeboStatus compute_qp(LibMeboRateController *rc, LibMeboRCFrameParams* rc_frame_params) override;
	LibMeboStatus get_qp(LibMeboRateController* rc, int* qp) override;
	LibMeboStatus get_loop_filter_level(LibMeboRateController* rc, int* filter_level) override;
		
private:
     typedef void* BrcCodecEnginePtr;
	 BrcCodecEnginePtr brc_codec_handler;
};