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
#include "libmebo.h"
#include "brc/vp9/libvpx_derived/ratectrl_rtc.h"

#define GET_LAYER_INDEX(s_layer, t_layer, num_temporal_layers) \
	((s_layer) * (num_temporal_layers) + (t_layer))

typedef struct {
  VP9RateControlRTC *vp9_rtc;
  VP9RateControlRtcConfig vp9_rtc_config;
  VP9FrameParamsQpRTC vp9_frame_params;

} LibMeboRateControllerPrivate;

LibMeboStatus
vp9_get_libmebo_status (VP9RateControlStatus status)
{
  LibMeboStatus libmebo_status = LIBMEBO_STATUS_FAILED;
 
  switch (status) {
    case STATUS_BRC_VP9_SUCCESS:
      libmebo_status = LIBMEBO_STATUS_SUCCESS;
      break;
    default:
      libmebo_status = LIBMEBO_STATUS_FAILED;
      break;    
  }
  return libmebo_status;
}

static void
vp9_update_config (LibMeboRateControllerConfig* rc_cfg,
    VP9RateControlRtcConfig *vp9_rtc_config)
{
  memset (vp9_rtc_config, 0, sizeof (VP9RateControlRtcConfig));

  vp9_rtc_config->width = rc_cfg->width;
  vp9_rtc_config->height = rc_cfg->height;
  vp9_rtc_config->max_quantizer = rc_cfg->max_quantizer;
  vp9_rtc_config->min_quantizer = rc_cfg->min_quantizer;
  vp9_rtc_config->target_bandwidth = rc_cfg->target_bandwidth;
  vp9_rtc_config->buf_initial_sz = rc_cfg->buf_initial_sz;
  vp9_rtc_config->buf_optimal_sz = rc_cfg->buf_optimal_sz;
  vp9_rtc_config->buf_sz = rc_cfg->buf_sz;
  vp9_rtc_config->undershoot_pct = rc_cfg->undershoot_pct;
  vp9_rtc_config->overshoot_pct = rc_cfg->overshoot_pct;
  vp9_rtc_config->max_intra_bitrate_pct = rc_cfg->max_intra_bitrate_pct;
  vp9_rtc_config->framerate = rc_cfg->framerate;

  // SVC params
  vp9_rtc_config->ss_number_layers = rc_cfg->ss_number_layers;
  vp9_rtc_config->ts_number_layers = rc_cfg->ts_number_layers;
  for (int i = 0 ; i<rc_cfg->ss_number_layers; i++) {
    for (int j = 0 ; j<rc_cfg->ts_number_layers; j++) {
      int layer = GET_LAYER_INDEX (i, j, rc_cfg->ts_number_layers);
      vp9_rtc_config->layer_target_bitrate[layer] = rc_cfg->layer_target_bitrate[layer];
      vp9_rtc_config->scaling_factor_num[layer] = rc_cfg->scaling_factor_num[layer];
      vp9_rtc_config->scaling_factor_den[layer] = rc_cfg->scaling_factor_den[layer];
      vp9_rtc_config->max_quantizers[layer] = rc_cfg->max_quantizers[layer];
      vp9_rtc_config->min_quantizers[layer] = rc_cfg->min_quantizers[layer];
      vp9_rtc_config->ts_rate_decimator[layer] = rc_cfg->ts_rate_decimator[layer];
    }
  }
}

/********************** API *************************/

/**
 * \brief libmebo_rate_controller_get_loop_filter_level:
 *
 * Get the loop filter level for the current frame
 *
 * @param[in] rc                   LibMeboRateController to be initialized
 *
 * \return Retruns proposed loop filter value for the current frame
 */
int
libmebo_rate_contoller_get_loop_filter_level(LibMeboRateController *rc)
{
  LibMeboRateControllerPrivate *priv = (LibMeboRateControllerPrivate *)rc->priv;
  int lf = 0;

  switch (rc->codec_type) {
    case LIBMEBO_CODEC_VP9:
      lf = brc_vp9_get_loop_filter_level (priv->vp9_rtc);
      break;

    default:
      break;
  }
  return lf;
}

/**
 * \brief libmebo_rate_controller_get_qp:
 *
 * Get the quantization parameter for the current frame
 *
 * @param[in] rc                   LibMeboRateController to be initialized
 *
 * \return Retruns proposed qp for the current frame
 */
int
libmebo_rate_controller_get_qp(LibMeboRateController *rc)
{
  LibMeboRateControllerPrivate *priv = (LibMeboRateControllerPrivate *)rc->priv;
  int qp = 0;

  switch (rc->codec_type) {
    case LIBMEBO_CODEC_VP9:
      qp = brc_vp9_get_qp (priv->vp9_rtc);
      break;

    default:
      break;
  }
  return qp;
}

