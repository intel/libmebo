/*  Copyright (c) 2020 Intel Corporation. All Rights Reserved.
*  Copyright (c) 2020 Sreerenj Balachandran <sreerenj.balachandran@intel.com>
*  Author: Sreerenj Balachandran <sreerenj.balachandran@intel.com>
*
* sample command:
    ./fake-enc --codec=VP9 --preset=0 --framecount=120
*/

#include "../src/lib/libmebo.hpp"
#include <assert.h>
#include <getopt.h>
#include <iostream>
#include <memory>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../src/Handlers/AV1RateControlHandler.hpp"
#include "../src/Handlers/LibMeboControlHandler.hpp"
#include "../src/Handlers/VP8RateControlHandler.hpp"
#include "../src/Handlers/VP9RateControlHandler.hpp"
#include "../src/lib/RateControlFactory.hpp"

#define MaxSpatialLayers 3
#define MaxTemporalLayers 3

static LibMeboRateController *libMeboRc;
LibMeboRateControllerConfig libmebo_rc_config;

typedef enum CodecID {
  VP8_ID = 0,
  VP9_ID = 1,
  AV1_ID = 2,
} CodecID;

struct EncParams {
  unsigned int preset;
  unsigned int id;
  unsigned int bitrate; // in kbps
  unsigned int framerate;
  unsigned int width;
  unsigned int height;
  unsigned int framecount;          // Number of Frames to be encoded
  unsigned int numSpatLayers;              // Number of Spatial Layers
  unsigned int numTempoLayers;              // Number of Temporal Layers
  unsigned int dynamicRateChange; // dynamic rate change enablement flag
};

struct BitrateBounds {
  unsigned int lower; // in kbps
  unsigned int upper; // in kbps
};

struct SvcBitrateBounds {
  int layerBitrateLower[MaxSpatialLayers][MaxTemporalLayers];
  int layerBitrateUpper[MaxSpatialLayers][MaxTemporalLayers];
};

// Default config
static struct EncParams encParams = {
    .preset = 0,
    .id = VP9_ID,
    .bitrate = 1024,
    .framerate = 30,
    .width = 320,
    .height = 240,
    .framecount = 100,
    .numSpatLayers = 1,
    .numTempoLayers = 1,
    .dynamicRateChange = 0,
};

static struct EncParams presetList[] = {
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
    {11, 1, 8192, 30, 1920, 1080, 100, 1, 1, 0},
    {12, 1, 8192, 30, 1920, 1080, 100, 1, 1, 1},
    {13, 1, 4096, 30, 1280, 720, 100, 3, 2, 0},
};

// heuristics to predict a decent key/intra-frame size
static struct BitrateBounds bitrateBoundsIntra[] = {
    {3500, 4440},    {3700, 5600},   {5500, 9500},    // qvga-intra-frames
    {4100, 7400},    {3700, 11150},  {10100, 16100},  // vga-intra-frames
    {16000, 25600},  {27600, 35800}, {30100, 63100},  // hd-intra-frames
    {14400, 30000},  {60000, 75100}, {65400, 126600}, // full_hd-intra-frames
    {65400, 126600},                                  // full_hd-intra-frames
};

// heuristics to predict a decent inter-frame size
static struct BitrateBounds bitrateBoundsInter[] = {
    {800, 1170},    {1700, 2200},   {3000, 5000},   // qvga-inter-frames
    {900, 1200},    {1800, 2200},   {3600, 4500},   // vga-inter-frames
    {3000, 4500},   {7100, 8400},   {14700, 17500}, // hd-inter-frames
    {3100, 8500},   {14100, 16500}, {30000, 35600}, // full_hd-inter-frames
    {30000, 35600},                                 // full_hd-inter-frames
};

// heuristics to predict a decent key/intra-frame size for SVC
static struct SvcBitrateBounds svcBitrateBoundsIntra[] = {
    {.layerBitrateLower =
         {
             {5957, 5957, 0},
             {8007, 8007, 0},
             {17520, 17520, 0},
         },
     .layerBitrateUpper =
         {
             {9884, 9884, 0},
             {17241, 17241, 0},
             {19084, 19084, 0},
         }},
};

// heuristics to predict a decent inter-frame size for SVC
static struct SvcBitrateBounds svcBitrateBoundsInter[] = {
    {.layerBitrateLower =
         {
             {4520, 4520, 0},
             {4876, 4876, 0},
             {4327, 4327, 0},
         },
     .layerBitrateUpper =
         {
             {5700, 5700, 0},
             {5670, 5670, 0},
             {5689, 5689, 0},
         }},
};

