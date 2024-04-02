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
#include <iostream>
#include <assert.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef HAVE_LIBMEBO_CONFIG_H
# include "libmebo_config.h"
#endif

#include "libmebo.h"
#if LIBMEBO_ENABLE_VP9
#include "brc/vp9/libvpx_derived/libvpx_vp9_rtc.h"
#endif
#if LIBMEBO_ENABLE_VP8
#include "brc/vp8/libvpx_derived/libvpx_vp8_rtc.h"
#endif




