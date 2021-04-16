/*
 *  Copyright (c) 2020 The WebM project authors. All Rights Reserved.
 *  Copyright (c) 2020 Intel corporation. All Rights Reserved.
 *  Copyright (c) 2020 Sreerenj Balachandran<sreerenj.balachandran@intel.com>
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef LIBMEBO_BRC_VP8_RATECTRL_H
#define LIBMEBO_BRC_VP8_RATECTRL_H

#include <assert.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "libvpx_vp8_common.h"

void libvpx_vp8_update_rate_correction_factors(VP8_COMP *cpi, int damp_var);
int libvpx_vp8_regulate_q(VP8_COMP *cpi, int target_bits_per_frame);
void libvpx_vp8_compute_frame_size_bounds(VP8_COMP *cpi,
                                   int *frame_under_shoot_limit,
                                   int *frame_over_shoot_limit);

/* return of 0 means drop frame */
int libvpx_vp8_pick_frame_size(VP8_COMP *cpi);

int libvpx_vp8_drop_encodedframe_overshoot(VP8_COMP *cpi, int Q);

void libvpx_vp8_new_framerate(VP8_COMP *cpi, double framerate);

void
libvpx_vp8_rc_postencode_update (VP8_COMP *cpi_, uint64_t encoded_frame_size);

#endif  // LIBMEBO_BRC_VP8_RATECTRL_H
