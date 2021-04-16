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
#include <assert.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libmebo.h"
#include "brc/vp9/libvpx_derived/libvpx_vp9_rtc.h"
#include "brc/vp8/libvpx_derived/libvpx_vp8_rtc.h"
#include "brc/av1/aom_derived/aom_av1_rtc.h"

#define GET_LAYER_INDEX(s_layer, t_layer, num_temporal_layers) \
	((s_layer) * (num_temporal_layers) + (t_layer))

typedef LibMeboStatus (*libmebo_brc_init_fn)(
    LibMeboRateControllerConfig *rc_config, BrcCodecEnginePtr *handler);

typedef LibMeboStatus (*libmebo_brc_update_config_fn)(
    BrcCodecEnginePtr handler, LibMeboRateControllerConfig *rc_config);

typedef LibMeboStatus (*libmebo_brc_compute_qp_fn)(
    BrcCodecEnginePtr handler, LibMeboRCFrameParams *rc_frame_params);

typedef LibMeboStatus (*libmebo_brc_get_qp_fn)(
    BrcCodecEnginePtr handler, int *qp);

typedef LibMeboStatus (*libmebo_brc_get_loop_filter_fn)(
    BrcCodecEnginePtr handler, int *lf);

typedef LibMeboStatus (*libmebo_brc_post_encode_update_fn)(
    BrcCodecEnginePtr handler, uint64_t encoded_frame_size);

typedef void (*libmebo_brc_free_fn)(
    BrcCodecEnginePtr handler);

typedef struct LibMeboCodecInterface {
  libmebo_brc_init_fn init;
  libmebo_brc_update_config_fn update_config;
  libmebo_brc_compute_qp_fn compute_qp;
  libmebo_brc_get_qp_fn get_qp;
  libmebo_brc_get_loop_filter_fn get_loop_filter; 
  libmebo_brc_post_encode_update_fn post_encode_update;
  libmebo_brc_free_fn free;
} LibMeboCodecInterface;

typedef struct {
  LibMeboCodecInterface brc_interface;
  BrcCodecEnginePtr brc_codec_handler;
} LibMeboRateControllerPrivate;

typedef struct _brc_algo_map {
  LibMeboCodecType  codec_type;
  LibMeboBrcAlgorithmID algo_id;
  const char *description;
  LibMeboCodecInterface algo_interface;
} brc_algo_map;

static const brc_algo_map algo_impl_map[] = {
  {
    LIBMEBO_CODEC_VP8,
    LIBMEBO_BRC_ALGORITHM_DERIVED_LIBVPX_VP8,
    "VP8 BRC algorithm derived from libvpx",
    {
      brc_vp8_rate_control_init,
      brc_vp8_update_rate_control,
      brc_vp8_compute_qp,
      brc_vp8_get_qp,
      brc_vp8_get_loop_filter_level,
      brc_vp8_post_encode_update,
      brc_vp8_rate_control_free,
    },
  },
  {
    LIBMEBO_CODEC_VP9,
    LIBMEBO_BRC_ALGORITHM_DERIVED_LIBVPX_VP9,
    "VP9 BRC algorithm derivded from libvpx",
    {
      brc_vp9_rate_control_init,
      brc_vp9_update_rate_control,
      brc_vp9_compute_qp,
      brc_vp9_get_qp,
      brc_vp9_get_loop_filter_level,
      brc_vp9_post_encode_update,
      brc_vp9_rate_control_free,
    },
  },
  {
    LIBMEBO_CODEC_AV1,
    LIBMEBO_BRC_ALGORITHM_DERIVED_AOM_AV1,
    "AV1 BRC algorithm dervied from AOM",
    {
      brc_av1_rate_control_init,
      brc_av1_update_rate_control,
      brc_av1_compute_qp,
      brc_av1_get_qp,
      brc_av1_get_loop_filter_level,
      brc_av1_post_encode_update,
      brc_av1_rate_control_free,
    },
  },
  {
    LIBMEBO_CODEC_UNKNOWN,
    LIBMEBO_BRC_ALGORITHM_UNKNOWN,
    "Unknown"
  },
};

LibMeboBrcAlgorithmID
get_default_algorithm_id (LibMeboCodecType codec_type)
{
  switch (codec_type) {
    case LIBMEBO_CODEC_VP8:
      return LIBMEBO_BRC_ALGORITHM_DERIVED_LIBVPX_VP8;
    case LIBMEBO_CODEC_VP9:
      return LIBMEBO_BRC_ALGORITHM_DERIVED_LIBVPX_VP9;
    case LIBMEBO_CODEC_AV1:
      return LIBMEBO_BRC_ALGORITHM_DERIVED_AOM_AV1;
    case LIBMEBO_CODEC_UNKNOWN:
      return LIBMEBO_BRC_ALGORITHM_UNKNOWN;
    default:
      return LIBMEBO_BRC_ALGORITHM_UNKNOWN;
  }
  return LIBMEBO_BRC_ALGORITHM_UNKNOWN;
}

