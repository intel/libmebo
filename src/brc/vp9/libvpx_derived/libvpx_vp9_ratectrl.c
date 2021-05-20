/*
 *  Copyright (c) 2020 The WebM project authors. All Rights Reserved.
 *  Copyright (c) 2020 Intel Corporation. All Rights Reserved.
 *  Copyright (c) 2020 Sreerenj Balachandran <sreerenj.balachandran@intel.com>
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "libvpx_vp9_ratectrl.h"
#include "libvpx_vp9_svc_layercontext.h"
#include "libvpx_vp9_common.h"

#define ASSIGN_MINQ_TABLE(bit_depth, name)       \
  do {                                           \
    switch (bit_depth) {                         \
      case VPX_BITS_8: name = name##_8; break;   \
      case VPX_BITS_10: name = name##_10; break; \
      default:                                   \
        assert(bit_depth == VPX_BITS_12);        \
        name = name##_12;                        \
        break;                                   \
    }                                            \
  } while (0)

/* clang-format off */
static const Vp9LevelSpec vp9_level_defs[VP9_LEVELS] = {
  //         sample rate    size   breadth  bitrate  cpb
  { LEVEL_1,   829440,      36864,    512,   200,    400,    2, 1,  4,  8 },
  { LEVEL_1_1, 2764800,     73728,    768,   800,    1000,   2, 1,  4,  8 },
  { LEVEL_2,   4608000,     122880,   960,   1800,   1500,   2, 1,  4,  8 },
  { LEVEL_2_1, 9216000,     245760,   1344,  3600,   2800,   2, 2,  4,  8 },
  { LEVEL_3,   20736000,    552960,   2048,  7200,   6000,   2, 4,  4,  8 },
  { LEVEL_3_1, 36864000,    983040,   2752,  12000,  10000,  2, 4,  4,  8 },
  { LEVEL_4,   83558400,    2228224,  4160,  18000,  16000,  4, 4,  4,  8 },
  { LEVEL_4_1, 160432128,   2228224,  4160,  30000,  18000,  4, 4,  5,  6 },
  { LEVEL_5,   311951360,   8912896,  8384,  60000,  36000,  6, 8,  6,  4 },
  { LEVEL_5_1, 588251136,   8912896,  8384,  120000, 46000,  8, 8,  10, 4 },
  // TODO(huisu): update max_cpb_size for level 5_2 ~ 6_2 when
  // they are finalized (currently tentative).
  { LEVEL_5_2, 1176502272,  8912896,  8384,  180000, 90000,  8, 8,  10, 4 },
  { LEVEL_6,   1176502272,  35651584, 16832, 180000, 90000,  8, 16, 10, 4 },
  { LEVEL_6_1, 2353004544u, 35651584, 16832, 240000, 180000, 8, 16, 10, 4 },
  { LEVEL_6_2, 4706009088u, 35651584, 16832, 480000, 360000, 8, 16, 10, 4 },
};

static const int16_t ac_qlookup[QINDEX_RANGE] = {
  4, 8, 9, 10, 11, 12, 13, 14,
  15, 16, 17, 18, 19, 20, 21, 22,
  23, 24, 25, 26, 27, 28, 29, 30,
  31, 32, 33, 34, 35, 36, 37, 38,
  39, 40, 41, 42, 43, 44, 45, 46,
  47, 48, 49, 50, 51, 52, 53, 54,
  55, 56, 57, 58, 59, 60, 61, 62,
  63, 64, 65, 66, 67, 68, 69, 70,
  71, 72, 73, 74, 75, 76, 77, 78,
  79, 80, 81, 82, 83, 84, 85, 86,
  87, 88, 89, 90, 91, 92, 93, 94,
  95, 96, 97, 98, 99, 100, 101, 102,
  104, 106, 108, 110, 112, 114, 116, 118,
  120, 122, 124, 126, 128, 130, 132, 134,
  136, 138, 140, 142, 144, 146, 148, 150,
  152, 155, 158, 161, 164, 167, 170, 173,
  176, 179, 182, 185, 188, 191, 194, 197,
  200, 203, 207, 211, 215, 219, 223, 227,
  231, 235, 239, 243, 247, 251, 255, 260,
  265, 270, 275, 280, 285, 290, 295, 300,
  305, 311, 317, 323, 329, 335, 341, 347,
  353, 359, 366, 373, 380, 387, 394, 401,
  408, 416, 424, 432, 440, 448, 456, 465,
  474, 483, 492, 501, 510, 520, 530, 540,
  550, 560, 571, 582, 593, 604, 615, 627,
  639, 651, 663, 676, 689, 702, 715, 729,
  743, 757, 771, 786, 801, 816, 832, 848,
  864, 881, 898, 915, 933, 951, 969, 988,
  1007, 1026, 1046, 1066, 1087, 1108, 1129, 1151,
  1173, 1196, 1219, 1243, 1267, 1292, 1317, 1343,
  1369, 1396, 1423, 1451, 1479, 1508, 1537, 1567,
  1597, 1628, 1660, 1692, 1725, 1759, 1793, 1828,
};

static const int16_t ac_qlookup_10[QINDEX_RANGE] = {
  4, 9, 11, 13, 16, 18, 21, 24,
  27, 30, 33, 37, 40, 44, 48, 51,
  55, 59, 63, 67, 71, 75, 79, 83,
  88, 92, 96, 100, 105, 109, 114, 118,
  122, 127, 131, 136, 140, 145, 149, 154,
  158, 163, 168, 172, 177, 181, 186, 190,
  195, 199, 204, 208, 213, 217, 222, 226,
  231, 235, 240, 244, 249, 253, 258, 262,
  267, 271, 275, 280, 284, 289, 293, 297,
  302, 306, 311, 315, 319, 324, 328, 332,
  337, 341, 345, 349, 354, 358, 362, 367,
  371, 375, 379, 384, 388, 392, 396, 401,
  409, 417, 425, 433, 441, 449, 458, 466,
  474, 482, 490, 498, 506, 514, 523, 531,
  539, 547, 555, 563, 571, 579, 588, 596,
  604, 616, 628, 640, 652, 664, 676, 688,
  700, 713, 725, 737, 749, 761, 773, 785,
  797, 809, 825, 841, 857, 873, 889, 905,
  922, 938, 954, 970, 986, 1002, 1018, 1038,
  1058, 1078, 1098, 1118, 1138, 1158, 1178, 1198,
  1218, 1242, 1266, 1290, 1314, 1338, 1362, 1386,
  1411, 1435, 1463, 1491, 1519, 1547, 1575, 1603,
  1631, 1663, 1695, 1727, 1759, 1791, 1823, 1859,
  1895, 1931, 1967, 2003, 2039, 2079, 2119, 2159,
  2199, 2239, 2283, 2327, 2371, 2415, 2459, 2507,
  2555, 2603, 2651, 2703, 2755, 2807, 2859, 2915,
  2971, 3027, 3083, 3143, 3203, 3263, 3327, 3391,
  3455, 3523, 3591, 3659, 3731, 3803, 3876, 3952,
  4028, 4104, 4184, 4264, 4348, 4432, 4516, 4604,
  4692, 4784, 4876, 4972, 5068, 5168, 5268, 5372,
  5476, 5584, 5692, 5804, 5916, 6032, 6148, 6268,
  6388, 6512, 6640, 6768, 6900, 7036, 7172, 7312,
};

