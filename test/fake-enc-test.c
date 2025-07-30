/*  Copyright (c) 2020 Intel Corporation. All Rights Reserved.
 *  Copyright (c) 2020 Sreerenj Balachandran <sreerenj.balachandran@intel.com>
 *  Author: Sreerenj Balachandran <sreerenj.balachandran@intel.com>
 *
 * sample command:
    ./fake-enc --codec=VP9 --preset=0 --framecount=120
*/

#include <stdio.h>
#include <string.h>
#include <libmebo.hpp>
#include <getopt.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>

#define MaxSpatialLayers 3
#define MaxTemporalLayers 3

static LibMeboRateController *libmebo_rc;
static LibMeboRateControllerConfig libmebo_rc_config;

typedef enum CodecID
{
  VP8_ID = 0,
  VP9_ID = 1,
  AV1_ID = 2,
} CodecID;

struct EncParams {
  unsigned int preset;
  unsigned int id;
  unsigned int bitrate; //in kbps
  unsigned int framerate;
  unsigned int width;
  unsigned int height;
  unsigned int framecount; //Number of Frames to be encoded
  unsigned int num_sl; //Number of Spatial Layers
  unsigned int num_tl; //Number of Temporal Layers
  unsigned int dynamic_rate_change; //dynamic rate change enablement flag
};

struct BitrateBounds {
  unsigned int lower; //in kbps
  unsigned int upper; //in kbps
};

struct SvcBitrateBounds {
  int layer_bitrate_lower[MaxSpatialLayers][MaxTemporalLayers];
  int layer_bitrate_upper[MaxSpatialLayers][MaxTemporalLayers];
};

//Default config
static struct EncParams enc_params =
{
    .preset = 0,
    .id = VP9_ID,
    .bitrate = 1024,
    .framerate = 30,
    .width = 320,
    .height = 240,
    .framecount = 100,
    .num_sl = 1,
    .num_tl = 1,
    .dynamic_rate_change = 0,
  };

static struct EncParams preset_list [] = {
	{0, 1, 256, 30, 320, 240, 100, 1, 1, 0},
	{1, 1, 512, 30, 320, 240, 100, 1, 1, 0},
	{2, 1, 1024, 30, 320, 240, 100, 1, 1, 0},
	{3, 1, 256, 30, 640, 480, 100, 1, 1, 0},
	{4, 1, 512, 30, 640, 480, 100, 1, 1, 0},
	{5, 1, 1024, 30, 640, 480, 100, 1, 1, 0},
	{6, 1, 1024, 30, 1280, 720, 100, 1, 1, 0},
	{7, 1, 2048, 30, 1280, 720, 100, 1, 1, 0},
	{8, 1, 4096, 30, 1280, 720, 100, 1, 1, 0},
	{9, 1, 1024, 30, 1920, 1080, 100, 1, 1, 0},
	{10, 1, 4096, 30, 1920, 1080, 100, 1, 1, 0},
	{11, 1, 8192, 30, 1920, 1080, 100,1, 1, 0},
	{12, 1, 8192, 30, 1920, 1080, 100,1, 1, 1},
	{13, 1, 4096, 30, 1280, 720, 100, 3, 2, 0},
};

//heuristics to predict a decent key/intra-frame size
static struct BitrateBounds bitrate_bounds_intra [] = {
	{3500, 4440}, {3700, 5600}, {5500,9500}, //qvga-intra-frames
	{4100, 7400}, {3700, 11150}, {10100,16100}, //vga-intra-frames
	{16000, 25600}, {27600,35800}, {30100,63100}, //hd-intra-frames
	{14400,30000}, {60000,75100}, {65400,126600}, //full_hd-intra-frames
	{65400,126600}, //full_hd-intra-frames
};

//heuristics to predict a decent inter-frame size
static struct BitrateBounds bitrate_bounds_inter [] = {
	{800, 1170}, {1700, 2200}, {3000, 5000}, //qvga-inter-frames
	{900, 1200}, {1800, 2200}, {3600, 4500}, //vga-inter-frames
	{3000, 4500}, {7100, 8400}, {14700,17500}, //hd-inter-frames
	{3100,8500}, {14100,16500}, {30000,35600}, //full_hd-inter-frames
	{30000,35600}, //full_hd-inter-frames
};

