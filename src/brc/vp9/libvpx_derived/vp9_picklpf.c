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

#include "vp9_picklpf.h"
#include "vp9_common.h"

#define ROUND_POWER_OF_TWO(value, n) (((value) + (1 << ((n)-1))) >> (n))
#define ROUND64_POWER_OF_TWO(value, n) (((value) + (1ULL << ((n)-1))) >> (n))


static int get_max_filter_level(const VP9_COMP *cpi) {
  if (cpi->oxcf.pass == 2) {
    //Fixme : Two pass is not yet supported
    return -1;
  } else {
    return MAX_LOOP_FILTER;
  }
}

static int
clamp (int value, int low, int high)
{
  return value < low ? low : (value > high ? high : value);
}

void vp9_pick_filter_level(VP9_COMP *cpi,
                           LPF_PICK_METHOD method) {
  VP9_COMMON *const cm = &cpi->common;
  struct loopfilter *const lf = &cm->lf;

  if (method == LPF_PICK_MINIMAL_LPF && lf->filter_level) {
    lf->filter_level = 0;
  } else if (method >= LPF_PICK_FROM_Q) {
    const int min_filter_level = 0;
    const int max_filter_level = get_max_filter_level(cpi);
    const int q = vp9_ac_quant(cm->base_qindex, 0, cm->bit_depth);
#if 0
// These values were determined by linear fitting the result of the
// searched level, filt_guess = q * 0.316206 + 3.87252
    int filt_guess;
    switch (cm->bit_depth) {
      case VPX_BITS_8:
        filt_guess = ROUND_POWER_OF_TWO(q * 20723 + 1015158, 18);
        break;
      case VPX_BITS_10:
        filt_guess = ROUND_POWER_OF_TWO(q * 20723 + 4060632, 20);
        break;
      default:
        assert(cm->bit_depth == VPX_BITS_12);
        filt_guess = ROUND_POWER_OF_TWO(q * 20723 + 16242526, 22);
        break;
    }
#endif
    int filt_guess = ROUND_POWER_OF_TWO(q * 20723 + 1015158, 18);

    //Fixme: no infra available to communicate the segmentation info
    if (cpi->oxcf.pass == 0 && cpi->oxcf.rc_mode == VPX_CBR &&
        cpi->oxcf.aq_mode == CYCLIC_REFRESH_AQ && /*cm->seg.enabled && */
        (cm->base_qindex < 200 || cm->width * cm->height > 320 * 240) &&
        cpi->oxcf.content != VP9E_CONTENT_SCREEN && cm->frame_type != KEY_FRAME)
      filt_guess = 5 * filt_guess >> 3;

    if (cm->frame_type == KEY_FRAME) filt_guess -= 4;
    lf->filter_level = clamp(filt_guess, min_filter_level, max_filter_level);
  } //else {
    //lf->filter_level =
    //    search_filter_level(sd, cpi, method == LPF_PICK_FROM_SUBIMAGE);
  //}
}