static const int16_t ac_qlookup_12[QINDEX_RANGE] = {
  4, 13, 19, 27, 35, 44, 54, 64,
  75, 87, 99, 112, 126, 139, 154, 168,
  183, 199, 214, 230, 247, 263, 280, 297,
  314, 331, 349, 366, 384, 402, 420, 438,
  456, 475, 493, 511, 530, 548, 567, 586,
  604, 623, 642, 660, 679, 698, 716, 735,
  753, 772, 791, 809, 828, 846, 865, 884,
  902, 920, 939, 957, 976, 994, 1012, 1030,
  1049, 1067, 1085, 1103, 1121, 1139, 1157, 1175,
  1193, 1211, 1229, 1246, 1264, 1282, 1299, 1317,
  1335, 1352, 1370, 1387, 1405, 1422, 1440, 1457,
  1474, 1491, 1509, 1526, 1543, 1560, 1577, 1595,
  1627, 1660, 1693, 1725, 1758, 1791, 1824, 1856,
  1889, 1922, 1954, 1987, 2020, 2052, 2085, 2118,
  2150, 2183, 2216, 2248, 2281, 2313, 2346, 2378,
  2411, 2459, 2508, 2556, 2605, 2653, 2701, 2750,
  2798, 2847, 2895, 2943, 2992, 3040, 3088, 3137,
  3185, 3234, 3298, 3362, 3426, 3491, 3555, 3619,
  3684, 3748, 3812, 3876, 3941, 4005, 4069, 4149,
  4230, 4310, 4390, 4470, 4550, 4631, 4711, 4791,
  4871, 4967, 5064, 5160, 5256, 5352, 5448, 5544,
  5641, 5737, 5849, 5961, 6073, 6185, 6297, 6410,
  6522, 6650, 6778, 6906, 7034, 7162, 7290, 7435,
  7579, 7723, 7867, 8011, 8155, 8315, 8475, 8635,
  8795, 8956, 9132, 9308, 9484, 9660, 9836, 10028,
  10220, 10412, 10604, 10812, 11020, 11228, 11437, 11661,
  11885, 12109, 12333, 12573, 12813, 13053, 13309, 13565,
  13821, 14093, 14365, 14637, 14925, 15213, 15502, 15806,
  16110, 16414, 16734, 17054, 17390, 17726, 18062, 18414,
  18766, 19134, 19502, 19886, 20270, 20670, 21070, 21486,
  21902, 22334, 22766, 23214, 23662, 24126, 24590, 25070,
  25551, 26047, 26559, 27071, 27599, 28143, 28687, 29247,
};

static int vp9_compute_qdelta(const RATE_CONTROL *rc,
    double qstart, double qtarget, vpx_bit_depth_t bit_depth);

static int
clamp (int value, int low, int high)
{
  return value < low ? low : (value > high ? high : value);
}

int16_t
brc_libvpx_vp9_ac_quant (int qindex, int delta, int bit_depth)
{
  const uint8_t q_table_idx = clamp (qindex + delta, 0, MAXQ);

  switch (bit_depth) {
    case 8:
      return ac_qlookup[q_table_idx];
    case 10:
      return ac_qlookup_10[q_table_idx];
    case 12:
      return ac_qlookup_12[q_table_idx];
    default:
      return -1;
  }
  return -1;
}

#define INLINE inline
int brc_libvpx_vp9_frame_is_intra_only(const VP9_COMMON *const cm) {
  return cm->frame_type == KEY_FRAME || cm->intra_only;
}   
    
double fclamp(double value, double low, double high) {
  return value < low ? low : (value > high ? high : value);
}   

int brc_libvpx_is_one_pass_cbr_svc(const struct VP9_COMP *const cpi) {
  return (cpi->use_svc && cpi->oxcf.pass == 0);
}

int vp9_get_level_index(VP9_LEVEL level) {
  int i;
  for (i = 0; i < VP9_LEVELS; ++i) {
    if (level == vp9_level_defs[i].level) return i;
  }
  return -1;
}

#define MI_SIZE_LOG2 3
#define MI_BLOCK_SIZE_LOG2 (6 - MI_SIZE_LOG2)  // 64 = 2^6

#define MI_SIZE (1 << MI_SIZE_LOG2)              // pixels per mi-unit
#define MI_BLOCK_SIZE (1 << MI_BLOCK_SIZE_LOG2)  // mi-units per max block
  
#define MI_MASK (MI_BLOCK_SIZE - 1)

#define ALIGN_POWER_OF_TWO(value, n) \
  (((value) + ((1 << (n)) - 1)) & ~((1 << (n)) - 1))

static INLINE int calc_mi_size(int len) {
  // len is in mi units.
  return len + MI_BLOCK_SIZE;
}

static void vp9_set_mi_size(int *mi_rows, int *mi_cols, int *mi_stride, int width,
                     int height) {
  const int aligned_width = ALIGN_POWER_OF_TWO(width, MI_SIZE_LOG2);
  const int aligned_height = ALIGN_POWER_OF_TWO(height, MI_SIZE_LOG2);
  *mi_cols = aligned_width >> MI_SIZE_LOG2;
  *mi_rows = aligned_height >> MI_SIZE_LOG2;
  *mi_stride = calc_mi_size(*mi_cols);
}

static void vp9_set_mb_size(int *mb_rows, int *mb_cols, int *mb_num, int mi_rows,
                     int mi_cols) {
  *mb_cols = (mi_cols + 1) >> 1;
  *mb_rows = (mi_rows + 1) >> 1;
  *mb_num = (*mb_rows) * (*mb_cols);
} 

void brc_libvpx_vp9_set_mb_mi(VP9_COMMON *cm, int width, int height) {
  vp9_set_mi_size(&cm->mi_rows, &cm->mi_cols, &cm->mi_stride, width, height);
  vp9_set_mb_size(&cm->mb_rows, &cm->mb_cols, &cm->MBs, cm->mi_rows,
                  cm->mi_cols); 
}

// Table that converts 0-63 Q-range values passed in outside to the Qindex
// range used internally.
static const int quantizer_to_qindex[] = {
  0,   4,   8,   12,  16,  20,  24,  28,  32,  36,  40,  44,  48,
  52,  56,  60,  64,  68,  72,  76,  80,  84,  88,  92,  96,  100,
  104, 108, 112, 116, 120, 124, 128, 132, 136, 140, 144, 148, 152,
  156, 160, 164, 168, 172, 176, 180, 184, 188, 192, 196, 200, 204,
  208, 212, 216, 220, 224, 228, 232, 236, 240, 244, 249, 255,
};    
  
int brc_libvpx_vp9_quantizer_to_qindex(int quantizer) {
  return quantizer_to_qindex[quantizer];
} 

// Tables relating active max Q to active min Q
static int kf_low_motion_minq_8[QINDEX_RANGE];
static int kf_high_motion_minq_8[QINDEX_RANGE];
static int arfgf_low_motion_minq_8[QINDEX_RANGE];
static int arfgf_high_motion_minq_8[QINDEX_RANGE];
static int inter_minq_8[QINDEX_RANGE];
static int rtc_minq_8[QINDEX_RANGE];

static int kf_low_motion_minq_10[QINDEX_RANGE];
static int kf_high_motion_minq_10[QINDEX_RANGE];
static int arfgf_low_motion_minq_10[QINDEX_RANGE];
static int arfgf_high_motion_minq_10[QINDEX_RANGE];
static int inter_minq_10[QINDEX_RANGE];
static int rtc_minq_10[QINDEX_RANGE];
static int kf_low_motion_minq_12[QINDEX_RANGE];
static int kf_high_motion_minq_12[QINDEX_RANGE];
static int arfgf_low_motion_minq_12[QINDEX_RANGE];
static int arfgf_high_motion_minq_12[QINDEX_RANGE];
static int inter_minq_12[QINDEX_RANGE];
static int rtc_minq_12[QINDEX_RANGE];

static int kf_high = 4800;
static int kf_low = 300;

#define ROUND_POWER_OF_TWO(value, n) (((value) + (1 << ((n)-1))) >> (n))
#define ROUND64_POWER_OF_TWO(value, n) (((value) + (1ULL << ((n)-1))) >> (n))

void brc_libvpx_vp9_set_rc_buffer_sizes(RATE_CONTROL *rc,
                                const VP9EncoderConfig *oxcf) {
  const int64_t bandwidth = oxcf->target_bandwidth;
  const int64_t starting = oxcf->starting_buffer_level_ms;
  const int64_t optimal = oxcf->optimal_buffer_level_ms;
  const int64_t maximum = oxcf->maximum_buffer_size_ms;
 
  rc->starting_buffer_level = starting * bandwidth / 1000;
  rc->optimal_buffer_level =
      (optimal == 0) ? bandwidth / 8 : optimal * bandwidth / 1000;
  rc->maximum_buffer_size =
      (maximum == 0) ? bandwidth / 8 : maximum * bandwidth / 1000;

  // Under a configuration change, where maximum_buffer_size may change,
  // keep buffer level clipped to the maximum allowed buffer size.
  rc->bits_off_target = VPXMIN(rc->bits_off_target, rc->maximum_buffer_size);
  rc->buffer_level = VPXMIN(rc->buffer_level, rc->maximum_buffer_size);
}

void brc_libvpx_vp9_check_reset_rc_flag(VP9_COMP *cpi) {
  RATE_CONTROL *rc = &cpi->rc;

  if (cpi->common.current_video_frame >
      (unsigned int)cpi->svc.number_spatial_layers) {
    if (cpi->use_svc) {
      vp9_svc_check_reset_layer_rc_flag(cpi);
    } else {

      if (rc->avg_frame_bandwidth > (3 * rc->last_avg_frame_bandwidth >> 1) ||
          rc->avg_frame_bandwidth < (rc->last_avg_frame_bandwidth >> 1)) {
        rc->rc_1_frame = 0;
        rc->rc_2_frame = 0;
        rc->bits_off_target = rc->optimal_buffer_level;
        rc->buffer_level = rc->optimal_buffer_level;
      }

    }
  }
}