//heuristics to predict a decent key/intra-frame size for SVC
static struct SvcBitrateBounds svc_bitrate_bounds_intra [] = {
        {
                .layer_bitrate_lower = {
                        {5957,  5957,  0},
                        {8007,  8007,  0},
                        {17520, 17520, 0},
                },
                .layer_bitrate_upper = {
                        {9884,  9884,  0},
                        {17241, 17241, 0},
                        {19084, 19084, 0},
                }
        },
};

//heuristics to predict a decent inter-frame size for SVC
static struct SvcBitrateBounds svc_bitrate_bounds_inter [] = {
        {
                .layer_bitrate_lower = {
                        {4520,  4520,  0},
                        {4876,  4876,  0},
                        {4327,  4327,  0},
                },
                .layer_bitrate_upper = {
                        {5700,  5700,  0},
                        {5670,  5670,  0},
                        {5689,  5689,  0},
                }
        },
};

int layered_bitrates[MaxSpatialLayers][MaxTemporalLayers];
int layered_frame_rate[MaxTemporalLayers];
int layered_stream_size[MaxSpatialLayers][MaxTemporalLayers];
int layered_frame_count[MaxSpatialLayers][MaxTemporalLayers];

#define LAYER_IDS_TO_IDX(sl, tl, num_tl) ((sl) * (num_tl) + (tl))

unsigned int dynamic_size[2] = {0, 0};
unsigned int dynamic_bitrates[2] = {0, 0};

static int verbose = 0;

static char*
get_codec_id_string (CodecID id)
{
  switch (id) {
    case VP8_ID:
      return "VP8";
    case VP9_ID:
      return "VP9";
    case AV1_ID:
      return "AV1";
    default:
      return "Unknown";
  }
}

static void show_help()
{
  printf("Usage: \n"
		  "  fake-enc [--codec=VP8|VP9|AV1] [--framecount=frame count] "
		  "[--preset= 0 to 13] \n\n"
		  "    Preset0: QVGA_256kbps_30fps \n"
		  "    Preset1: QVGA_512kbps_30fps \n"
		  "    Preset2: QVGA_1024kbps_30fps \n"
		  "    Preset3: VGA_256kbps_30fps \n"
		  "    Preset4: VGA_512kbps_30fps \n"
		  "    Preset5: VGA_1024kbps_30fps \n"
		  "    Preset6: HD_1024kbps_30fps \n"
		  "    Preset7: HD_2048kbps_30fps \n"
		  "    Preset8: HD_4096kbps_30fps \n"
		  "    Preset9: FULLHD_1024kbps_30fps \n"
		  "    Preset10: FULLHD_4096kbps_30fps \n"
		  "    Preset11: FULLHD_8192kbps_30fps \n"
		  //Please add any non-SVC, non-dynamirc-rate-change
		  // presets here and increment the SVC_PRESET_START_INDEX
		  // and DYNAMIC_RATE_CHAGE_PRESET_START_INDEX
		  "    Preset12: FULLHD_8192kbps_30fps_DynamicRateChange \n"
		  //Please add any non-SVC presets here and
		  //increment the SVC_PRESET_START_INDEX
		  "    Preset13: SVC_HD_4096kbps_30fps_S3T2 \n"
		 "\n");
}

#define DYNAMIC_RATE_CHAGE_PRESET_START_INDEX 12
#define SVC_PRESET_START_INDEX 13