int layeredBitrates[MaxSpatialLayers][MaxTemporalLayers];
int layeredFrameRate[MaxTemporalLayers];
int layeredStreamSize[MaxSpatialLayers][MaxTemporalLayers];
int layeredFrameCount[MaxSpatialLayers][MaxTemporalLayers];

#define LAYER_IDS_TO_IDX(sl, tl, numTempoLayers) ((sl) * (numTempoLayers) + (tl))

unsigned int dynamicSize[2] = {0, 0};
unsigned int dynamicBitrates[2] = {0, 0};

static int verbose = 0;

static char *getCodecIdString(CodecID id) {
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

static void show_help() {
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
         // Please add any non-SVC, non-dynamirc-rate-change
         //  presets here and increment the SVC_PRESET_START_INDEX
         //  and DYNAMIC_RATE_CHAGE_PRESET_START_INDEX
         "    Preset12: FULLHD_8192kbps_30fps_DynamicRateChange \n"
         // Please add any non-SVC presets here and
         // increment the SVC_PRESET_START_INDEX
         "    Preset13: SVC_HD_4096kbps_30fps_S3T2 \n"
         "\n");
}

#define DYNAMIC_RATE_CHAGE_PRESET_START_INDEX 12
#define SVC_PRESET_START_INDEX 13

static void parse_args(int argc, char **argv) {
  int c, option_index;

  static const struct option long_options[] = {
      {"help", no_argument, 0, 0},
      {"codec", required_argument, 0, 1},
      {"preset", required_argument, 0, 2},
      {"framecount", required_argument, 0, 3},
      {"temporal-layers", required_argument, 0, 4},
      {"spatial-layers", required_argument, 0, 5},
      {"dynamic-rate-change", required_argument, 0, 6},
      {"verbose", required_argument, 0, 7},
      {NULL, 0, NULL, 0}};

  while (1) {
    c = getopt_long_only(argc, argv, "hcb:?", long_options, &option_index);
    if (c == -1)
      break;

    switch (c) {
    case 1:
      if (!strcmp(optarg, "VP8"))
        encParams.id = VP8_ID;
      else if (!strcmp(optarg, "VP9"))
        encParams.id = VP9_ID;
      else
        encParams.id = AV1_ID;
      break;
    case 2: {
      int preset = atoi(optarg);
      CodecID id = static_cast<CodecID>(encParams.id);
      if (preset < 0 || preset > SVC_PRESET_START_INDEX) {
        printf("Unknown preset, Failed \n");
        exit(0);
      }
      encParams = presetList[preset];
      encParams.id = id;
    } break;
    case 3:
      encParams.framecount = atoi(optarg);
      break;
    case 7:
      verbose = atoi(optarg);
      break;
    default:
      break;
    }
  }
}

static void display_encode_status(unsigned int bitstream_size) {

  printf("======= Encoder Input Configuration ======= \n");
  printf("Codec      = %s \n"
         "bitrate    = %d kbps \n"
         "framerate  = %d \n"
         "width      = %d \n"
         "height     = %d \n"
         "framecount = %d \n",
         getCodecIdString(static_cast<CodecID>(encParams.id)),
         encParams.bitrate, encParams.framerate, encParams.width,
         encParams.height, encParams.framecount);
  if (encParams.numSpatLayers > 1 || encParams.numTempoLayers > 1) {
    printf("Target bitrates in kbps:\n");
    for (unsigned int sl = 0; sl < encParams.numSpatLayers; sl++) {
      int bitrate = 0;
      for (unsigned int tl = 0; tl < encParams.numTempoLayers; tl++) {
        bitrate += layeredBitrates[sl][tl];
        printf("S%dT%d target bitrate = %d \n", sl, tl,
               layeredBitrates[sl][tl]);
      }
      printf("SpatialLayer[%d] target bitrate: %d\n", sl, bitrate);
    }
  }

  printf("\n============ Encode Staus Report ============ \n\n");
  // SVC
  if (encParams.numSpatLayers > 1 || encParams.numTempoLayers > 1) {
    int totalBitrate = 0;
    for (unsigned int sl = 0; sl < encParams.numSpatLayers; sl++) {
      for (unsigned int tl = 0; tl < encParams.numTempoLayers; tl++) {
        int bitrate = 0;
        bitrate =
            (((layeredStreamSize[sl][tl] / layeredFrameCount[sl][tl]) *
              layeredFrameRate[tl]) *
             8) /
            1000;
        totalBitrate += bitrate;
        printf(
            "SpatialLayer[%d]TemporlLayer[%d]: \n"
            "  Bitrate (with out including any other layers)     = %d kbps \n"
            "  Bitrate (accumulated the lower layer targets)     = %d kbps \n",
            sl, tl, bitrate, totalBitrate);
      }
    }
  }

  // Dynamic Rate Chaange
  if (encParams.dynamicRateChange) {
    int frames_count = encParams.framecount / 2;
    printf("Bitrate used for the streams with %d target bitrate = %d kbps\n\n",
           dynamicBitrates[0],
           (((dynamicSize[0] / frames_count) * encParams.framerate) * 8) /
               1000);
    printf("Bitrate used for the streams with %d target bitrate = %d kbps\n\n",
           dynamicBitrates[1],
           (((dynamicSize[1] / frames_count) * encParams.framerate) * 8) /
               1000);
  }
  printf(
      "Bitrate of the Compressed stream = %d kbps\n\n",
      (((bitstream_size / encParams.framecount) * encParams.framerate) * 8) /
          1000);
  printf("\n");
}