void brc_libvpx_vp9_change_config(struct VP9_COMP *cpi, const VP9EncoderConfig *oxcf) {
  VP9_COMMON *const cm = &cpi->common;
  RATE_CONTROL *const rc = &cpi->rc;
  int last_w = cpi->oxcf.width;
  int last_h = cpi->oxcf.height;

  cpi->refresh_golden_frame = 0;
  cpi->refresh_last_frame = 1;

  cpi->target_level = oxcf->target_level;
  //vp9_set_level_constraint(&cpi->level_constraint,
  //                     vp9_get_level_index(cpi->target_level));

  rc->baseline_gf_interval = (MIN_GF_INTERVAL + MAX_GF_INTERVAL) / 2;

  brc_libvpx_vp9_set_rc_buffer_sizes(rc, &cpi->oxcf);

    // Under a configuration change, where maximum_buffer_size may change,
  // keep buffer level clipped to the maximum allowed buffer size.
  rc->bits_off_target = VPXMIN(rc->bits_off_target, rc->maximum_buffer_size);
  rc->buffer_level = VPXMIN(rc->buffer_level, rc->maximum_buffer_size);

  // Set up frame rate and related parameters rate control values.
  brc_libvpx_vp9_new_framerate(cpi, cpi->framerate);

  // Set absolute upper and lower quality limits
  rc->worst_quality = cpi->oxcf.worst_allowed_q;
  rc->best_quality = cpi->oxcf.best_allowed_q;

  if (last_w != cpi->oxcf.width || last_h != cpi->oxcf.height) {
    cm->width = cpi->oxcf.width;
    cm->height = cpi->oxcf.height;
    cpi->external_resize = 1;
  }

  if (last_w != cpi->oxcf.width || last_h != cpi->oxcf.height) {
    rc->rc_1_frame = 0;
    rc->rc_2_frame = 0;
  }

  if ((cpi->svc.number_temporal_layers > 1 && cpi->oxcf.rc_mode == VPX_CBR) ||
      ((cpi->svc.number_temporal_layers > 1 ||
        cpi->svc.number_spatial_layers > 1) &&
       cpi->oxcf.pass != 1)) {
    vp9_update_layer_context_change_config(cpi,
                                           (int)cpi->oxcf.target_bandwidth);
  }

  brc_libvpx_vp9_check_reset_rc_flag(cpi);
}

// These functions use formulaic calculations to make playing with the
// quantizer tables easier. If necessary they can be replaced by lookup
// tables if and when things settle down in the experimental bitstream
static double vp9_convert_qindex_to_q(int qindex, vpx_bit_depth_t bit_depth) {
// Convert the index to a real Q value (scaled down to match old Q values)
  switch (bit_depth) {
    case VPX_BITS_8: return brc_libvpx_vp9_ac_quant(qindex, 0, bit_depth) / 4.0;
    case VPX_BITS_10: return brc_libvpx_vp9_ac_quant(qindex, 0, bit_depth) / 16.0;
    default:
      //assert(bit_depth == VPX_BITS_12);
      return brc_libvpx_vp9_ac_quant(qindex, 0, bit_depth) / 64.0;
  }
}

// Functions to compute the active minq lookup table entries based on a
// formulaic approach to facilitate easier adjustment of the Q tables.
// The formulae were derived from computing a 3rd order polynomial best
// fit to the original data (after plotting real maxq vs minq (not q index))
static int get_minq_index(double maxq, double x3, double x2, double x1,
                          vpx_bit_depth_t bit_depth) {
  int i;
  const double minqtarget = VPXMIN(((x3 * maxq + x2) * maxq + x1) * maxq, maxq);

  // Special case handling to deal with the step from q2.0
  // down to lossless mode represented by q 1.0.
  if (minqtarget <= 2.0) return 0;

  for (i = 0; i < QINDEX_RANGE; i++) {
    if (minqtarget <= vp9_convert_qindex_to_q(i, bit_depth)) return i;
  }

  return QINDEX_RANGE - 1;
}

static void init_minq_luts(int *kf_low_m, int *kf_high_m, int *arfgf_low,
                           int *arfgf_high, int *inter, int *rtc,
                           vpx_bit_depth_t bit_depth) {
  int i;
  for (i = 0; i < QINDEX_RANGE; i++) {
    const double maxq = vp9_convert_qindex_to_q(i, bit_depth);
    kf_low_m[i] = get_minq_index(maxq, 0.000001, -0.0004, 0.150, bit_depth);
    kf_high_m[i] = get_minq_index(maxq, 0.0000021, -0.00125, 0.45, bit_depth);
    arfgf_low[i] = get_minq_index(maxq, 0.0000015, -0.0009, 0.30, bit_depth);
    inter[i] = get_minq_index(maxq, 0.00000271, -0.00113, 0.70, bit_depth);
    arfgf_high[i] = get_minq_index(maxq, 0.0000021, -0.00125, 0.55, bit_depth);
    rtc[i] = get_minq_index(maxq, 0.00000271, -0.00113, 0.70, bit_depth);
  }
}

void brc_libvpx_vp9_rc_init_minq_luts(void) {
  init_minq_luts(kf_low_motion_minq_8, kf_high_motion_minq_8,
                 arfgf_low_motion_minq_8, arfgf_high_motion_minq_8,
                 inter_minq_8, rtc_minq_8, VPX_BITS_8);
  init_minq_luts(kf_low_motion_minq_10, kf_high_motion_minq_10,
                 arfgf_low_motion_minq_10, arfgf_high_motion_minq_10,
                 inter_minq_10, rtc_minq_10, VPX_BITS_10);
  init_minq_luts(kf_low_motion_minq_12, kf_high_motion_minq_12,
                 arfgf_low_motion_minq_12, arfgf_high_motion_minq_12,
                 inter_minq_12, rtc_minq_12, VPX_BITS_12);
}

static int vp9_rc_bits_per_mb(FRAME_TYPE frame_type, int qindex,
                       double correction_factor, vpx_bit_depth_t bit_depth) {
  const double q = vp9_convert_qindex_to_q(qindex, bit_depth);
  int enumerator = frame_type == KEY_FRAME ? 2700000 : 1800000;

  assert(correction_factor <= MAX_BPB_FACTOR &&
         correction_factor >= MIN_BPB_FACTOR);

  // q based adjustment to baseline enumerator
  enumerator += (int)(enumerator * q) >> 12;
  return (int)(enumerator * correction_factor / q);
}

static int vp9_estimate_bits_at_q(FRAME_TYPE frame_type, int q, int mbs,
                           double correction_factor,
                           vpx_bit_depth_t bit_depth) {
  const int bpm =
      (int)(vp9_rc_bits_per_mb(frame_type, q, correction_factor, bit_depth));
  return VPXMAX(FRAME_OVERHEAD_BITS,
                (int)(((int64_t)bpm * mbs) >> BPER_MB_NORMBITS));
}

static int vp9_rc_clamp_iframe_target_size(const VP9_COMP *const cpi, int target) {
  const RATE_CONTROL *rc = &cpi->rc;
  const VP9EncoderConfig *oxcf = &cpi->oxcf;
  if (oxcf->rc_max_intra_bitrate_pct) {
    const int max_rate =
        rc->avg_frame_bandwidth * oxcf->rc_max_intra_bitrate_pct / 100;
    target = VPXMIN(target, max_rate);
  }
  if (target > rc->max_frame_bandwidth) target = rc->max_frame_bandwidth;
  return target;
}

// TODO(marpan/jianj): bits_off_target and buffer_level are used in the saame
// way for CBR mode, for the buffering updates below. Look into removing one
// of these (i.e., bits_off_target).
// Update the buffer level before encoding with the per-frame-bandwidth,
void brc_libvpx_update_buffer_level_preencode(VP9_COMP *cpi) {
  RATE_CONTROL *const rc = &cpi->rc;
  rc->bits_off_target += rc->avg_frame_bandwidth;
  // Clip the buffer level to the maximum specified buffer size.
  rc->bits_off_target = VPXMIN(rc->bits_off_target, rc->maximum_buffer_size);
  rc->buffer_level = rc->bits_off_target;
}