static void
parse_args(int argc, char **argv)
{
  int c,option_index;

  static const struct option long_options[] = {
        {"help", no_argument, 0, 0},
        {"codec", required_argument, 0, 1},
        {"preset", required_argument, 0, 2},
        {"framecount", required_argument, 0, 3},
        {"temporal-layers", required_argument, 0, 4},
        {"spatial-layers", required_argument, 0, 5},
        {"dynamic-rate-change", required_argument, 0, 6},
        {"verbose", required_argument, 0, 7},
        { NULL,  0, NULL, 0 }
  };

  while (1) {
    c = getopt_long_only (argc, argv,
                     "hcb:?",
                     long_options,
                     &option_index);
    if (c == -1)
      break;

    switch(c) {
      case 1:
	if (!strcmp (optarg, "VP8"))
	  enc_params.id = VP8_ID;
	else if (!strcmp (optarg, "VP9"))
	  enc_params.id = VP9_ID;
	else
	  enc_params.id = AV1_ID;
        break;
      case 2: {
	  int preset = atoi(optarg);
	  CodecID id = enc_params.id;
          if (preset < 0 || preset > SVC_PRESET_START_INDEX) {
            printf ("Unknown preset, Failed \n");
            exit(0);
	  }
	  enc_params = preset_list[preset];
	  enc_params.id = id;
	}
	break;
      case 3:
        enc_params.framecount = atoi(optarg);
	break;
      case 7:
        verbose = atoi(optarg);
	break;
      default:
        break;
    }
  }
}

