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

#ifndef AOM_AV1_COMMON_H_
#define AOM_AV1_COMMON_H_

#include "av1_ratectrl.h"
#include "av1_svc_layercontext.h"
//#include "av1_picklpf.h"

#define INVALID_IDX (-1)  // Invalid buffer index.

typedef struct AV1EncoderConfig {
  // Configuration related to the input video.
  InputCfg input_cfg;

  // Configuration related to frame-dimensions.
  FrameDimensionCfg frm_dim_cfg;

  // Configuration related to key-frames.
  KeyFrameCfg kf_cfg;

  // Rate control configuration
  RateControlCfg rc_cfg;

  // Configuration related to Tune.
  TuneCfg tune_cfg;

  // Configuration related to Quantization.
  QuantizationCfg q_cfg;

  // Indicates the spped preset to be used.
  int speed;

  // Indicates the target sequence level index for each operating point(OP).
  AV1_LEVEL target_seq_level_idx[MAX_NUM_OPERATING_POINTS];

  // Indicates the bitstream profile to be used.
  AV1_BITSTREAM_PROFILE profile;

  /*!\endcond */
  /*!
   * Indicates the current encoder pass :
   * 0 = 1 Pass encode,
   * 1 = First pass of two pass,
   * 2 = Second pass of two pass.
   *
   */
  enum aom_enc_pass pass;
  /*!\cond */

  // Indicates if the encoding is GOOD or REALTIME.
  ENC_MODE mode;

} AV1EncoderConfig;

typedef struct AV1_COMMON {
  //Information about the current frame that is being coded.
  CurrentFrame current_frame;

  int width;
  int height;

  PrevFrame prev_frame;

  /*!
   * If true, this frame is actually shown after decoding.
   * If false, this frame is coded in the bitstream, but not shown. It is only
   * used as a reference for other frames coded later.
   */
  int show_frame;

  /*!
   * Params related to MB_MODE_INFO arrays and related info.
   */
  CommonModeInfoParams mi_params;

  /*!
   * Quantization params.
   */
  CommonQuantParams quant_params;

  /*!
   * Elements part of the sequence header, that are applicable for all the
   * frames in the video.
   */
  SequenceHeader seq_params;
    /*!
   * Number of temporal layers: may be > 1 for SVC (scalable vector coding).
   */

  unsigned int number_temporal_layers;
  /*!
   * Temporal layer ID of this frame
   * (in the range 0 ... (number_temporal_layers - 1)).
   */
  int temporal_layer_id;

  /*!
   * Number of spatial layers: may be > 1 for SVC (scalable vector coding).
   */
  unsigned int number_spatial_layers;
  /*!
   * Spatial layer ID of this frame
   * (in the range 0 ... (number_spatial_layers - 1)).
   */
  int spatial_layer_id;

} AV1_COMMON;

typedef struct AV1_COMP {
  AV1_COMMON common;
  AV1EncoderConfig oxcf;
  AV1_RATE_CONTROL rc;

  /*!
   * Frame rate of the video.
   */
  double framerate;
   /*!
   * Bitmask indicating which reference buffers may be referenced by this frame.
   */
  int ref_frame_flags;

  /*!
   * speed is passed as a per-frame parameter into the encoder.
   */
  int speed;

  /*!
   * Information related to a gf group.
   */
  GF_GROUP gf_group;

  /*!
   * sf contains fine-grained config set internally based on speed.
   */
  AV1_SPEED_FEATURES sf;

  /*!
   * Stores the frame parameters during encoder initialization.
   */
  FRAME_INFO frame_info;

  /*!
   * Structure to store the dimensions of current frame.
   */
  InitialDimensions initial_dimensions;

   /*!
   * Number of MBs in the full-size frame; to be used to
   * normalize the firstpass stats. This will differ from the
   * number of MBs in the current frame when the frame is
   * scaled.
   */
  int initial_mbs;

  /*!
   * Resize related parameters.
   */
  ResizePendingParams resize_pending_params;

  /*!
   * A flag to indicate "real" screen content videos.
   * For example, screen shares, screen editing.
   * This type is true indicates |use_screen_content_tools| must be true.
   * In addition, rate control strategy is adjusted when this flag is true.
   */
  int is_screen_content_type;

  /*!
   * Indicates whether to use SVC.
   */
  int use_svc;
  /*!
   * Parameters for scalable video coding.
   */
  AV1_SVC svc;

  /*!
   * Flag indicating whether look ahead processing (LAP) is enabled.
   */
  int lap_enabled;

  /*!
   * Frame type of the last frame. May be used in some heuristics for speeding
   * up the encoding.
   */
  AV1_FRAME_TYPE last_frame_type;

  /*!
   * Super-resolution mode currently being used by the encoder.
   * This may / may not be same as user-supplied mode in oxcf->superres_mode
   * (when we are recoding to try multiple options for example).
   */
  aom_superres_mode superres_mode;
} AV1_COMP;

#endif