// Update the buffer level before encoding with the per-frame-bandwidth
// for SVC. The current and all upper temporal layers are updated, needed
// for the layered rate control which involves cumulative buffer levels for
// the temporal layers. Allow for using the timestamp(pts) delta for the
// framerate when the set_ref_frame_config is used.
static void update_buffer_level_svc_preencode(VP9_COMP *cpi) {
  SVC *const svc = &cpi->svc;
  int i;
  for (i = svc->temporal_layer_id; i < svc->number_temporal_layers; ++i) {
    const int layer =
        LAYER_IDS_TO_IDX(svc->spatial_layer_id, i, svc->number_temporal_layers);
    LAYER_CONTEXT *const lc = &svc->layer_context[layer];
    RATE_CONTROL *const lrc = &lc->rc;
    lrc->bits_off_target += (int)(lc->target_bandwidth / lc->framerate);
    // Clip buffer level to maximum buffer size for the layer.
    lrc->bits_off_target =
        VPXMIN(lrc->bits_off_target, lrc->maximum_buffer_size);
    lrc->buffer_level = lrc->bits_off_target;
    if (i == svc->temporal_layer_id) {
      cpi->rc.bits_off_target = lrc->bits_off_target;
      cpi->rc.buffer_level = lrc->buffer_level;
    }
  }
}

// Update the buffer level for higher temporal layers, given the encoded current
// temporal layer.
static void update_layer_buffer_level_postencode(SVC *svc,
                                                 int encoded_frame_size) {
  int i = 0;
  const int current_temporal_layer = svc->temporal_layer_id;
  for (i = current_temporal_layer + 1; i < svc->number_temporal_layers; ++i) {
    const int layer =
        LAYER_IDS_TO_IDX(svc->spatial_layer_id, i, svc->number_temporal_layers);
    LAYER_CONTEXT *lc = &svc->layer_context[layer];
    RATE_CONTROL *lrc = &lc->rc;
    lrc->bits_off_target -= encoded_frame_size;
    // Clip buffer level to maximum buffer size for the layer.
    lrc->bits_off_target =
        VPXMIN(lrc->bits_off_target, lrc->maximum_buffer_size);
    lrc->buffer_level = lrc->bits_off_target;
  }
}

// Update the buffer level after encoding with encoded frame size.
static void update_buffer_level_postencode(VP9_COMP *cpi,
                                           int encoded_frame_size) {
  RATE_CONTROL *const rc = &cpi->rc;
  rc->bits_off_target -= encoded_frame_size;
  // Clip the buffer level to the maximum specified buffer size.
  rc->bits_off_target = VPXMIN(rc->bits_off_target, rc->maximum_buffer_size);
  // For screen-content mode, and if frame-dropper is off, don't let buffer
  // level go below threshold, given here as -rc->maximum_ buffer_size.
  if (cpi->oxcf.content == VP9E_CONTENT_SCREEN &&
      cpi->oxcf.drop_frames_water_mark == 0)
    rc->bits_off_target = VPXMAX(rc->bits_off_target, -rc->maximum_buffer_size);

  rc->buffer_level = rc->bits_off_target;

  if (brc_libvpx_is_one_pass_cbr_svc(cpi)) {
    update_layer_buffer_level_postencode(&cpi->svc, encoded_frame_size);
  }
}

static int vp9_rc_get_default_min_gf_interval(int width, int height,
                                       double framerate) {
  // Assume we do not need any constraint lower than 4K 20 fps
  static const double factor_safe = 3840 * 2160 * 20.0;
  const double factor = width * height * framerate;
  const int default_interval =
      clamp((int)(framerate * 0.125), MIN_GF_INTERVAL, MAX_GF_INTERVAL);

  if (factor <= factor_safe)
    return default_interval;
  else
    return VPXMAX(default_interval,
                  (int)(MIN_GF_INTERVAL * factor / factor_safe + 0.5));
  // Note this logic makes:
  // 4K24: 5
  // 4K30: 6
  // 4K60: 12
}

static int vp9_rc_get_default_max_gf_interval(double framerate, int min_gf_interval) {
  int interval = VPXMIN(MAX_GF_INTERVAL, (int)(framerate * 0.75));
  interval += (interval & 0x01);  // Round to even value
  return VPXMAX(interval, min_gf_interval);
}

void brc_libvpx_vp9_rc_init(const VP9EncoderConfig *oxcf, int pass, RATE_CONTROL *rc) {
  int i;

  if (pass == 0 && oxcf->rc_mode == VPX_CBR) {
    rc->avg_frame_qindex[KEY_FRAME] = oxcf->worst_allowed_q;
    rc->avg_frame_qindex[INTER_FRAME] = oxcf->worst_allowed_q;
  } else {
    rc->avg_frame_qindex[KEY_FRAME] =
        (oxcf->worst_allowed_q + oxcf->best_allowed_q) / 2;
    rc->avg_frame_qindex[INTER_FRAME] =
        (oxcf->worst_allowed_q + oxcf->best_allowed_q) / 2;
  }

  rc->last_q[KEY_FRAME] = oxcf->best_allowed_q;
  rc->last_q[INTER_FRAME] = oxcf->worst_allowed_q;

  rc->buffer_level = rc->starting_buffer_level;
  rc->bits_off_target = rc->starting_buffer_level;

  rc->rolling_target_bits = rc->avg_frame_bandwidth;
  rc->rolling_actual_bits = rc->avg_frame_bandwidth;
  rc->long_rolling_target_bits = rc->avg_frame_bandwidth;
  rc->long_rolling_actual_bits = rc->avg_frame_bandwidth;

  rc->total_actual_bits = 0;
  rc->total_target_bits = 0;
  rc->total_target_vs_actual = 0;
  rc->avg_frame_low_motion = 0;
  rc->count_last_scene_change = 0;
  rc->af_ratio_onepass_vbr = 10;
  rc->prev_avg_source_sad_lag = 0;
  rc->high_source_sad = 0;
  rc->reset_high_source_sad = 0;
  rc->high_source_sad_lagindex = -1;
  rc->high_num_blocks_with_motion = 0;
  rc->hybrid_intra_scene_change = 0;
  rc->re_encode_maxq_scene_change = 0;
  rc->alt_ref_gf_group = 0;
  rc->last_frame_is_src_altref = 0;
  rc->fac_active_worst_inter = 150;
  rc->fac_active_worst_gf = 100;
  rc->force_qpmin = 0;
  for (i = 0; i < MAX_LAG_BUFFERS; ++i) rc->avg_source_sad[i] = 0;
  rc->frames_to_key = 0;
  rc->frames_since_key = 8;  // Sensible default for first frame.
  rc->this_key_frame_forced = 0;
  rc->next_key_frame_forced = 0;

  rc->frames_till_gf_update_due = 0;
  rc->ni_av_qi = oxcf->worst_allowed_q;
  rc->ni_tot_qi = 0;
  rc->ni_frames = 0;

  rc->tot_q = 0.0;
  rc->avg_q = vp9_convert_qindex_to_q(oxcf->worst_allowed_q, oxcf->bit_depth);

  for (i = 0; i < RATE_FACTOR_LEVELS; ++i) {
    rc->rate_correction_factors[i] = 1.0;
    rc->damped_adjustment[i] = 0;
  }

  rc->min_gf_interval = oxcf->min_gf_interval;
  rc->max_gf_interval = oxcf->max_gf_interval;
  if (rc->min_gf_interval == 0)
    rc->min_gf_interval = vp9_rc_get_default_min_gf_interval(
        oxcf->width, oxcf->height, oxcf->init_framerate);
  if (rc->max_gf_interval == 0)
    rc->max_gf_interval = vp9_rc_get_default_max_gf_interval(
        oxcf->init_framerate, rc->min_gf_interval);
  rc->baseline_gf_interval = (rc->min_gf_interval + rc->max_gf_interval) / 2;

  rc->force_max_q = 0;
  rc->last_post_encode_dropped_scene_change = 0;
  rc->arf_active_best_quality_adjustment_factor = 1.0;
  rc->arf_increase_active_best_quality = 0;
  rc->preserve_arf_as_gld = 0;
  rc->preserve_next_arf_as_gld = 0;
}

static int adjust_q_cbr(const VP9_COMP *cpi, int q) {
  // This makes sure q is between oscillating Qs to prevent resonance.
  if (!cpi->rc.reset_high_source_sad &&
      (cpi->rc.rc_1_frame * cpi->rc.rc_2_frame == -1) &&
      cpi->rc.q_1_frame != cpi->rc.q_2_frame) {
    int qclamp = clamp(q, VPXMIN(cpi->rc.q_1_frame, cpi->rc.q_2_frame),
                       VPXMAX(cpi->rc.q_1_frame, cpi->rc.q_2_frame));
    // If the previous frame had overshoot and the current q needs to increase
    // above the clamped value, reduce the clamp for faster reaction to
    // overshoot.
    if (cpi->rc.rc_1_frame == -1 && q > qclamp)
      q = (q + qclamp) >> 1;
    else
      q = qclamp;
  }
  return VPXMAX(VPXMIN(q, cpi->rc.worst_quality), cpi->rc.best_quality);
}