const brc_algo_map *
get_backend_impl (LibMeboCodecType codec_type,
    LibMeboBrcAlgorithmID algo_id)
{
  const brc_algo_map *bm = NULL;

  //Chose the default option
  if (algo_id == LIBMEBO_BRC_ALGORITHM_DEFAULT) {
    algo_id = get_default_algorithm_id (codec_type);
  }

  for (bm = algo_impl_map; bm->codec_type != LIBMEBO_CODEC_UNKNOWN; bm++){
    if (bm->algo_id == algo_id)
      return bm;
  }
  return NULL;
}

/********************** API *************************/

/**
 * \brief libmebo_rate_controller_get_loop_filter_level:
 *
 * Get the loop filter level for the current frame
 *
 * @param[in] rc                   LibMeboRateController to be initialized
 * @param[out] lf                  Retruns proposed loop-filter level for the current frame
 *
 * \return Retruns LibMeboStatus code
 */
LibMeboStatus
libmebo_rate_controller_get_loop_filter_level(LibMeboRateController *rc, int *lf)
{
  LibMeboStatus status = LIBMEBO_STATUS_UNKNOWN;
  LibMeboRateControllerPrivate *priv;

  if (!rc)
    return status;
  priv = (LibMeboRateControllerPrivate *)rc->priv;

  status = priv->brc_interface.get_loop_filter (priv->brc_codec_handler, lf);
  if (status != LIBMEBO_STATUS_SUCCESS)
    fprintf(stderr, "Failed to get the Loop filter level\n");

  return status;
}

/**
 * \brief libmebo_rate_controller_get_qp:
 *
 * Get the quantization parameter for the current frame
 *
 * @param[in] rc                   LibMeboRateController to be initialized
 * @param[out] qp                  Retruns proposed qp for the current frame
 *
 * \return Retrun LibMeboStatus code
 */
LibMeboStatus
libmebo_rate_controller_get_qp(LibMeboRateController *rc, int *qp)
{
  LibMeboStatus status = LIBMEBO_STATUS_UNKNOWN;
  LibMeboRateControllerPrivate *priv;

  if (!rc)
    return status;
  priv = (LibMeboRateControllerPrivate *)rc->priv;

  status = priv->brc_interface.get_qp (priv->brc_codec_handler, qp);
  if (status != LIBMEBO_STATUS_SUCCESS)
    fprintf(stderr, "Failed to get the QP\n");

  return status;
}

/**
 * \brief libmebo_rate_controller_compute_qp:
 *
 * Compute the quantization parameter for the current frame
 *
 * @param[in] rc                   LibMeboRateController to be initialized
 * @param[in] rc_frame_params      LibMeboRCFrameParams of current frame
 *
 * \return Retrun LibMeboStatus code
 *
 */
LibMeboStatus
libmebo_rate_controller_compute_qp (LibMeboRateController *rc,
    LibMeboRCFrameParams rc_frame_params)
{
  LibMeboStatus status = LIBMEBO_STATUS_UNKNOWN;
  LibMeboRateControllerPrivate *priv;

  if (!rc)
    return status;
  priv = (LibMeboRateControllerPrivate *)rc->priv;

  status = priv->brc_interface.compute_qp (priv->brc_codec_handler, &rc_frame_params);
  if (status != LIBMEBO_STATUS_SUCCESS)
    fprintf(stderr, "Failed to compute the QP\n");

  return status;
}

/**
 * \brief libmebo_rate_controller_post_encode_update:
 *
 * Update the LibMeboRateConroller instance with
 * the compressed sized of last encoded frame
 *
 * @param[in] rc                   LibMeboRateController to be initialized
 * @param[in] encoded_frame_size   Size of the last compressed frame
 *
 * \return Retrun LibMeboStatus code
 *
 */
LibMeboStatus
libmebo_rate_controller_post_encode_update (LibMeboRateController *rc,
    uint64_t encoded_frame_size)
{
  LibMeboStatus status = LIBMEBO_STATUS_UNKNOWN;
  LibMeboRateControllerPrivate *priv;

  if (!rc)
    return status;
  priv = (LibMeboRateControllerPrivate *)rc->priv;
  
  status = priv->brc_interface.post_encode_update (priv->brc_codec_handler,
		  encoded_frame_size);
  if (status != LIBMEBO_STATUS_SUCCESS)
    fprintf(stderr, "Failed to do the post encode update \n");

  return status;
}