static int _prev_temporal_id = 0;

static void get_layer_ids(int frame_count, int numSpatLayers, int numTempoLayers,
                          int *spatialId, int *temporalId) {
  int s_id = 0, t_id = 0;

  // spatial id
  s_id = frame_count % numSpatLayers;

  // Increment the temporalId only when all the spatial
  // variants are encoded for a particualr frame id
  if (!(frame_count % numSpatLayers)) {
    // temporal id
    if (numTempoLayers > 1) {
      switch (numTempoLayers) {
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
        printf("Exit: Not supporting more than 3 temporal layers \n");
        exit(0);
      }
    } else {
      t_id = 0;
    }
  } else {
    t_id = _prev_temporal_id;
  }

  *spatialId = s_id;
  *temporalId = t_id;
  _prev_temporal_id = t_id;
}

static void start_virtual_encode(std::unique_ptr<LibMeboBrc> &brc,
                                 LibMeboRateController *rc,
                                 LibMeboRateControllerConfig rc_config) {
  int i, qp = 0;
  int key_frame_period = 30;
  uint32_t buf_size = 1024;
  uint32_t total_size = 0;
  int prev_qp = 0;
  uint32_t predicted_size = 0;
  uint32_t lower = 0, upper = 0;
  LibMeboFrameType libmebo_frame_type;
  LibMeboRCFrameParams rc_frame_params;
  unsigned int preset = encParams.preset;
  unsigned int svc_preset = 0;
  int frame_count = 0;
  unsigned int prev_is_key = 0;
  assert(brc != nullptr);
  if (verbose)
    printf("=======Fake Encode starts ==============\n");

  if (encParams.numSpatLayers > 1 || encParams.numTempoLayers > 1)
    svc_preset = preset - SVC_PRESET_START_INDEX;

  key_frame_period *= encParams.numSpatLayers;

  // Account spatial svc in frame_count
  frame_count = encParams.framecount * encParams.numSpatLayers;
  srand(time(NULL));
  for (i = 0; i < frame_count; i++) {
    LibMeboStatus status;
    int spatialId;
    int temporalId;
    int update_rate = 0;
    unsigned int *dyn_size;
    predicted_size = 0;
    lower = 0;
    upper = 0;

    get_layer_ids(i, encParams.numSpatLayers, encParams.numTempoLayers, &spatialId,
                  &temporalId);

    // Dynamic rate reset: Reduce the framerate by 1/8 th
    if (encParams.dynamicRateChange && i >= frame_count / 2) {
      preset = 9; // bitrate change from 4096 to 1024
      dyn_size = &dynamicSize[1];
      update_rate = (i == frame_count / 2) ? 1 : 0;
    } else {
      dyn_size = &dynamicSize[0];
    }

    if (i % key_frame_period == 0) {
      libmebo_frame_type = LIBMEBO_KEY_FRAME;
      if (preset < SVC_PRESET_START_INDEX) {
        lower = bitrateBoundsIntra[preset].lower;
        upper = bitrateBoundsIntra[preset].upper;
      } else {
        // SVC
        lower = svcBitrateBoundsIntra[svc_preset]
                    .layerBitrateLower[spatialId][temporalId];
        upper = svcBitrateBoundsIntra[svc_preset]
                    .layerBitrateUpper[spatialId][temporalId];
      }
    } else {
      libmebo_frame_type = LIBMEBO_INTER_FRAME;
      if (preset < SVC_PRESET_START_INDEX) {
        lower = bitrateBoundsInter[preset].lower;
        upper = bitrateBoundsInter[preset].upper;
      } else {
        // SVC
        lower = svcBitrateBoundsInter[svc_preset]
                    .layerBitrateLower[spatialId][temporalId];
        upper = svcBitrateBoundsInter[svc_preset]
                    .layerBitrateUpper[spatialId][temporalId];
      }
    }
    predicted_size = (rand() % (upper - lower)) + lower;

    // Update libmebo rate control config
    if (update_rate) {
      dynamicBitrates[0] = libmebo_rc_config.target_bandwidth;
      libmebo_rc_config.target_bandwidth /= 8;
      dynamicBitrates[1] = libmebo_rc_config.target_bandwidth;
      status = brc->update_config(rc, &libmebo_rc_config);
      assert(status == LIBMEBO_STATUS_SUCCESS);
    }

    rc_frame_params.frame_type = libmebo_frame_type;

    // Set spatial layer id
    rc_frame_params.spatial_layer_id = spatialId;
    rc_frame_params.temporal_layer_id = temporalId;
    status = brc->compute_qp(rc, &rc_frame_params);
    assert(status == LIBMEBO_STATUS_SUCCESS);
    status = brc->get_qp(rc, &qp);
    assert(status == LIBMEBO_STATUS_SUCCESS);

    printf("QP = %d \n", qp);

    buf_size = predicted_size;

    // Heuristics to calculate a reasonable value of the
    // compressed frame size
    if (i != 0 && !prev_is_key) {
      int qp_val = (qp == 0) ? 1 : qp;
      int size_range = upper - lower;
      int qp_range_length = size_range / 256;
      int suggested_size = lower + (qp_val * qp_range_length);
      int new_predicted_size = 0;

      if ((qp) < prev_qp)
        new_predicted_size =
            (rand() % (upper - suggested_size)) + suggested_size;
      else
        new_predicted_size = (rand() % (suggested_size - lower)) + lower;

      buf_size = new_predicted_size;
    }

    prev_qp = qp;

    if (verbose)
      printf("PostEncodeBufferSize = %d \n", buf_size);
    status = brc->post_encode_update(rc, buf_size);
    assert(status == LIBMEBO_STATUS_SUCCESS);

    // Calculate per layer stream size
    if (encParams.numTempoLayers > 1 || encParams.numSpatLayers > 1) {
      layeredStreamSize[spatialId][temporalId] += buf_size;
      layeredFrameCount[spatialId][temporalId] += 1;
      if (verbose) {
        printf("PostEncodeBufferSize of "
               "spatial_layer[%d],temporal_layer[%d]  = %d \n",
               0, rc_frame_params.temporal_layer_id, buf_size);
      }
    }
    total_size = total_size + buf_size;
    *dyn_size += buf_size;

    prev_is_key = !(i % key_frame_period);
  }

  if (verbose)
    printf("=======Encode Finished==============\n");

  display_encode_status(total_size);
}