static double get_rate_correction_factor(const VP9_COMP *cpi) {
  const RATE_CONTROL *const rc = &cpi->rc;
  const VP9_COMMON *const cm = &cpi->common;
  double rcf;

  if (brc_libvpx_vp9_frame_is_intra_only(cm)) {
    rcf = rc->rate_correction_factors[KF_STD];
  } else {
      rcf = rc->rate_correction_factors[INTER_NORMAL];
  }
  rcf *= rcf_mult[rc->frame_size_selector % FRAME_SCALE_STEPS];
  return fclamp(rcf, MIN_BPB_FACTOR, MAX_BPB_FACTOR);
}

static void set_rate_correction_factor(VP9_COMP *cpi, double factor) {
  RATE_CONTROL *const rc = &cpi->rc;
  const VP9_COMMON *const cm = &cpi->common;

  // Normalize RCF to account for the size-dependent scaling factor.
  factor /= rcf_mult[cpi->rc.frame_size_selector % FRAME_SCALE_STEPS];

  factor = fclamp(factor, MIN_BPB_FACTOR, MAX_BPB_FACTOR);

  if (brc_libvpx_vp9_frame_is_intra_only(cm)) {
    rc->rate_correction_factors[KF_STD] = factor;
  } 
  else {
      rc->rate_correction_factors[INTER_NORMAL] = factor;
  }
}

static void vp9_rc_update_rate_correction_factors(VP9_COMP *cpi) {
  const VP9_COMMON *const cm = &cpi->common;
  int correction_factor = 100;
  double rate_correction_factor = get_rate_correction_factor(cpi);
  double adjustment_limit;
  int rf_lvl = 0;
  int projected_size_based_on_q = 0;

  FRAME_TYPE frame_type = cm->intra_only ? KEY_FRAME : cm->frame_type;
  projected_size_based_on_q =
      vp9_estimate_bits_at_q(frame_type, cm->base_qindex, cm->MBs,
                               rate_correction_factor, cm->bit_depth);

  // Work out a size correction factor.
  if (projected_size_based_on_q > FRAME_OVERHEAD_BITS)
    correction_factor = (int)((100 * (int64_t)cpi->rc.projected_frame_size) /
                              projected_size_based_on_q);

  // Do not use damped adjustment for the first frame of each frame type
  if (!cpi->rc.damped_adjustment[rf_lvl]) {
    adjustment_limit = 1.0;
    cpi->rc.damped_adjustment[rf_lvl] = 1;
  } else {
    // More heavily damped adjustment used if we have been oscillating either
    // side of target.
    adjustment_limit =
        0.25 + 0.5 * VPXMIN(1, fabs(log10(0.01 * correction_factor)));
  }

  cpi->rc.q_2_frame = cpi->rc.q_1_frame;
  cpi->rc.q_1_frame = cm->base_qindex;
  cpi->rc.rc_2_frame = cpi->rc.rc_1_frame;
  if (correction_factor > 110)
    cpi->rc.rc_1_frame = -1;
  else if (correction_factor < 90)
    cpi->rc.rc_1_frame = 1;
  else
    cpi->rc.rc_1_frame = 0;

  // Turn off oscilation detection in the case of massive overshoot.
  if (cpi->rc.rc_1_frame == -1 && cpi->rc.rc_2_frame == 1 &&
      correction_factor > 1000) {
    cpi->rc.rc_2_frame = 0;
  }

  if (correction_factor > 102) {
    // We are not already at the worst allowable quality
    correction_factor =
        (int)(100 + ((correction_factor - 100) * adjustment_limit));
    rate_correction_factor = (rate_correction_factor * correction_factor) / 100;
    // Keep rate_correction_factor within limits
    if (rate_correction_factor > MAX_BPB_FACTOR)
      rate_correction_factor = MAX_BPB_FACTOR;
  } else if (correction_factor < 99) {
    // We are not already at the best allowable quality
    correction_factor =
        (int)(100 - ((100 - correction_factor) * adjustment_limit));
    rate_correction_factor = (rate_correction_factor * correction_factor) / 100;

    // Keep rate_correction_factor within limits
    if (rate_correction_factor < MIN_BPB_FACTOR)
      rate_correction_factor = MIN_BPB_FACTOR;
  }

  set_rate_correction_factor(cpi, rate_correction_factor);
}

static int vp9_rc_regulate_q(const VP9_COMP *cpi, int target_bits_per_frame,
                      int active_best_quality, int active_worst_quality) {
  const VP9_COMMON *const cm = &cpi->common;
  int q = active_worst_quality;
  int last_error = INT_MAX;
  int i, target_bits_per_mb, bits_per_mb_at_this_q;
  const double correction_factor = get_rate_correction_factor(cpi);

  // Calculate required scaling factor based on target frame size and size of
  // frame produced using previous Q.
  target_bits_per_mb =
      (int)(((int64_t)target_bits_per_frame << BPER_MB_NORMBITS) / cm->MBs);

  i = active_best_quality;

  do {
    {
      FRAME_TYPE frame_type = cm->intra_only ? KEY_FRAME : cm->frame_type;
      bits_per_mb_at_this_q = (int)vp9_rc_bits_per_mb(
          frame_type, i, correction_factor, cm->bit_depth);
    }

    if (bits_per_mb_at_this_q <= target_bits_per_mb) {
      if ((target_bits_per_mb - bits_per_mb_at_this_q) <= last_error)
        q = i;
      else
        q = i - 1;

      break;
    } else {
      last_error = bits_per_mb_at_this_q - target_bits_per_mb;
    }
  } while (++i <= active_worst_quality);

  // Adjustment to q for CBR mode.
  if (cpi->oxcf.rc_mode == VPX_CBR) return adjust_q_cbr(cpi, q);

  return q;
}

static int get_active_quality(int q, int gfu_boost, int low, int high,
                              int *low_motion_minq, int *high_motion_minq) {
  if (gfu_boost > high) {
    return low_motion_minq[q];
  } else if (gfu_boost < low) {
    return high_motion_minq[q];
  } else {
    const int gap = high - low;
    const int offset = high - gfu_boost;
    const int qdiff = high_motion_minq[q] - low_motion_minq[q];
    const int adjustment = ((offset * qdiff) + (gap >> 1)) / gap;
    return low_motion_minq[q] + adjustment;
  }
}

static int get_kf_active_quality(const RATE_CONTROL *const rc, int q,
                                 vpx_bit_depth_t bit_depth) {
  int *kf_low_motion_minq;
  int *kf_high_motion_minq;
  ASSIGN_MINQ_TABLE(bit_depth, kf_low_motion_minq);
  ASSIGN_MINQ_TABLE(bit_depth, kf_high_motion_minq);
  return get_active_quality(q, rc->kf_boost, kf_low, kf_high,
                            kf_low_motion_minq, kf_high_motion_minq);
}