/**
 * \brief libmebo_rate_controller_compute_qp:
 *
 * Compute the quantization parameter for the current frame
 *
 * @param[in] rc                   LibMeboRateController to be initialized
 * @param[in] rc_frame_params      LibMeboRCFrameParams of current frame
 *
 */
void
libmebo_rate_controller_compute_qp (LibMeboRateController *rc,
    LibMeboRCFrameParams rc_frame_params)
{
  LibMeboRateControllerPrivate *priv = (LibMeboRateControllerPrivate *)rc->priv;

  switch (rc->codec_type) {
    case LIBMEBO_CODEC_VP9:
      priv->vp9_frame_params.frame_type = rc_frame_params.frame_type;
      priv->vp9_frame_params.spatial_layer_id = rc_frame_params.spatial_layer_id;
      priv->vp9_frame_params.temporal_layer_id = rc_frame_params.temporal_layer_id;
      brc_vp9_compute_qp (priv->vp9_rtc, &priv->vp9_frame_params);
      break;

    default:
      break;
  }
}

/**
 * \brief libmebo_rate_controller_update_frame_size:
 *
 * Update the LibMeboRateConroller instance with
 * the compressed sized of last encoded frame
 *
 * @param[in] rc                   LibMeboRateController to be initialized
 * @param[in] encoded_frame_size   Size of the last compressed frame
 *
 */
void
libmebo_rate_controller_update_frame_size (LibMeboRateController *rc,
    uint64_t encoded_frame_size)
{
  LibMeboRateControllerPrivate *priv = (LibMeboRateControllerPrivate *)rc->priv;

  switch (rc->codec_type) {
    case LIBMEBO_CODEC_VP9:
      brc_vp9_post_encode_update (priv->vp9_rtc, encoded_frame_size);
      break;

    default:
      break;
  }
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
  LibMeboRateControllerPrivate *priv = (LibMeboRateControllerPrivate *)rc->priv;
  LibMeboStatus status = LIBMEBO_STATUS_SUCCESS;

  switch (rc->codec_type) {
    case LIBMEBO_CODEC_VP9:
      {
        VP9RateControlStatus vp9_status;
        vp9_update_config (rc_config, &priv->vp9_rtc_config);

        vp9_status = brc_vp9_validate (&priv->vp9_rtc_config);
	if (vp9_get_libmebo_status (vp9_status) != LIBMEBO_STATUS_SUCCESS)
          return LIBMEBO_STATUS_INVALID_PARAM;

        brc_vp9_update_rate_control (priv->vp9_rtc, &priv->vp9_rtc_config);
      }
      break;

    default:
      return LIBMEBO_STATUS_UNSUPPORTED_CODEC; 
      break;
  }
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
  LibMeboRateControllerPrivate *priv = (LibMeboRateControllerPrivate *)rc->priv;
  LibMeboStatus status = LIBMEBO_STATUS_SUCCESS;

  switch (rc->codec_type) {
    case LIBMEBO_CODEC_VP9:
      {
        VP9RateControlStatus vp9_status;
        vp9_update_config (rc_config, &priv->vp9_rtc_config);

        vp9_status = brc_vp9_validate (&priv->vp9_rtc_config);
	if (vp9_get_libmebo_status (vp9_status) != LIBMEBO_STATUS_SUCCESS)
          return LIBMEBO_STATUS_INVALID_PARAM;
	
        priv->vp9_rtc = brc_vp9_rate_control_new (&priv->vp9_rtc_config);
        if (!priv->vp9_rtc) {
          fprintf (stderr, "Failed to Initialize the vp9 brc \n");
          return LIBMEBO_STATUS_FAILED; 
        }
      }
      break;
    default:
      return LIBMEBO_STATUS_UNSUPPORTED_CODEC; 
  }
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

  if (rc->priv) {
      brc_vp9_rate_control_free (priv->vp9_rtc);
      free (rc->priv);
      rc->priv = NULL;
  }
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
libmebo_rate_controller_new (LibMeboCodecType codec_type) {
  LibMeboRateController *rc;
  LibMeboRateControllerPrivate *priv;

  if (codec_type != LIBMEBO_CODEC_VP9) {
    fprintf (stderr, "Error: Unsupported Codec \n");
    return NULL;
  }

  rc = (LibMeboRateController *) malloc (sizeof(LibMeboRateController));
  if (!rc) {
    fprintf(stderr, "Failed allocation for LibMeboRateController \n");
    return NULL;
  }

  priv = (LibMeboRateControllerPrivate *) malloc (sizeof (LibMeboRateControllerPrivate));
  if (!priv) {
    if (rc)
      free(rc);
    return NULL;
  }

  rc->priv = priv;
  rc->codec_type = codec_type;

  return rc;
}