// The return value is expressed as a percentage of the average. For example,
// to allocate no more than 4.5 frames worth of bitrate to a keyframe, the
// return value is 450.
uint32_t MaxSizeOfKeyframeAsPercentage(uint32_t optimal_buffer_size,
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

static void
InitLayeredBitrateAlloc (int num_sl,
    int num_tl, int bitrate)
{
  int sl, tl;

  assert (num_sl && num_sl <= MaxSpatialLayers);
  assert (num_tl && num_tl <= MaxTemporalLayers);

  for (sl=0; sl<num_sl; sl++) {
    int sl_rate = bitrate/num_sl;
    for (tl=0; tl<num_tl; tl++) {
      layered_bitrates[sl][tl] = sl_rate/num_tl;
    }
  }
}

static void
InitLayeredFramerate (int num_tl, int framerate,
                      int *ts_rate_decimator)
{
  for (int tl = 0; tl < num_tl; tl++) {
    if (tl == 0)
      layered_frame_rate [tl] = framerate / ts_rate_decimator[tl];
    else
      layered_frame_rate [tl] =
        (framerate / ts_rate_decimator[tl]) -
		  (framerate / ts_rate_decimator[tl-1]);
  }
}

static int
GetBitratekBps (int sl_id, int tl_id)
{
  return layered_bitrates[sl_id][tl_id];
}

static int
libmebo_software_brc_init (
    LibMeboRateController *rc,
    LibMeboRateControllerConfig *rc_config)
{
  LibMeboStatus status;

  rc_config->width = enc_params.width;
  rc_config->height = enc_params.height;
  rc_config->max_quantizer = 63;
  rc_config->min_quantizer = 0;
  rc_config->target_bandwidth = enc_params.bitrate;
  rc_config->buf_initial_sz = 500;
  rc_config->buf_optimal_sz = 600;;
  rc_config->buf_sz = 1000;
  rc_config->undershoot_pct = 50;
  rc_config->overshoot_pct = 50;
  rc_config->buf_initial_sz = 500;
  rc_config->buf_optimal_sz = 600;;
  rc_config->buf_sz = 1000;
  rc_config->undershoot_pct = 50;
  rc_config->overshoot_pct = 50;
  //Fixme
  rc_config->max_intra_bitrate_pct = MaxSizeOfKeyframeAsPercentage(
      rc_config->buf_optimal_sz, enc_params.framerate);
  rc_config->max_intra_bitrate_pct = 0;
  rc_config->framerate = enc_params.framerate;

  rc_config->max_quantizers[0] = rc_config->max_quantizer;
  rc_config->min_quantizers[0] = rc_config->min_quantizer;

  rc_config->ss_number_layers = enc_params.num_sl;
  rc_config->ts_number_layers = enc_params.num_tl;

  switch (enc_params.num_sl) {
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
  for (unsigned int sl = 0; sl < enc_params.num_sl; sl++) {
    int bitrate_sum = 0;
    for (unsigned int tl = 0; tl < enc_params.num_tl; tl++) {
      const int layer_id = LAYER_IDS_TO_IDX(sl, tl, enc_params.num_tl);
      rc_config->max_quantizers[layer_id] = rc_config->max_quantizer;
      rc_config->min_quantizers[layer_id] = rc_config->min_quantizer;
      bitrate_sum += GetBitratekBps(sl, tl);
      rc_config->layer_target_bitrate[layer_id] = bitrate_sum;
      rc_config->ts_rate_decimator[tl] = 1u << (enc_params.num_tl - tl - 1);
    }
    InitLayeredFramerate (enc_params.num_tl,enc_params.framerate,
		    rc_config->ts_rate_decimator);
  }

  status = libmebo_rate_controller_init (rc, rc_config);
  if (status != LIBMEBO_STATUS_SUCCESS)
    return 0;

  return 1;
}

static void display_encode_status (unsigned int bitstream_size) {

  printf ("======= Encoder Input Configuration ======= \n");
  printf (	  "Codec      = %s \n"
		  "bitrate    = %d kbps \n"
		  "framerate  = %d \n"
		  "width      = %d \n"
		  "height     = %d \n"
		  "framecount = %d \n",
		  get_codec_id_string (enc_params.id),
		  enc_params.bitrate,
		  enc_params.framerate,
		  enc_params.width,
		  enc_params.height,
		  enc_params.framecount);
  if (enc_params.num_sl > 1 || enc_params.num_tl > 1) {
    printf (	  "Target bitrates in kbps:\n");
    for (unsigned int sl = 0; sl <enc_params.num_sl; sl++) {
      int bitrate = 0;
      for (unsigned int tl = 0; tl <enc_params.num_tl; tl++) {
        bitrate += layered_bitrates[sl][tl];
        printf ("S%dT%d target bitrate = %d \n",sl, tl, layered_bitrates[sl][tl]);
      }
      printf ("SpatialLayer[%d] target bitrate: %d\n", sl, bitrate);
    }
  }

  printf ("\n============ Encode Staus Report ============ \n\n");
  //SVC
  if (enc_params.num_sl > 1 || enc_params.num_tl > 1) {
    int total_bitrate = 0;
    for (unsigned int sl = 0; sl < enc_params.num_sl; sl++) {
      for (unsigned int tl = 0; tl < enc_params.num_tl; tl++) {
        int bitrate = 0;
        bitrate = (((layered_stream_size [sl][tl] / layered_frame_count[sl][tl]) *
                   layered_frame_rate[tl]) * 8) / 1000;
        total_bitrate += bitrate;
        printf ("SpatialLayer[%d]TemporlLayer[%d]: \n"
              "  Bitrate (with out including any other layers)     = %d kbps \n"
              "  Bitrate (accumulated the lower layer targets)     = %d kbps \n",
	      sl, tl, bitrate, total_bitrate);
      }
    }
  }

  //Dynamic Rate Chaange
  if (enc_params.dynamic_rate_change) {
    int frames_count = enc_params.framecount/2;
    printf ("Bitrate used for the streams with %d target bitrate = %d kbps\n\n",
            dynamic_bitrates[0],
            (((dynamic_size[0] / frames_count) *
		     enc_params.framerate) * 8 ) / 1000);
    printf ("Bitrate used for the streams with %d target bitrate = %d kbps\n\n",
            dynamic_bitrates[1],
            (((dynamic_size[1] / frames_count) *
		     enc_params.framerate) * 8 ) / 1000);
  }
  printf ("Bitrate of the Compressed stream = %d kbps\n\n",
		   (((bitstream_size / enc_params.framecount) *
		     enc_params.framerate) * 8 ) / 1000);
  printf ("\n");
}

static int _prev_temporal_id = 0;

static void
get_layer_ids (int frame_count, int num_sl, int num_tl,
    int *spatial_id, int *temporal_id)
{
  int s_id =0, t_id = 0;

  //spatial id
  s_id =  frame_count % num_sl;

  //Increment the temporal_id only when all the spatial
  //variants are encoded for a particualr frame id
  if (!(frame_count % num_sl)) {
    //temporal id
    if (num_tl > 1) {
      switch (num_tl) {
        case 2:
          if (frame_count % 2 == 0)
            t_id = 0;
	  else
            t_id = 1;
          break;
        case 3:
          if (frame_count % 4 == 0)
            t_id = 0;
	  else if (frame_count % 2 == 0)
            t_id = 1;
	  else
            t_id = 2;
          break;
        default:
          printf ("Exit: Not supporting more than 3 temporal layers \n");
          exit(0);
      }
    } else {
      t_id = 0;
    }
  } else {
    t_id = _prev_temporal_id;
  }

  *spatial_id = s_id;
  *temporal_id = t_id;
  _prev_temporal_id = t_id;
}

static void
start_virtual_encode (LibMeboRateController *rc)
{
   int i, qp = 0;
   int key_frame_period = 30;
   uint32_t buf_size = 1024;
   uint32_t total_size = 0;
   int prev_qp = 0;
   uint32_t predicted_size = 0;
   uint32_t lower =0, upper=0;
   LibMeboFrameType libmebo_frame_type;
   LibMeboRCFrameParams rc_frame_params;
   unsigned int preset = enc_params.preset;
   unsigned int svc_preset = 0;
   int frame_count = 0;
   unsigned int prev_is_key = 0;

   if (verbose)
     printf ("=======Fake Encode starts ==============\n");

   if (enc_params.num_sl > 1 || enc_params.num_tl >1)
     svc_preset = preset - SVC_PRESET_START_INDEX;

   key_frame_period *= enc_params.num_sl;

   //Account spatial svc in frame_count
   frame_count = enc_params.framecount * enc_params.num_sl;

   srand(time(NULL));
   for (i = 0; i < frame_count; i++) {
     LibMeboStatus status;
     int spatial_id;
     int temporal_id;
     int update_rate = 0;
     unsigned int *dyn_size;
     predicted_size = 0;
     lower =0;
     upper=0;

     get_layer_ids (i,
         enc_params.num_sl, enc_params.num_tl,
	 &spatial_id, &temporal_id);

     //Dynamic rate reset: Reduce the framerate by 1/8 th
     if (enc_params.dynamic_rate_change &&
		     i >= frame_count/2){
       preset  = 9;//bitrate change from 4096 to 1024
       dyn_size = &dynamic_size[1];
       update_rate = (i == frame_count/2) ? 1 : 0;
     } else {
       dyn_size = &dynamic_size[0];
     }

     if (i % key_frame_period == 0) {
       libmebo_frame_type = LIBMEBO_KEY_FRAME;
       if (preset < SVC_PRESET_START_INDEX) {
         lower = bitrate_bounds_intra [preset].lower;
         upper = bitrate_bounds_intra [preset].upper;
       } else {
         //SVC
         lower =
             svc_bitrate_bounds_intra[svc_preset].
	         layer_bitrate_lower[spatial_id][temporal_id];
         upper =
             svc_bitrate_bounds_intra[svc_preset].
	         layer_bitrate_upper[spatial_id][temporal_id];
       }
     }
     else {
       libmebo_frame_type = LIBMEBO_INTER_FRAME;
       if (preset < SVC_PRESET_START_INDEX) {
         lower = bitrate_bounds_inter [preset].lower;
         upper = bitrate_bounds_inter [preset].upper;
       } else {
         //SVC
         lower =
             svc_bitrate_bounds_inter[svc_preset].
	         layer_bitrate_lower[spatial_id][temporal_id];
         upper =
             svc_bitrate_bounds_inter[svc_preset].
	         layer_bitrate_upper[spatial_id][temporal_id];
       }
     }
     predicted_size = (rand() % (upper - lower)) + lower;

     //Update libmebo rate control config
     if (update_rate) {
      dynamic_bitrates[0] = libmebo_rc_config.target_bandwidth; 
      libmebo_rc_config.target_bandwidth /= 8;
      dynamic_bitrates[1] = libmebo_rc_config.target_bandwidth;
      status = libmebo_rate_controller_update_config (rc, &libmebo_rc_config); 
      assert (status == LIBMEBO_STATUS_SUCCESS);
     }

     rc_frame_params.frame_type = libmebo_frame_type;

     //Set spatial layer id
     rc_frame_params.spatial_layer_id =  spatial_id;
     rc_frame_params.temporal_layer_id = temporal_id;

     status = libmebo_rate_controller_compute_qp (rc, rc_frame_params);
     assert (status == LIBMEBO_STATUS_SUCCESS);

     status  = libmebo_rate_controller_get_qp (libmebo_rc, &qp);
     assert (status == LIBMEBO_STATUS_SUCCESS);

     if (verbose)
       printf ("QP = %d \n", qp);

     buf_size = predicted_size;

     //Heuristics to calculate a reasonable value of the
     //compressed frame size
     if (i != 0 && !prev_is_key) {
       int qp_val  = (qp == 0) ? 1 : qp;
       int size_range = upper - lower;
       int qp_range_length = size_range / 256;
       int suggested_size = lower + (qp_val * qp_range_length);
       int new_predicted_size = 0;

       if ((qp) < prev_qp)
         new_predicted_size = (rand() % (upper - suggested_size)) + suggested_size;
       else
	 new_predicted_size = (rand() % (suggested_size - lower)) + lower;

       buf_size = new_predicted_size;
     }

     prev_qp = qp;

     if (verbose)
       printf ("PostEncodeBufferSize = %d \n",buf_size);

     status = libmebo_rate_controller_post_encode_update (rc, buf_size);
     assert (status == LIBMEBO_STATUS_SUCCESS);

     // Calculate per layer stream size
     if (enc_params.num_tl > 1 || enc_params.num_sl > 1) {
       layered_stream_size[spatial_id][temporal_id] +=
           buf_size;
       layered_frame_count[spatial_id][temporal_id] += 1;
       if (verbose) {
         printf ("PostEncodeBufferSize of "
             "spatial_layer[%d],temporal_layer[%d]  = %d \n",
	     0, rc_frame_params.temporal_layer_id,
	     buf_size);
       }
     }
     total_size = total_size + buf_size;
     *dyn_size += buf_size;

     prev_is_key = !(i % key_frame_period);

   }

   if (verbose)
     printf ("=======Encode Finished==============\n");

   display_encode_status (total_size);
}

static void
ValidateInput ()
{
  if (enc_params.num_sl > 3) {
    printf ("Fixme: fake-enc only supports upto 3 spatail layers, exiting\n");
    exit (0);
  }
  if (enc_params.num_tl > 3) {
    printf ("Fixme: fake-enc only supports upto 3 temporal layers, exiting\n");
    exit (0);
  }
}

void
get_codec_and_algo_id (CodecID id, int *codec_id, int *algo_id)
{
  switch (id)
  {
    case VP8_ID:
      *codec_id = LIBMEBO_CODEC_VP8;
      *algo_id = LIBMEBO_BRC_ALGORITHM_DERIVED_LIBVPX_VP8;
      break;
    case VP9_ID:
      *codec_id = LIBMEBO_CODEC_VP9;
      *algo_id = LIBMEBO_BRC_ALGORITHM_DERIVED_LIBVPX_VP9;
      break;
    case AV1_ID:
      *codec_id = LIBMEBO_CODEC_VP8;
      *algo_id = LIBMEBO_BRC_ALGORITHM_DERIVED_AOM_AV1;
      break;
    default:
      *codec_id = LIBMEBO_CODEC_UNKNOWN;
      *algo_id = LIBMEBO_BRC_ALGORITHM_UNKNOWN;
  }
}

int main (int argc,char **argv)
{
  int codec_type, algo_id;
  if (argc < 3) {
    show_help();
    return -1;
  }
  parse_args(argc, (char **)argv);

  ValidateInput ();

  //Init layered bitrate allocation estimation
  InitLayeredBitrateAlloc (enc_params.num_sl, enc_params.num_tl, enc_params.bitrate);

  //Create the rate-controller
  get_codec_and_algo_id (enc_params.id, &codec_type, &algo_id);

  libmebo_rc = libmebo_rate_controller_new (codec_type, algo_id);
  if (!libmebo_rc) {
    printf ("Failed to create the rate-controller \n");
    return -1;
  }

  if (!libmebo_software_brc_init (libmebo_rc, &libmebo_rc_config)) {
    printf ("Failed to init brc: \n");
    return -1;
  }

  start_virtual_encode (libmebo_rc);

  libmebo_rate_controller_free (libmebo_rc);

  return 0;
}
