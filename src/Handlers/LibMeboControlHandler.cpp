#include <assert.h>
#include <iostream>
extern "C" {
#include "../lib/libmebo.hpp"
}

#include "LibMeboControlHandler.hpp"

LibMeboBrc::LibMeboBrc(LibMeboCodecType codecTypeIn,
                         LibMeboBrcAlgorithmID algoIdIn) {
  codec_type = codecTypeIn;
  algo_id = algoIdIn; // algo id is not used now after going for direct repos
  // other members will init later
}

uint32_t
LibMeboBrc::MaxSizeOfKeyframeAsPercentage(uint32_t optimalBufferSize,
                                           uint32_t maxFrameRate) {
  // Set max to the optimal buffer level (normalized by target BR),
  // and scaled by a scale_par.
  // Max target size = scale_par * optimalBufferSize * targetBR[Kbps].
  // This value is presented in percentage of perFrameBw:
  // perFrameBw = targetBR[Kbps] * 1000 / framerate.
  // The target in % is as follows:
  const double targetSizeBytePerFrame = optimalBufferSize * 0.5;
  const uint32_t targetSizeKbyte =
      targetSizeBytePerFrame * maxFrameRate / 1000;
  const uint32_t targetSizeKbytePercent = targetSizeKbyte * 100;

  // Don't go below 3 times the per frame bandwidth.
  const uint32_t kMinIntraSizePercentage = 300u;
  if (kMinIntraSizePercentage > targetSizeKbytePercent)
    return kMinIntraSizePercentage;
  else
    return targetSizeKbytePercent;
}

int LibMeboBrc::GetBitratekBps_l(int sl_id, int tl_id) {
  return layered_bitrates[sl_id][tl_id];
}

void LibMeboBrc::InitLayeredFramerate(int num_tl, int framerate,
                                       int *ts_rate_decimator) {
  for (int tl = 0; tl < num_tl; tl++) {
    if (tl == 0)
      layeredFrameRate[tl] = framerate / ts_rate_decimator[tl];
    else
      layeredFrameRate[tl] = (framerate / ts_rate_decimator[tl]) -
                               (framerate / ts_rate_decimator[tl - 1]);
  }
}

#define LAYER_IDS_TO_IDX(sl, tl, num_tl) ((sl) * (num_tl) + (tl))

void LibMeboBrc::InitLayeredBitrateAlloc(int num_sl, int num_tl, int bitrate) {
  int sl, tl;

  assert(num_sl && num_sl <= MaxSpatialLayers);
  assert(num_tl && num_tl <= MaxTemporalLayers);

  for (sl = 0; sl < num_sl; sl++) {
    int sl_rate = bitrate / num_sl;
    for (tl = 0; tl < num_tl; tl++) {
      layered_bitrates[sl][tl] = sl_rate / num_tl;
    }
  }
}

LibMeboRateController *
LibMeboBrc::init(LibMeboRateController *rc,
                  LibMeboRateControllerConfig *rc_config) {
  InitLayeredBitrateAlloc(encParamsLibMebo.num_sl, encParamsLibMebo.num_tl,
                          encParamsLibMebo.bitrate);
  rc_config->width = encParamsLibMebo.width;
  rc_config->height = encParamsLibMebo.height;
  rc_config->max_quantizer = 63;
  rc_config->min_quantizer = 0;
  rc_config->target_bandwidth = encParamsLibMebo.bitrate;
  rc_config->buf_initial_sz = 500;
  rc_config->buf_optimal_sz = 600;

  rc_config->buf_sz = 1000;
  rc_config->undershoot_pct = 50;
  rc_config->overshoot_pct = 50;
  rc_config->buf_initial_sz = 500;
  rc_config->buf_optimal_sz = 600;

  rc_config->buf_sz = 1000;
  rc_config->undershoot_pct = 50;
  rc_config->overshoot_pct = 50;
  // Fixme
  rc_config->max_intra_bitrate_pct = MaxSizeOfKeyframeAsPercentage(
      rc_config->buf_optimal_sz, encParamsLibMebo.framerate);
  rc_config->max_intra_bitrate_pct = 0;
  rc_config->framerate = encParamsLibMebo.framerate;

  rc_config->max_quantizers[0] = rc_config->max_quantizer;
  rc_config->min_quantizers[0] = rc_config->min_quantizer;

  rc_config->ss_number_layers = encParamsLibMebo.num_sl;
  rc_config->ts_number_layers = encParamsLibMebo.num_tl;

  switch (encParamsLibMebo.num_sl) {
  case 1:
    rc_config->scaling_factor_num[0] = 1;
    rc_config->scaling_factor_den[0] = 1;
    break;
  case 2:
    rc_config->scaling_factor_num[0] = 1;
    rc_config->scaling_factor_den[0] = 2;
    rc_config->scaling_factor_num[1] = 1;
    rc_config->scaling_factor_den[1] = 1;
    break;
  case 3:
    rc_config->scaling_factor_num[0] = 1;
    rc_config->scaling_factor_den[0] = 4;
    rc_config->scaling_factor_num[1] = 1;
    rc_config->scaling_factor_den[1] = 2;
    rc_config->scaling_factor_num[2] = 1;
    rc_config->scaling_factor_den[2] = 1;
    break;
  default:
    break;
  }

  std::cout << "enc params - width, height, bitrate, sl, tl :"
            << encParamsLibMebo.width << "," << encParamsLibMebo.height
            << "," << encParamsLibMebo.num_tl << ","
            << encParamsLibMebo.num_sl << "," << encParamsLibMebo.bitrate
            << "\n";
  for (unsigned int sl = 0; sl < encParamsLibMebo.num_sl; sl++) {
    int bitrate_sum = 0;
    for (unsigned int tl = 0; tl < encParamsLibMebo.num_tl; tl++) {
      const int layer_id = LAYER_IDS_TO_IDX(sl, tl, encParamsLibMebo.num_tl);
      rc_config->max_quantizers[layer_id] = rc_config->max_quantizer;
      rc_config->min_quantizers[layer_id] = rc_config->min_quantizer;
      bitrate_sum += GetBitratekBps_l(sl, tl);
      rc_config->layer_target_bitrate[layer_id] = bitrate_sum;
      std::cout << "bitrate_sum : " << bitrate_sum << std::endl;
      rc_config->ts_rate_decimator[tl] =
          1u << (encParamsLibMebo.num_tl - tl - 1);
    }
    InitLayeredFramerate(encParamsLibMebo.num_tl,
                         encParamsLibMebo.framerate,
                         rc_config->ts_rate_decimator);
  }
  return rc;
}