/**
 * \brief libmebo_rate_controller_update_config:
 *
 * Update the LibMeboRateConroller instance with
 * brc params in LibMeboRateControllerConfig and returns
 * LIBMEBO_STATUS_SUCCESS on success
 *
 * @param[in] rc           LibMeboRateController to be initialized
 * @param[in] rc_config    LibMeboRateControllerConfig with brc params
 *
 * \return Retrun LibMeboStatus code
 */
LibMeboStatus
libmebo_rate_controller_update_config (LibMeboRateController *rc,
    LibMeboRateControllerConfig* rc_config)
{
  LibMeboStatus status = LIBMEBO_STATUS_UNKNOWN;
  LibMeboRateControllerPrivate *priv;

  if (!rc || !rc_config)
    return status;
  priv = (LibMeboRateControllerPrivate *)rc->priv;

  status = priv->brc_interface.update_config (priv->brc_codec_handler, rc_config);
  if (status != LIBMEBO_STATUS_SUCCESS)
    fprintf(stderr, "Failed to Update the RateController\n");

  return status;
}

/**
 * \brief libmebo_rate_controller_init:
 *
 * Initialize the LibMeboRateConroller instance with
 * brc params in LibMeboRateControllerConfig and returns
 * LIBMEBO_STATUS_SUCCESS on success
 *
 * @param[in] rc           LibMeboRateController to be initialized
 * @param[in] rc_config    LibMeboRateControllerConfig with brc params
 * 
 * \return Retrun LibMeboStatus code
 */
LibMeboStatus
libmebo_rate_controller_init (LibMeboRateController *rc,
    LibMeboRateControllerConfig *rc_config)
{
  LibMeboStatus status = LIBMEBO_STATUS_UNKNOWN;
  LibMeboRateControllerPrivate *priv;;

  if (!rc || !rc_config)
    return status;
  priv = (LibMeboRateControllerPrivate *)rc->priv;

  status = priv->brc_interface.init (rc_config, &priv->brc_codec_handler);
  if (status != LIBMEBO_STATUS_SUCCESS)
    fprintf(stderr, "Failed to Initialize the RateController\n");

  //ToDo: Make it explicit to enforce the algorithm implementor to validate
  //the input params 
  //brc_status = priv->brc_interface.validate (rc_config, &priv->brc_codec_handler);
 
  return status;
}

/**
 * \brief libmebo_rate_controller_free:
 *
 * Frees the @rc and set to NULL
 *
 * @param[in] rc LibMeboRateController to be freed.
 * 
 */
void
libmebo_rate_controller_free (LibMeboRateController *rc)
{
  if (!rc)
    return;

  LibMeboRateControllerPrivate *priv = (LibMeboRateControllerPrivate *)rc->priv;
  
  priv->brc_interface.free (priv->brc_codec_handler);
  free (rc->priv);
  free (rc);
  rc = NULL;
}


/**
 * \brief libmebo_rate_controller_new:
 *
 * Creates a new LibMeboRateController instance.
 * It should be freed with libmebo_rate_controller_free()
 * after the use.
 *
 * @param[in] codec_type    LibMeboCodecType for video codec in use
 * 
 * \return Retrun the newly created LibMeboRateController instance 
 */
LibMeboRateController *
libmebo_rate_controller_new (LibMeboCodecType codec_type,
    LibMeboBrcAlgorithmID algo_id) {
  LibMeboRateController *rc;
  LibMeboRateControllerPrivate *priv;

  const brc_algo_map *brc_backend = get_backend_impl (codec_type, algo_id);
  if (brc_backend == NULL) {
    fprintf (stderr, "Error: Unsupported Codec/Algorithm \n");
    return NULL;
  }

  rc = (LibMeboRateController *) malloc (sizeof(LibMeboRateController));
  if (!rc) {
    fprintf(stderr, "Failed allocation for LibMeboRateController \n");
    return NULL;
  }

  priv = (LibMeboRateControllerPrivate *) malloc (sizeof (LibMeboRateControllerPrivate));
  if (!priv) {
    fprintf(stderr, "Failed allocation for LibMeboRateController Private interface\n");
    if (rc)
      free(rc);
    return NULL;
  }
  priv->brc_interface = brc_backend->algo_interface;

  rc->priv = priv;
  rc->codec_type = codec_type;

  return rc;
}