// Adjust active_worst_quality level based on buffer level.
static int calc_active_worst_quality_one_pass_cbr(const VP9_COMP *cpi) {
  // Adjust active_worst_quality: If buffer is above the optimal/target level,
  // bring active_worst_quality down depending on fullness of buffer.
  // If buffer is below the optimal level, let the active_worst_quality go from
  // ambient Q (at buffer = optimal level) to worst_quality level
  // (at buffer = critical level).
  const VP9_COMMON *const cm = &cpi->common;
  const RATE_CONTROL *rc = &cpi->rc;
  // Buffer level below which we push active_worst to worst_quality.
  int64_t critical_level = rc->optimal_buffer_level >> 3;
  int64_t buff_lvl_step = 0;
  int adjustment = 0;
  int active_worst_quality;
  int ambient_qp;
  unsigned int num_frames_weight_key = 5 * 1; //Note:one temporal layer
  if (brc_libvpx_vp9_frame_is_intra_only(cm) || rc->reset_high_source_sad || rc->force_max_q)
    return rc->worst_quality;
  // For ambient_qp we use minimum of avg_frame_qindex[KEY_FRAME/INTER_FRAME]
  // for the first few frames following key frame. These are both initialized
  // to worst_quality and updated with (3/4, 1/4) average in postencode_update.
  // So for first few frames following key, the qp of that key frame is weighted
  // into the active_worst_quality setting.
  ambient_qp = (cm->current_video_frame < num_frames_weight_key)
                   ? VPXMIN(rc->avg_frame_qindex[INTER_FRAME],
                            rc->avg_frame_qindex[KEY_FRAME])
                   : rc->avg_frame_qindex[INTER_FRAME];
  active_worst_quality = VPXMIN(rc->worst_quality, (ambient_qp * 5) >> 2);

  // For SVC if the current base spatial layer was key frame, use the QP from
  // that base layer for ambient_qp.
  if (cpi->use_svc && cpi->svc.spatial_layer_id > 0) {
    int layer = LAYER_IDS_TO_IDX(0, cpi->svc.temporal_layer_id,
                                 cpi->svc.number_temporal_layers);
    const LAYER_CONTEXT *lc = &cpi->svc.layer_context[layer];
    if (lc->is_key_frame) {
      const RATE_CONTROL *lrc = &lc->rc;
      ambient_qp = VPXMIN(ambient_qp, lrc->last_q[KEY_FRAME]);
      active_worst_quality = VPXMIN(rc->worst_quality, (ambient_qp * 9) >> 3);
    }
  }

  if (rc->buffer_level > rc->optimal_buffer_level) {
    // Adjust down.
    // Maximum limit for down adjustment ~30%; make it lower for screen content.
    int max_adjustment_down = active_worst_quality / 3;
    if (max_adjustment_down) {
      buff_lvl_step = ((rc->maximum_buffer_size - rc->optimal_buffer_level) /
                       max_adjustment_down);
      if (buff_lvl_step)
        adjustment = (int)((rc->buffer_level - rc->optimal_buffer_level) /
                           buff_lvl_step);
      active_worst_quality -= adjustment;
    }
  } else if (rc->buffer_level > critical_level) {
    // Adjust up from ambient Q.
    if (critical_level) {
      buff_lvl_step = (rc->optimal_buffer_level - critical_level);
      if (buff_lvl_step) {
        adjustment = (int)((rc->worst_quality - ambient_qp) *
                           (rc->optimal_buffer_level - rc->buffer_level) /
                           buff_lvl_step);
      }
      active_worst_quality = ambient_qp + adjustment;
    }
  } else {
    // Set to worst_quality if buffer is below critical level.
    active_worst_quality = rc->worst_quality;
  }
  return active_worst_quality;
}

static int rc_pick_q_and_bounds_one_pass_cbr(const VP9_COMP *cpi,
                                             int *bottom_index,
                                             int *top_index) {
  const VP9_COMMON *const cm = &cpi->common;
  const RATE_CONTROL *const rc = &cpi->rc;
  int active_best_quality = 0;
  int active_worst_quality = calc_active_worst_quality_one_pass_cbr(cpi);
  int q;
  int *rtc_minq;
  ASSIGN_MINQ_TABLE(cm->bit_depth, rtc_minq);

  if (brc_libvpx_vp9_frame_is_intra_only(cm)) {
    active_best_quality = rc->best_quality;
    // Handle the special case for key frames forced when we have reached
    // the maximum key frame interval. Here force the Q to a range
    // based on the ambient Q to reduce the risk of popping.
    if (rc->this_key_frame_forced) {
      int qindex = rc->last_boosted_qindex;
      double last_boosted_q = vp9_convert_qindex_to_q(qindex, cm->bit_depth);
      int delta_qindex = vp9_compute_qdelta(
          rc, last_boosted_q, (last_boosted_q * 0.75), cm->bit_depth);
      active_best_quality = VPXMAX(qindex + delta_qindex, rc->best_quality);
    } else if (cm->current_video_frame > 0) {
      // not first frame of one pass and kf_boost is set
      double q_adj_factor = 1.0;
      double q_val;

      active_best_quality = get_kf_active_quality(
          rc, rc->avg_frame_qindex[KEY_FRAME], cm->bit_depth);

      // Allow somewhat lower kf minq with small image formats.
      if ((cm->width * cm->height) <= (352 * 288)) {
        q_adj_factor -= 0.25;
      }

      // Convert the adjustment factor to a qindex delta
      // on active_best_quality.
      q_val = vp9_convert_qindex_to_q(active_best_quality, cm->bit_depth);
      active_best_quality +=
          vp9_compute_qdelta(rc, q_val, q_val * q_adj_factor, cm->bit_depth);
    }
  } else {
    // Use the lower of active_worst_quality and recent/average Q.
    if (cm->current_video_frame > 1) {
      if (rc->avg_frame_qindex[INTER_FRAME] < active_worst_quality)
        active_best_quality = rtc_minq[rc->avg_frame_qindex[INTER_FRAME]];
      else
        active_best_quality = rtc_minq[active_worst_quality];
    } else {
      if (rc->avg_frame_qindex[KEY_FRAME] < active_worst_quality)
        active_best_quality = rtc_minq[rc->avg_frame_qindex[KEY_FRAME]];
      else
        active_best_quality = rtc_minq[active_worst_quality];
    }
  }

  // Clip the active best and worst quality values to limits
  active_best_quality =
      clamp(active_best_quality, rc->best_quality, rc->worst_quality);
  active_worst_quality =
      clamp(active_worst_quality, active_best_quality, rc->worst_quality);

  *top_index = active_worst_quality;
  *bottom_index = active_best_quality;

  // Special case code to try and match quality with forced key frames
  if (brc_libvpx_vp9_frame_is_intra_only(cm) && rc->this_key_frame_forced) {
    q = rc->last_boosted_qindex;
  } else {
    q = vp9_rc_regulate_q(cpi, rc->this_frame_target, active_best_quality,
                          active_worst_quality);
    if (q > *top_index) {
      // Special case when we are targeting the max allowed rate
      if (rc->this_frame_target >= rc->max_frame_bandwidth)
        *top_index = q;
      else
        q = *top_index;
    }
  }

  assert(*top_index <= rc->worst_quality && *top_index >= rc->best_quality);
  assert(*bottom_index <= rc->worst_quality &&
         *bottom_index >= rc->best_quality);
  assert(q <= rc->worst_quality && q >= rc->best_quality);
  return q;
}

#define SMOOTH_PCT_MIN 0.1
#define SMOOTH_PCT_DIV 0.05

#define STATIC_MOTION_THRESH 95

int brc_libvpx_vp9_rc_pick_q_and_bounds(const VP9_COMP *cpi, int *bottom_index,
                             int *top_index) {
  int q = 0;
  if (cpi->oxcf.pass == 0) {
    if (cpi->oxcf.rc_mode == VPX_CBR)
      q = rc_pick_q_and_bounds_one_pass_cbr(cpi, bottom_index, top_index);
  }

  if (cpi->sf.use_nonrd_pick_mode) {
    if (q < *bottom_index)
      *bottom_index = q;
    else if (q > *top_index)
      *top_index = q;
  }

  return q;
}

void brc_libvpx_vp9_rc_set_frame_target(VP9_COMP *cpi, int target) {
  const VP9_COMMON *const cm = &cpi->common;
  RATE_CONTROL *const rc = &cpi->rc;

  rc->this_frame_target = target;

  // Target rate per SB64 (including partial SB64s.
  rc->sb64_target_rate = (int)(((int64_t)rc->this_frame_target * 64 * 64) /
                               (cm->width * cm->height));
}

static void update_golden_frame_stats(VP9_COMP *cpi) {
  RATE_CONTROL *const rc = &cpi->rc;

  // Update the Golden frame usage counts.
  if (cpi->refresh_golden_frame) {
    // this frame refreshes means next frames don't unless specified by user
    rc->frames_since_golden = 0;

    // Decrement count down till next gf
    if (rc->frames_till_gf_update_due > 0) rc->frames_till_gf_update_due--;

  } else {
    // Decrement count down till next gf
    if (rc->frames_till_gf_update_due > 0) rc->frames_till_gf_update_due--;

    rc->frames_since_golden++;
  }
}

