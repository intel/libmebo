/*
 *  Copyright (c) 2020 Intel Corporation. All Rights Reserved.
 *  Copyright (c) 2020 Sreerenj Balachandran <sreerenj.balachandran@intel.com>
 *  Author: Sreerenj Balachandran <sreerenj.balachandran@intel.com>
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "libmebo.hpp"
#include "RateControlFactory.hpp"
#include <cstdlib>
#include <memory>
#include "../src/Handlers/AV1RateControlHandler.hpp"
#include "../src/Handlers/LibMeboControlHandler.hpp"
#include "../src/Handlers/VP8RateControlHandler.hpp"
#include "../src/Handlers/VP9RateControlHandler.hpp"
typedef enum CodecID_ {
  VP8_ID = 0,
  VP9_ID = 1,
  AV1_ID = 2,
} CodecID;

LibMeboRateController *
libmebo_create_rate_controller(LibMeboCodecType CodecType,
                               LibMeboBrcAlgorithmID algo_id) {
  std::unique_ptr<Libmebo_brc> brc =
      Libmebo_brc_factory::create(static_cast<LibMeboBrcAlgorithmID>(algo_id));
  if (!brc)
    return nullptr;
  LibMeboRateController *libmebo_rc = static_cast<LibMeboRateController *>(
      malloc(sizeof(LibMeboRateController)));
  if (!libmebo_rc) {
      return NULL;
  }

  libmebo_rc->priv = brc.release();
  libmebo_rc->codec_type = static_cast<LibMeboCodecType>(CodecType); // later change
  return libmebo_rc;
}
void libmebo_release_rate_controller(LibMeboRateController *rc) {
  if (rc) {
    Libmebo_brc *brc = static_cast<Libmebo_brc *>(rc->priv);
    delete brc;
    free(rc);
  }
}

LibMeboRateController *
libmebo_init_rate_controller(LibMeboRateController *rc,
                             LibMeboRateControllerConfig *rc_config) {
  Libmebo_brc *brc = reinterpret_cast<Libmebo_brc *>(rc->priv);
  return reinterpret_cast<LibMeboRateController *>(brc->init(rc, rc_config));
}

LibMeboStatus
libmebo_update_rate_controller_config(LibMeboRateController *rc,
                                      LibMeboRateControllerConfig *rc_cfg) {
  Libmebo_brc *brc = reinterpret_cast<Libmebo_brc *>(rc->priv);
  return static_cast<LibMeboStatus>(brc->update_config(rc, rc_cfg));
}

LibMeboStatus libmebo_post_encode_update(LibMeboRateController *rc,
                                         uint64_t encoded_frame_size) {
  Libmebo_brc *brc = reinterpret_cast<Libmebo_brc *>(rc->priv);
  return static_cast<LibMeboStatus>(
      brc->post_encode_update(rc, encoded_frame_size));
}

LibMeboStatus libmebo_compute_qp(LibMeboRateController *rc,
                                 LibMeboRCFrameParams *rc_frame_params) {
  Libmebo_brc *brc = reinterpret_cast<Libmebo_brc *>(rc->priv);
  return static_cast<LibMeboStatus>(brc->compute_qp(rc, rc_frame_params));
}

LibMeboStatus libmebo_get_qp(LibMeboRateController *rc, int *qp) {
  Libmebo_brc *brc = reinterpret_cast<Libmebo_brc *>(rc->priv);
  return static_cast<LibMeboStatus>(brc->get_qp(rc, qp));
}

LibMeboStatus libmebo_get_loop_filter_level(LibMeboRateController *rc,
                                            int *filter_level) {
  Libmebo_brc *brc = reinterpret_cast<Libmebo_brc *>(rc->priv);
  return static_cast<LibMeboStatus>(
      brc->get_loop_filter_level(rc, filter_level));
}
