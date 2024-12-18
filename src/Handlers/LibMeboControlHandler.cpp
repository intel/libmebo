#include <assert.h>
#include <iostream>
extern "C" {
#include "../lib/libmebo.hpp"
}

#include "LibMeboControlHandler.hpp"

Libmebo_brc::Libmebo_brc(LibMeboCodecType codec_type_in,
                         LibMeboBrcAlgorithmID algo_id_in) {
  /* will init encparams based on algo id*/
  codec_type = codec_type_in;
  algo_id = algo_id_in;
}

uint32_t
Libmebo_brc::MaxSizeOfKeyframeAsPercentage(uint32_t optimal_buffer_size,
                                           uint32_t max_framerate) {
  // Set max to the optimal buffer level (normalized by target BR),
  // and scaled by a scale_par.
  // Max target size = scale_par * optimal_buffer_size * targetBR[Kbps].
  // This value is presented in percentage of perFrameBw:
  // perFrameBw = targetBR[Kbps] * 1000 / framerate.
  // The target in % is as follows:
  const double target_size_byte_per_frame = optimal_buffer_size * 0.5;
  const uint32_t target_size_kbyte =
      target_size_byte_per_frame * max_framerate / 1000;
  const uint32_t target_size_kbyte_as_percent = target_size_kbyte * 100;

  // Don't go below 3 times the per frame bandwidth.
  const uint32_t kMinIntraSizePercentage = 300u;
  if (kMinIntraSizePercentage > target_size_kbyte_as_percent)
    return kMinIntraSizePercentage;
  else
    return target_size_kbyte_as_percent;
}

int Libmebo_brc::GetBitratekBps_l(int sl_id, int tl_id) {
  return layered_bitrates[sl_id][tl_id];
}

void Libmebo_brc::InitLayeredFramerate(int num_tl, int framerate,
                                       int *ts_rate_decimator) {
  for (int tl = 0; tl < num_tl; tl++) {
    if (tl == 0)
      layered_frame_rate[tl] = framerate / ts_rate_decimator[tl];
    else
      layered_frame_rate[tl] = (framerate / ts_rate_decimator[tl]) -
                               (framerate / ts_rate_decimator[tl - 1]);
  }
}

#define LAYER_IDS_TO_IDX(sl, tl, num_tl) ((sl) * (num_tl) + (tl))

void Libmebo_brc::InitLayeredBitrateAlloc(int num_sl, int num_tl, int bitrate) {
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
Libmebo_brc::init(LibMeboRateController *rc,
                  LibMeboRateControllerConfig *rc_config) {
  InitLayeredBitrateAlloc(enc_params_libmebo.num_sl, enc_params_libmebo.num_tl,
                          enc_params_libmebo.bitrate);
  rc_config->width = enc_params_libmebo.width;
  rc_config->height = enc_params_libmebo.height;
  rc_config->max_quantizer = 63;
  rc_config->min_quantizer = 0;
  rc_config->target_bandwidth = enc_params_libmebo.bitrate;
  rc_config->buf_initial_sz = 500;
  rc_config->buf_optimal_sz = 600;
  ;
  rc_config->buf_sz = 1000;
  rc_config->undershoot_pct = 50;
  rc_config->overshoot_pct = 50;
  rc_config->buf_initial_sz = 500;
  rc_config->buf_optimal_sz = 600;
  ;
  rc_config->buf_sz = 1000;
  rc_config->undershoot_pct = 50;
  rc_config->overshoot_pct = 50;
  // Fixme
  rc_config->max_intra_bitrate_pct = MaxSizeOfKeyframeAsPercentage(
      rc_config->buf_optimal_sz, enc_params_libmebo.framerate);
  rc_config->max_intra_bitrate_pct = 0;
  rc_config->framerate = enc_params_libmebo.framerate;

  rc_config->max_quantizers[0] = rc_config->max_quantizer;
  rc_config->min_quantizers[0] = rc_config->min_quantizer;

  rc_config->ss_number_layers = enc_params_libmebo.num_sl;
  rc_config->ts_number_layers = enc_params_libmebo.num_tl;

  switch (enc_params_libmebo.num_sl) {
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
            << enc_params_libmebo.width << "," << enc_params_libmebo.height
            << "," << enc_params_libmebo.num_tl << ","
            << enc_params_libmebo.num_sl << "," << enc_params_libmebo.bitrate
            << "\n";
  for (unsigned int sl = 0; sl < enc_params_libmebo.num_sl; sl++) {
    int bitrate_sum = 0;
    for (unsigned int tl = 0; tl < enc_params_libmebo.num_tl; tl++) {
      const int layer_id = LAYER_IDS_TO_IDX(sl, tl, enc_params_libmebo.num_tl);
      rc_config->max_quantizers[layer_id] = rc_config->max_quantizer;
      rc_config->min_quantizers[layer_id] = rc_config->min_quantizer;
      bitrate_sum += GetBitratekBps_l(sl, tl);
      rc_config->layer_target_bitrate[layer_id] = bitrate_sum;
      std::cout << "bitrate_sum : " << bitrate_sum << std::endl;
      rc_config->ts_rate_decimator[tl] =
          1u << (enc_params_libmebo.num_tl - tl - 1);
    }
    InitLayeredFramerate(enc_params_libmebo.num_tl,
                         enc_params_libmebo.framerate,
                         rc_config->ts_rate_decimator);
  }
  return rc;
}