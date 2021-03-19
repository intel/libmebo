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

/*
 * LibMebo API Specification
 *
 * Acknowledgements:
 *   Some concepts are borrowed from libvpx:
 *   Jerome Jiang <jianj@google.com> and Marco Paniconi <marpan@google.com>
 *   contributed to various aspects of the VP9 BRC solution.
 */

/**
 * \file libmebo.h
 * \brief APIs exposed in LibMebo
 *
 * This file contains the \ref api_core "LibMebo API".
 */

#ifndef __LIBMEBO_H__
#define __LIBMEBO_H__
#include <stdint.h>

/**
 * \mainpage Library for Media Encode Bitrate-control-algorithm Orchestration (libmebo)
 *
 * \section intro Introduction
 * LibMebo is an open source library for orchestrating the
 * bitrate algorithms for an encoder pipeline. The encoder
 * itself could be running on hardware or software.
 *
 * The intention for libmebo is to allow the various software and
 * hardware encoders across the frameworks and operatings systems
 * to use custom bitrate control alogorithms for specific
 * use cases.
 *
 * \section api_howto How to use the libmebo apis with an encoder
 *
 * \code
 * #include <libmebo/libmebo.h>
 *
 * LibMeboRateController *rc;
 * LibMeboRateControllerConfig rc_config;
 * LibMeboRCFrameParams rc_frame_param;
 * LibMeboStatus status;
 *
 * //Initialize the rc_config with required parameters
 * __InitializeConfig(rc_cfg);
 *
 * //Create an instance of libmebo
 * rc = libmebo_rate_controller_new (LIBMEBO_CODEC_VP9, LIBMEBO_BRC_ALGORITHM_DEFAULT);
 *
 * //Initialize the libmebo instance with the rc_config
 * status = libmebo_rate_controller_init (rc, &rc_config);
 * assert (status == LIBMEBO_STATUS_SUCCESS);
 *
 * //LibMebo in action
 * while (frame_to_encode) {
 *   if (rc_config_changed) {
 *     status = libmebo_rate_controller_update_config (rc, &rc_config);
 *     assert (status == LIBMEBO_STATUS_SUCCESS);
 *   }
 *
 *   //Initialize the per-frame parameters (frame_type & layer ids)
 *   __InitializeRCFrameParams (rc_frame_param);
 *
 *   //Compute the QP
 *   status = libmebo_rate_controller_compute_qp (rc, rc_frame_param);
 *   assert (status == LIBMEBO_STATUS_SUCCESS);
 *
 *   //Retrieve the QP for the next frame to be encoded
 *   status = libmebo_rate_controller_get_qp (rc, &qp);
 *   assert (status == LIBMEBO_STATUS_SUCCESS);
 *
 *   // Optional:libmebo can also recommend the loop-filter strength
 *   status = libmebo_rate_contoller_get_loop_filter_level (rc, &lf);
 * 
 *   // Ensure the status == LIBMEBO_STATUS_SUCCESS before using
 *   // the loop filter level since some algos are not supporting this API
 *
 *   //Encoder implementation that running in CQP mode
 *   __EncodeBitStream(qp, [lf])
 *
 *   //Update libmebo instance with encoded frame size
 *   status = libmebo_rate_controller_post_encode_update (rc, EncFrameSize);
 *   assert (status == LIBMEBO_STATUS_SUCCESS);
 * }
 * \endcode
*/

typedef void* BrcCodecEnginePtr;

/** 
 * Codec Types
 */
typedef enum {
  LIBMEBO_CODEC_VP8,
  LIBMEBO_CODEC_VP9,
  LIBMEBO_CODEC_AV1,
  LIBMEBO_CODEC_UNKNOWN,
} LibMeboCodecType;

/**
 * Backend algorithm IDs
 */
typedef enum
{
  LIBMEBO_BRC_ALGORITHM_DEFAULT,
  LIBMEBO_BRC_ALGORITHM_DERIVED_LIBVPX_VP8,
  LIBMEBO_BRC_ALGORITHM_DERIVED_LIBVPX_VP9,
  LIBMEBO_BRC_ALGORITHM_DERIVED_AOM_AV1,
  LIBMEBO_BRC_ALGORITHM_UNKNOWN,
} LibMeboBrcAlgorithmID;

/**
LIBMEBO_CODEC_VP8, * Return status type of libmebo APIs
 */
typedef enum {
  LIBMEBO_STATUS_SUCCESS,
  LIBMEBO_STATUS_WARNING,
  LIBMEBO_STATUS_ERROR,
  LIBMEBO_STATUS_FAILED,
  LIBMEBO_STATUS_INVALID_PARAM,
  LIBMEBO_STATUS_UNSUPPORTED_CODEC,
  LIBMEBO_STATUS_UNIMPLEMENTED,
  LIBMEBO_STATUS_UNKNOWN,
} LibMeboStatus;


/** 
 * Rate Control Modes
 */
typedef enum {
  LIBMEBO_RC_CQP,
  LIBMEBO_RC_CBR,
  LIBMEBO_RC_VBR
} LibMeboRateControlMode;

/** 
 * Frame prediction types
 */