void brc_libvpx_vp9_rc_postencode_update(VP9_COMP *cpi, int64_t bytes_used) {
  const VP9_COMMON *const cm = &cpi->common;
  const VP9EncoderConfig *const oxcf = &cpi->oxcf;
  RATE_CONTROL *const rc = &cpi->rc;
  SVC *const svc = &cpi->svc;
  const int qindex = cm->base_qindex;

  // Update rate control heuristics
  rc->projected_frame_size = (int)(bytes_used << 3);

  // Post encode loop adjustment of Q prediction.
  vp9_rc_update_rate_correction_factors(cpi);

  // Keep a record of last Q and ambient average Q.
  if (brc_libvpx_vp9_frame_is_intra_only(cm)) {
    rc->last_q[KEY_FRAME] = qindex;
    rc->avg_frame_qindex[KEY_FRAME] =
        ROUND_POWER_OF_TWO(3 * rc->avg_frame_qindex[KEY_FRAME] + qindex, 2);

    if (cpi->use_svc) {
      int i = 0;
      SVC *svc = &cpi->svc;
      for (i = 0; i < svc->number_temporal_layers; ++i) {
        const int layer = LAYER_IDS_TO_IDX(svc->spatial_layer_id, i,
                                           svc->number_temporal_layers);
        LAYER_CONTEXT *lc = &svc->layer_context[layer];
        RATE_CONTROL *lrc = &lc->rc;
        lrc->last_q[KEY_FRAME] = rc->last_q[KEY_FRAME];
        lrc->avg_frame_qindex[KEY_FRAME] = rc->avg_frame_qindex[KEY_FRAME];
      }
    }

  } else {
    rc->last_q[INTER_FRAME] = qindex;
    rc->avg_frame_qindex[INTER_FRAME] =
       ROUND_POWER_OF_TWO(3 * rc->avg_frame_qindex[INTER_FRAME] + qindex, 2);
    rc->ni_frames++;
    rc->tot_q += vp9_convert_qindex_to_q(qindex, cm->bit_depth);
    rc->avg_q = rc->tot_q / rc->ni_frames;
    // Calculate the average Q for normal inter frames (not key or GFU
    // frames).
    rc->ni_tot_qi += qindex;
    rc->ni_av_qi = rc->ni_tot_qi / rc->ni_frames;
  }

  if (cpi->use_svc) vp9_svc_adjust_avg_frame_qindex(cpi);

  // Keep record of last boosted (KF/KF/ARF) Q value.
  // If the current frame is coded at a lower Q then we also update it.
  // If all mbs in this group are skipped only update if the Q value is
  // better than that already stored.
  // This is used to help set quality in forced key frames to reduce popping
  if ((qindex < rc->last_boosted_qindex) || (cm->frame_type == KEY_FRAME)) {
    rc->last_boosted_qindex = qindex;
  }

  if (brc_libvpx_vp9_frame_is_intra_only(cm)) rc->last_kf_qindex = qindex;

  update_buffer_level_postencode(cpi, rc->projected_frame_size);

  // Rolling monitors of whether we are over or underspending used to help
  // regulate min and Max Q in two pass.
  if (!brc_libvpx_vp9_frame_is_intra_only(cm)) {
    rc->rolling_target_bits = ROUND_POWER_OF_TWO(
        rc->rolling_target_bits * 3 + rc->this_frame_target, 2);
    rc->rolling_actual_bits = ROUND_POWER_OF_TWO(
        rc->rolling_actual_bits * 3 + rc->projected_frame_size, 2);
    rc->long_rolling_target_bits = ROUND_POWER_OF_TWO(
        rc->long_rolling_target_bits * 31 + rc->this_frame_target, 5);
    rc->long_rolling_actual_bits = ROUND_POWER_OF_TWO(
        rc->long_rolling_actual_bits * 31 + rc->projected_frame_size, 5);
  }

  // Actual bits spent
  rc->total_actual_bits += rc->projected_frame_size;
  rc->total_target_bits += cm->show_frame ? rc->avg_frame_bandwidth : 0;

  rc->total_target_vs_actual = rc->total_actual_bits - rc->total_target_bits;

  if (!cpi->use_svc) {
      // Update the Golden frame stats as appropriate.
      update_golden_frame_stats(cpi);
  }

  // If second (long term) temporal reference is used for SVC,
  // update the golden frame counter, only for base temporal layer.
  if (cpi->use_svc && svc->use_gf_temporal_ref_current_layer &&
      svc->temporal_layer_id == 0) {
    int i = 0;
    if (cpi->refresh_golden_frame)
      rc->frames_since_golden = 0;
    else
      rc->frames_since_golden++;
    // Decrement count down till next gf
    if (rc->frames_till_gf_update_due > 0) rc->frames_till_gf_update_due--;
    // Update the frames_since_golden for all upper temporal layers.
    for (i = 1; i < svc->number_temporal_layers; ++i) {
      const int layer = LAYER_IDS_TO_IDX(svc->spatial_layer_id, i,
                                         svc->number_temporal_layers);
      LAYER_CONTEXT *const lc = &svc->layer_context[layer];
      RATE_CONTROL *const lrc = &lc->rc;
      lrc->frames_since_golden = rc->frames_since_golden;
    }
  }

  if (brc_libvpx_vp9_frame_is_intra_only(cm)) rc->frames_since_key = 0;
  if (cm->show_frame) {
    rc->frames_since_key++;
    rc->frames_to_key--;
  }

  // Trigger the resizing of the next frame if it is scaled.
  if (oxcf->pass != 0) {
#if 0
    cpi->resize_pending =
        rc->next_frame_size_selector != rc->frame_size_selector;
#endif
    rc->frame_size_selector = rc->next_frame_size_selector;
  }

#if 0
  //Fixme: No VBR Support yet
  if (oxcf->pass == 0) {
    if (!brc_libvpx_vp9_frame_is_intra_only(cm))
      if (cpi->sf.use_altref_onepass) update_altref_usage(cpi);
    cpi->rc.last_frame_is_src_altref = cpi->rc.is_src_frame_alt_ref;
  }
#endif
  if (!brc_libvpx_vp9_frame_is_intra_only(cm)) rc->reset_high_source_sad = 0;

  rc->last_avg_frame_bandwidth = rc->avg_frame_bandwidth;
  if (cpi->use_svc && svc->spatial_layer_id < svc->number_spatial_layers - 1)
    svc->lower_layer_qindex = cm->base_qindex;

#if 0
  if (oxcf->pass == 0) {
    if (!brc_libvpx_vp9_frame_is_intra_only(cm) &&
        (!cpi->use_svc
	 ||
         (cpi->use_svc &&
          !svc->layer_context[svc->temporal_layer_id].is_key_frame &&
          svc->spatial_layer_id == svc->number_spatial_layers - 1)

	 )) {
       //Note: Only required in VBR: compute_frame_low_motion
      //compute_frame_low_motion(cpi);
    }

    // For SVC: set avg_frame_low_motion (only computed on top spatial layer)
    // to all lower spatial layers.
    if (cpi->use_svc &&
        svc->spatial_layer_id == svc->number_spatial_layers - 1) {
      int i;
      for (i = 0; i < svc->number_spatial_layers - 1; ++i) {
        const int layer = LAYER_IDS_TO_IDX(i, svc->temporal_layer_id,
                                           svc->number_temporal_layers);
        LAYER_CONTEXT *const lc = &svc->layer_context[layer];
        RATE_CONTROL *const lrc = &lc->rc;
        lrc->avg_frame_low_motion = rc->avg_frame_low_motion;
      }
    }

    cpi->rc.last_frame_is_src_altref = cpi->rc.is_src_frame_alt_ref;
  }
  if (!brc_libvpx_vp9_frame_is_intra_only(cm)) rc->reset_high_source_sad = 0;

  rc->last_avg_frame_bandwidth = rc->avg_frame_bandwidth;

  if (cpi->use_svc && svc->spatial_layer_id < svc->number_spatial_layers - 1)
    svc->lower_layer_qindex = cm->base_qindex;
#endif
}

int brc_libvpx_vp9_calc_pframe_target_size_one_pass_cbr(const VP9_COMP *cpi) {
  const VP9EncoderConfig *oxcf = &cpi->oxcf;
  const RATE_CONTROL *rc = &cpi->rc;
  const SVC *const svc = &cpi->svc;
  const int64_t diff = rc->optimal_buffer_level - rc->buffer_level;
  const int64_t one_pct_bits = 1 + rc->optimal_buffer_level / 100;
  int min_frame_target =
      VPXMAX(rc->avg_frame_bandwidth >> 4, FRAME_OVERHEAD_BITS);
  int target;

  target = rc->avg_frame_bandwidth;

  if (brc_libvpx_is_one_pass_cbr_svc(cpi)) {
    // Note that for layers, avg_frame_bandwidth is the cumulative
    // per-frame-bandwidth. For the target size of this frame, use the
    // layer average frame size (i.e., non-cumulative per-frame-bw).
    int layer = LAYER_IDS_TO_IDX(svc->spatial_layer_id, svc->temporal_layer_id,
                                 svc->number_temporal_layers);
    const LAYER_CONTEXT *lc = &svc->layer_context[layer];
    target = lc->avg_frame_size;
    min_frame_target = VPXMAX(lc->avg_frame_size >> 4, FRAME_OVERHEAD_BITS);
  }

  if (diff > 0) {
    // Lower the target bandwidth for this frame.
    const int pct_low = (int)VPXMIN(diff / one_pct_bits, oxcf->under_shoot_pct);
    target -= (target * pct_low) / 200;
  } else if (diff < 0) {
    // Increase the target bandwidth for this frame.
    const int pct_high =
        (int)VPXMIN(-diff / one_pct_bits, oxcf->over_shoot_pct);
    target += (target * pct_high) / 200;
  }
  if (oxcf->rc_max_inter_bitrate_pct) {
    const int max_rate =
        rc->avg_frame_bandwidth * oxcf->rc_max_inter_bitrate_pct / 100;
    target = VPXMIN(target, max_rate);
  }
  return VPXMAX(min_frame_target, target);
}