static void ValidateInput() {
  if (encParams.numSpatLayers > 3) {
    printf("Fixme: fake-enc only supports upto 3 spatail layers, exiting\n");
    exit(0);
  }
  if (encParams.numTempoLayers > 3) {
    printf("Fixme: fake-enc only supports upto 3 temporal layers, exiting\n");
    exit(0);
  }
}

LibMeboRateController *rc1;
int main(int argc, char **argv) {
  int codec_type, algo_id;
  if (argc < 3) {
    show_help();
    return -1;
  }
  parse_args(argc, (char **)argv);
  ValidateInput();
  printf("started in fake test main\n");
  std::unique_ptr<LibMeboBrc> brc = Libmebo_brc_factory::create(
      static_cast<LibMeboBrcAlgorithmID>(encParams.id));
  printf("started in fake test main - 1\n");
  if (brc != nullptr) {
    printf("started in fake test main - 2\n");
    LibMeboStatus status = LIBMEBO_STATUS_SUCCESS;
    libMeboRc = (LibMeboRateController *)malloc(sizeof(LibMeboRateController));
    if (!libMeboRc) {
      fprintf(stderr, "Failed allocation for LibMeboRateController \n");
      return NULL;
    }
    printf("started in fake test main - 3\n");
    rc1 = brc->init(libMeboRc, &libmebo_rc_config);
    start_virtual_encode(brc, libMeboRc, libmebo_rc_config);
  }
  free(libMeboRc);
  return 0;
}