typedef enum {
  LIBMEBO_KEY_FRAME = 0,
  LIBMEBO_INTER_FRAME = 1,
  LIBMEBO_FRAME_TYPES,
} LibMeboFrameType;

/** 
 * \biref Frame parameters
 *
 * This structure conveys frame level parameters and should be sent
 * once per frame
 */
typedef struct _LibMeboRCFrameParams {
  LibMeboFrameType frame_type;
  int spatial_layer_id;
  int temporal_layer_id;
} LibMeboRCFrameParams;

/* Temporal Scalability: Maximum number of coding layers */
#define LIBMEBO_TS_MAX_LAYERS 5

/* Spatial Scalability: Maximum number of Spatial coding layers */
#define LIBMEBO_SS_MAX_LAYERS 5

/** 
 * Temporal + Spatial Scalability: Maximum number of coding layers
 * 3 temporal + 4 spatial layers are allowed.
 */
#define LIBMEBO_MAX_LAYERS 12

/** 
 * \biref LibMebo Rate Controller configuration structure
 *
 * This structure conveys the encoding parameters required
 * for libmebo to configure the BRC instance.
 */
typedef struct _LibMeboRateControllerConfig {
  /** \brief pixel width of the bitstream to be encoded */
  int width;

  /** \brief pixel height of the bitstream to be encoded */
  int height;

  /**
   * \brief Maximum (Worst Quality) Quantizer
   *
   * The quantizer is the most direct control over the quality of the
   * encoded image. The range of valid values for the quantizer is codec
   * specific. Consult the documentation for the codec to determine the
   * values to use.
   */
  int max_quantizer;

  /*
   * \brief Minimum (Best Quality) Quantizer
   *
   * The quantizer is the most direct control over the quality of the
   * encoded image. The range of valid values for the quantizer is codec
   * specific. Consult the documentation for the codec to determine the
   * values to use.
   */
  int min_quantizer;

  /** \brief target bandwidth (in kilo bits per second) of the bitstream */
  int64_t target_bandwidth;

  /**
   * \brief Decoder(HRD) Buffer Initial Size
   *
   * This value indicates the amount of data that will be buffered by the
   * decoding application prior to beginning playback for the encoded stream.
   * This value is expressed in units of time (milliseconds).
   */
  int64_t buf_initial_sz;

  /**
   * \brief Decoder(HRD) Buffer Optimal Size
   *
   * This value indicates the amount of data that the encoder should try
   * to maintain in the decoder's buffer. This value is expressed in units
   * of time (milliseconds).
   */
  int64_t buf_optimal_sz;

  /*
   * \brief Decoder(HRD) Buffer Size
   *
   * This value indicates the amount of data that may be buffered by the
   * decoding application. Note that this value is expressed in units of
   * time (milliseconds). For example, a value of 5000 indicates that the
   * client will buffer (at least) 5000ms worth of encoded data.
   */
  int64_t buf_sz;

  /**
   * \brief Rate control adaptation undershoot control
   *
   * VP8: Expressed as a percentage of the target bitrate,
   * controls the maximum allowed adaptation speed of the codec.
   * This factor controls the maximum amount of bits that can
   * be subtracted from the target bitrate in order to compensate
   * for prior overshoot.
   * VP9: Expressed as a percentage of the target bitrate, a threshold
   * undershoot level (current rate vs target) beyond which more aggressive
   * corrective measures are taken.
   *
   * Valid values in the range VP8:0-1000 VP9: 0-100.
   */
  int undershoot_pct;

  /**
   * \brief Rate control adaptation overshoot control
   *
   * VP8: Expressed as a percentage of the target bitrate,
   * controls the maximum allowed adaptation speed of the codec.
   * This factor controls the maximum amount of bits that can
   * be added to the target bitrate in order to compensate for
   * prior undershoot.
   * VP9: Expressed as a percentage of the target bitrate, a threshold
   * overshoot level (current rate vs target) beyond which more aggressive
   * corrective measures are taken.
   *
   * Valid values in the range VP8:0-1000 VP9: 0-100.
   */
  int overshoot_pct;

  /**
   * \brief  maximum allowed bitrate for any intra frame in % of bitrate target
   */
  int max_intra_bitrate_pct;
 
  /**
   * \brief  maximum allowed bitrate for any inter frame in % of bitrate target
   */
  int max_inter_bitrate_pct;

  /** \brief framerate of the stream */
  double framerate;

  /**
   * \brief Number of spatial coding layers.
   *
   * This value specifies the number of spatial coding layers to be used.
   */
  int ss_number_layers;

  /*
   * \brief Number of temporal coding layers.
   *
   * This value specifies the number of temporal layers to be used.
   */
  int ts_number_layers;

  /**
   * \brief Maximum (Worst Quality) Quantizer for each layer
   */
  int max_quantizers[LIBMEBO_MAX_LAYERS];

  /*
   * \brief Minimum (Best Quality) Quantizer
   */
  int min_quantizers[LIBMEBO_MAX_LAYERS];

  /* \brief Scaling factor numerator for each spatial layer */
  int scaling_factor_num[LIBMEBO_SS_MAX_LAYERS];

  /* \brief Scaling factor denominator for each spatial layer */
  int scaling_factor_den[LIBMEBO_SS_MAX_LAYERS];

  /*
   * \brief Target bitrate for each spatial/temporal layer.
   *
   * These values specify the target coding bitrate to be used for each
   * spatial/temporal layer. (in kbps)
   *
   */
  int layer_target_bitrate[LIBMEBO_MAX_LAYERS];

  /*
   * \brief Frame rate decimation factor for each temporal layer.
   *
   * These values specify the frame rate decimation factors to apply
   * to each temporal layer.
   */
  int ts_rate_decimator[LIBMEBO_TS_MAX_LAYERS];

  /* Reserved bytes for future use, must be zero */
  uint32_t _libmebo_rc_config_reserved[32];
} LibMeboRateControllerConfig;

