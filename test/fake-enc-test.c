/*  Copyright (c) 2020 Intel Corporation. All Rights Reserved.
 *  Copyright (c) 2020 Sreerenj Balachandran <sreerenj.balachandran@intel.com>
 *  Author: Sreerenj Balachandran <sreerenj.balachandran@intel.com>
 *
 * sample command:
    ./fake-enc --codec=VP9 --preset=0 --framecount=120
*/

#include <stdio.h>
#include <string.h>
#include <libmebo.h>
#include <getopt.h>
#include <stdlib.h>
#include <time.h>

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
};

struct BitrateBounds {
  unsigned int lower; //in kbps
  unsigned int upper; //in kbps
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
  };

static struct EncParams preset_list [] = {
	{0, 1, 256, 30, 320, 240, 100},
	{1, 1, 512, 30, 320, 240, 100},
	{2, 1, 1024, 30, 320, 240, 100},
	{3, 1, 256, 30, 640, 480, 100},
	{4, 1, 512, 30, 640, 480, 100},
	{5, 1, 1024, 30, 640, 480, 100},
	{6, 1, 1024, 30, 1280, 720, 100},
	{7, 1, 2048, 30, 1280, 720, 100},
	{8, 1, 4096, 30, 1280, 720, 100},
	{9, 1, 1024, 30, 1920, 1080, 100},
	{10, 1, 4096, 30, 1920, 1080, 100},
	{11, 1, 8192, 30, 1920, 1080, 100},
};

//heuristics to predict a decent key/inter-frame size
static struct BitrateBounds bitrate_bounds_intra [] = {
	{3500, 4440}, {3700, 5600}, {5500,9500}, //qvga-intra-frames
	{4100, 7400}, {3700, 11150}, {10100,16100}, //vga-intra-frames
	{16000, 25600}, {27600,35800}, {30100,63100}, //hd-intra-frames
	{14400,30000}, {60000,75100}, {65400,126600}, //full_hd-intra-frames
};

//heuristics to predict a decent intra-frame size
static struct BitrateBounds bitrate_bounds_inter [] = {
	{800, 1170}, {1700, 2200}, {3000, 5000}, //qvga-inter-frames
	{900, 1200}, {1800, 2200}, {3600, 4500}, //vga-inter-frames
	{3000, 4500}, {7100, 8400}, {14700,17500}, //hd-inter-frames
	{3100,8500}, {14100,16500}, {30000,35600}, //full_hd-inter-frames
};

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
		  "[--preset= 1 to 10] \n"
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
		 "\n");
}

static void
parse_args(int argc, char **argv)
{
  int c,option_index;

  static const struct option long_options[] = {
        {"help", no_argument, 0, 0},
        {"codec", required_argument, 0, 1},
        {"preset", required_argument, 0, 2},
        {"framecount", required_argument, 0, 3},
        {"verbose", required_argument, 0, 4},
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
          if (preset < 0 || preset > 11) {
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
      case 4:
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

  rc_config->ss_number_layers = 1;
  rc_config->scaling_factor_num[0] = 1;
  rc_config->scaling_factor_den[0] = 1;
  rc_config->max_quantizers[0] = rc_config->max_quantizer;
  rc_config->min_quantizers[0] = rc_config->min_quantizer;
  rc_config->layer_target_bitrate[0] = rc_config->target_bandwidth;

  // Temporal layer variables.
  rc_config->ts_number_layers = 1;
  rc_config->ts_rate_decimator[0] = 1;

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

  printf ("\n======= Encode Staus Report ======= \n");
  printf ("Bitrate of the Compressed stream = %d kbps\n",
		   (((bitstream_size / enc_params.framecount) *
		     enc_params.framerate)*8)/1000);
  printf ("\n");
}

static void
start_virtual_encode (LibMeboRateController *rc)
{
   int i, qp = 0;
   int key_frame_period = 30;
   uint32_t buf_size = 1024;
   uint32_t total_size = 0;
   uint32_t prev_qp = 0;
   uint32_t predicted_size = 0;
   uint32_t lower =0, upper=0;
   LibMeboFrameType libmebo_frame_type;
   LibMeboRCFrameParams rc_frame_params;
   unsigned int preset = enc_params.preset;

   if (verbose)
     printf ("=======Fake Encode starts ==============\n");

   srand(time(NULL));
   for (i = 0; i<enc_params.framecount; i++) {
     predicted_size = 0;
     lower =0;
     upper=0;

     if (i % key_frame_period == 0) {
       libmebo_frame_type = LIBMEBO_KEY_FRAME;
       lower = bitrate_bounds_intra [preset].lower;
       upper = bitrate_bounds_intra [preset].upper;
     }
     else {
       libmebo_frame_type = LIBMEBO_INTER_FRAME;
       lower = bitrate_bounds_inter [preset].lower;
       upper = bitrate_bounds_inter [preset].upper;
     }
     predicted_size = (rand() % (upper - lower)) + lower;

     rc_frame_params.frame_type = libmebo_frame_type;
     rc_frame_params.spatial_layer_id = 0;
     rc_frame_params.temporal_layer_id = 0;

     libmebo_rate_controller_compute_qp (rc, rc_frame_params);

     qp = libmebo_rate_controller_get_qp (libmebo_rc);
     if (verbose)
       printf ("QP = %d \n", qp);

     buf_size = predicted_size;

     //adjust 5% of the prediction size based on QP
     if (qp < (prev_qp + 10))
       buf_size += ((buf_size * 5)/100);
     else
       buf_size -= ((buf_size * 5)/100);

     prev_qp = qp;

     if (verbose)
       printf ("PostEncodeBufferSize = %d \n",buf_size);
     total_size += buf_size;

     libmebo_rate_controller_update_frame_size (rc, buf_size);
   }

   if (verbose)
     printf ("=======Encode Finished==============\n");

   display_encode_status (total_size);
}

int main (int argc,char **argv)
{
  if (argc < 3) {
    show_help();
    return -1;
  }
  parse_args(argc, (char **)argv);


  //Create the rate-controller
  libmebo_rc = libmebo_rate_controller_new (enc_params.id);
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
}
