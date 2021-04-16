/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *  Copyright (c) 2020 Intel Corporation
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VPX_VP9_ENCODER_VP9_PICKLPF_H_
#define VPX_VP9_ENCODER_VP9_PICKLPF_H_

#include "libvpx_vp9_ratectrl.h"

#define MAX_LOOP_FILTER 63

typedef enum {
  // Try the full image with different values.
  LPF_PICK_FROM_FULL_IMAGE,
  // Try a small portion of the image with different values.
  LPF_PICK_FROM_SUBIMAGE,
  // Estimate the level based on quantizer and frame type
  LPF_PICK_FROM_Q,
  // Pick 0 to disable LPF if LPF was enabled last frame
  LPF_PICK_MINIMAL_LPF
} LPF_PICK_METHOD;

struct loopfilter {
  int filter_level;
};

void brc_vp9_pick_filter_level(struct VP9_COMP *cpi, LPF_PICK_METHOD method);

#endif  // VPX_VP9_ENCODER_VP9_PICKLPF_H_