int brc_libvpx_vp9_calc_iframe_target_size_one_pass_cbr(VP9_COMP *cpi) {
  RATE_CONTROL *rc = &cpi->rc;
  const VP9EncoderConfig *oxcf = &cpi->oxcf;
  const SVC *const svc = &cpi->svc;
  int target;
  if (cpi->common.current_video_frame == 0) {
    target = ((rc->starting_buffer_level / 2) > INT_MAX)
                 ? INT_MAX
                 : (int)(rc->starting_buffer_level / 2);
  } else {
    int kf_boost = 32;
    double framerate = cpi->framerate;

    if (svc->number_temporal_layers > 1 && oxcf->rc_mode == VPX_CBR) {
      // Use the layer framerate for temporal layers CBR mode.
      const int layer =
          LAYER_IDS_TO_IDX(svc->spatial_layer_id, svc->temporal_layer_id,
                           svc->number_temporal_layers);
      const LAYER_CONTEXT *lc = &svc->layer_context[layer];
      framerate = lc->framerate;
    }

    kf_boost = VPXMAX(kf_boost, (int)(2 * framerate - 16));
    if (rc->frames_since_key < framerate / 2) {
      kf_boost = (int)(kf_boost * rc->frames_since_key / (framerate / 2));
    }
    target = ((16 + kf_boost) * rc->avg_frame_bandwidth) >> 4;
  }
  return vp9_rc_clamp_iframe_target_size(cpi, target);
}

void brc_libvpx_vp9_rc_get_svc_params(VP9_COMP *cpi) {
  VP9_COMMON *const cm = &cpi->common;
  RATE_CONTROL *const rc = &cpi->rc;
  SVC *const svc = &cpi->svc;
  int target = rc->avg_frame_bandwidth;
  int layer = LAYER_IDS_TO_IDX(svc->spatial_layer_id, svc->temporal_layer_id,
                               svc->number_temporal_layers);
  if (svc->first_spatial_layer_to_encode)
    svc->layer_context[svc->temporal_layer_id].is_key_frame = 0;
  // Periodic key frames is based on the super-frame counter
  // (svc.current_superframe), also only base spatial layer is key frame.
  // Key frame is set for any of the following: very first frame, frame flags
  // indicates key, superframe counter hits key frequency, or (non-intra) sync
  // flag is set for spatial layer 0.
  if (cm->frame_type == KEY_FRAME) {
    if (brc_libvpx_is_one_pass_cbr_svc(cpi)) {
      if (cm->current_video_frame > 0) vp9_svc_reset_temporal_layers(cpi, 1);
      layer = LAYER_IDS_TO_IDX(svc->spatial_layer_id, svc->temporal_layer_id,
                               svc->number_temporal_layers);
      svc->layer_context[layer].is_key_frame = 1;
      //cpi->ref_frame_flags &= (~VP9_LAST_FLAG & ~VP9_GOLD_FLAG & ~VP9_ALT_FLAG);
      // Assumption here is that LAST_FRAME is being updated for a keyframe.
      // Thus no change in update flags.
      target = brc_libvpx_vp9_calc_iframe_target_size_one_pass_cbr(cpi);
    }
  } else {
    cm->frame_type = INTER_FRAME;
    if (brc_libvpx_is_one_pass_cbr_svc(cpi)) {
      LAYER_CONTEXT *lc = &svc->layer_context[layer];
      // Add condition current_video_frame > 0 for the case where first frame
      // is intra only followed by overlay/copy frame. In this case we don't
      // want to reset is_key_frame to 0 on overlay/copy frame.
      lc->is_key_frame =
          (svc->spatial_layer_id == 0 && cm->current_video_frame > 0)
              ? 0
              : svc->layer_context[svc->temporal_layer_id].is_key_frame;
      target = brc_libvpx_vp9_calc_pframe_target_size_one_pass_cbr(cpi);
    }
  }

  // Check if superframe contains a sync layer request.
  vp9_svc_check_spatial_layer_sync(cpi);

  rc->frames_till_gf_update_due = INT_MAX;
  rc->baseline_gf_interval = INT_MAX;

  //ToDo: Add support for intra-only

  brc_libvpx_vp9_rc_set_frame_target(cpi, target);
  if (cm->show_frame) update_buffer_level_svc_preencode(cpi);
}

static int vp9_compute_qdelta(const RATE_CONTROL *rc, double qstart, double qtarget,
                       vpx_bit_depth_t bit_depth) {
  int start_index = rc->worst_quality;
  int target_index = rc->worst_quality;
  int i;

  // Convert the average q value to an index.
  for (i = rc->best_quality; i < rc->worst_quality; ++i) {
    start_index = i;
    if (vp9_convert_qindex_to_q(i, bit_depth) >= qstart) break;
  }

  // Convert the q target to an index
  for (i = rc->best_quality; i < rc->worst_quality; ++i) {
    target_index = i;
    if (vp9_convert_qindex_to_q(i, bit_depth) >= qtarget) break;
  }

  return target_index - start_index;
}

void brc_libvpx_vp9_rc_set_gf_interval_range(const VP9_COMP *const cpi,
                                  RATE_CONTROL *const rc) {
  const VP9EncoderConfig *const oxcf = &cpi->oxcf;

  // Set Maximum gf/arf interval
  rc->max_gf_interval = oxcf->max_gf_interval;
  rc->min_gf_interval = oxcf->min_gf_interval;
  if (rc->min_gf_interval == 0)
    rc->min_gf_interval = vp9_rc_get_default_min_gf_interval(
        oxcf->width, oxcf->height, cpi->framerate);
  if (rc->max_gf_interval == 0)
    rc->max_gf_interval = vp9_rc_get_default_max_gf_interval(
        cpi->framerate, rc->min_gf_interval);

  // Extended max interval for genuinely static scenes like slide shows.
  rc->static_scene_max_gf_interval = MAX_STATIC_GF_GROUP_LENGTH;

  if (rc->max_gf_interval > rc->static_scene_max_gf_interval)
    rc->max_gf_interval = rc->static_scene_max_gf_interval;

  // Clamp min to max
  rc->min_gf_interval = VPXMIN(rc->min_gf_interval, rc->max_gf_interval);
}

static void vp9_rc_update_framerate(VP9_COMP *cpi) {
  const VP9_COMMON *const cm = &cpi->common;
  const VP9EncoderConfig *const oxcf = &cpi->oxcf;
  RATE_CONTROL *const rc = &cpi->rc;
  int vbr_max_bits;

  rc->avg_frame_bandwidth = (int)(oxcf->target_bandwidth / cpi->framerate);
  rc->min_frame_bandwidth =
      (int)(rc->avg_frame_bandwidth * oxcf->two_pass_vbrmin_section / 100);

  rc->min_frame_bandwidth =
      VPXMAX(rc->min_frame_bandwidth, FRAME_OVERHEAD_BITS);

  // A maximum bitrate for a frame is defined.
  // However this limit is extended if a very high rate is given on the command
  // line or the rate can not be achieved because of a user specified max q
  // (e.g. when the user specifies lossless encode).
  //
  // If a level is specified that requires a lower maximum rate then the level
  // value take precedence.
  vbr_max_bits =
      (int)(((int64_t)rc->avg_frame_bandwidth * oxcf->two_pass_vbrmax_section) /
            100);
  rc->max_frame_bandwidth =
      VPXMAX(VPXMAX((cm->MBs * MAX_MB_RATE), MAXRATE_1080P), vbr_max_bits);

  brc_libvpx_vp9_rc_set_gf_interval_range(cpi, rc);
}

void
brc_libvpx_vp9_new_framerate (VP9_COMP * cpi, double framerate)
{
  cpi->framerate = framerate < 0.1 ? 30 : framerate;
  vp9_rc_update_framerate (cpi);
}