typedef struct _LibMeboRateController {
  void *priv;

  /* \brief Indicates the codec in use */
  LibMeboCodecType codec_type;

  /* \brief currently configured rate control parameters */
  LibMeboRateControllerConfig rc_config;

  /* Reserved bytes for future use, must be zero */
  uint32_t _libmebo_reserved[32];
} LibMeboRateController;

/******** API *************/

/**
 * \brief libmebo_rate_controller_new:
 *
 * Creates a new LibMeboRateController instance. It should be freed with
 * libmebo_rate_controller_free after use.
 *
 * \param[in]  codec_type  LibMeboCodecType of the codec
 * \param[in]  algo_id     LibMeboBrcAlgorithmID of the backend implementation
 *
 * \returns  Returns a pointer to LibMeboRateController, or NULL
 *           if fails to create the controller instance.
 */
LibMeboRateController *
libmebo_rate_controller_new (LibMeboCodecType codec_type,
                             LibMeboBrcAlgorithmID algo_id);

/**
 * \brief libmebo_rate_controller_init:
 *
 * Initialize the rate-controller instance with encode tuning
 * parameters provided in LibMeboRateControllerConfig
 *
 * \param[in]  rc         LibMeboRateController to be initialized
 * \param[in]  rc_config  LibMeboRateControllerConfig with pre-filled enc params
 *
 * \returns  Returns a LibMeboStatus
 */
LibMeboStatus
libmebo_rate_controller_init (LibMeboRateController *rc,
                              LibMeboRateControllerConfig *rc_config);

/**
 * libmebo_rate_controller_free
 *
 * \param[in]    rc    the LibMeboRateController to free
 *
 * Frees #rc and sets it to NULL
 */
void libmebo_rate_controller_free (LibMeboRateController *rc);

/**
 * \brief libmebo_rate_controller_update_config:
 *
 * Update the rate-controller instance with encode tuning
 * parameters provided in LibMeboRateControllerConfig
 *
 * \param[in]  rc         LibMeboRateController to be updated
 * \param[in]  rc_config  LibMeboRateControllerConfig with pre-filled enc params
 *
 * \returns  Returns a LibMeboStatus
 */
LibMeboStatus
libmebo_rate_controller_update_config (LibMeboRateController *rc,
                                                     LibMeboRateControllerConfig*rc_cfg);

/**
 * libmebo_rate_controller_post_encode_update:
 *
 * This is a post-encode operation.
 * Update the currently encoded frame's size at #rc instance.
 *
 * \param[in]    rc                  the LibMeboRateController to update framesize
 * \param[in]    encoded_frame_size  size of the compressed frame
 *
 * \returns  Returns a LibMeboStatus
 */
LibMeboStatus
libmebo_rate_controller_post_encode_update (LibMeboRateController *rc,
                                                uint64_t encoded_frame_size);

/**
 * libmebo_rate_controller_compute_qp:
 *
 * Instruct the libmebo api to calculate the QP.
 * At this point libmebo start executing the qp calculation algorithm.
 *
 * \param[in]    rc               the LibMeboRateController
 * \param[in]    rc_frame_params  current frame params
 *
 * \returns  Returns a LibMeboStatus
 *
 */
LibMeboStatus
libmebo_rate_controller_compute_qp (LibMeboRateController *rc,
                                    LibMeboRCFrameParams rc_frame_params);

/**
 * libmebo_rate_controller_get_qp:
 *
 * Retrieve the current qp estimate from libmebo instance
 *
 * \param[in]    rc               the LibMeboRateController instance
 * \param[out]   qp               Retruns proposed qp for the current frame
 *
 * \returns  LibMeboStatus code
 */
LibMeboStatus
libmebo_rate_controller_get_qp(LibMeboRateController *rc, int *qp);

/**
 * libmebo_rate_contoller_get_loop_filter_level:
 *
 * Retrieve the current loop filter strength estimate
 * from libmebo instance
 *
 * \param[in]    rc               The LibMeboRateController instance
 * \param[out]   lf               Retruns proposed loop-filter level for the current frame
 *
 * \return Retruns LibMeboStatus code
 */
LibMeboStatus
libmebo_rate_contoller_get_loop_filter_level(LibMeboRateController *rc, int *lf);

#endif
