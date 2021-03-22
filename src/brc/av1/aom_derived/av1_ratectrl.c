/*
 * Copyright (c) 2016, Alliance for Open Media. All rights reserved
 *  Copyright (c) 2020 Intel corporation. All Rights Reserved.
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <assert.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "av1_ratectrl.h"
#include "av1_svc_layercontext.h"
#include "av1_common.h"

#define USE_UNRESTRICTED_Q_IN_CQ_MODE 0

// Max rate target for 1080P and below encodes under normal circumstances
// (1920 * 1080 / (16 * 16)) * MAX_MB_RATE bits per MB
#define MAX_MB_RATE 250
#define MAXRATE_1080P 2025000

#define MIN_BPB_FACTOR 0.005
#define MAX_BPB_FACTOR 50

#define SUPERRES_QADJ_PER_DENOM_KEYFRAME_SOLO 0
#define SUPERRES_QADJ_PER_DENOM_KEYFRAME 2
#define SUPERRES_QADJ_PER_DENOM_ARFFRAME 0

/* Shift down with rounding for use when n >= 0, value >= 0 */
#define ROUND_POWER_OF_TWO(value, n) (((value) + (((1 << (n)) >> 1))) >> (n))

/* Shift down with rounding for signed integers, for use when n >= 0 */
#define ROUND_POWER_OF_TWO_SIGNED(value, n)           \
  (((value) < 0) ? -ROUND_POWER_OF_TWO(-(value), (n)) \
                 : ROUND_POWER_OF_TWO((value), (n)))

/* Shift down with rounding for use when n >= 0, value >= 0 for (64 bit) */
#define ROUND_POWER_OF_TWO_64(value, n) \
  (((value) + ((((int64_t)1 << (n)) >> 1))) >> (n))
/* Shift down with rounding for signed integers, for use when n >= 0 (64 bit) */
#define ROUND_POWER_OF_TWO_SIGNED_64(value, n)           \
  (((value) < 0) ? -ROUND_POWER_OF_TWO_64(-(value), (n)) \
                 : ROUND_POWER_OF_TWO_64((value), (n)))

/* shift right or left depending on sign of n */
#define RIGHT_SIGNED_SHIFT(value, n) \
  ((n) < 0 ? ((value) << (-(n))) : ((value) >> (n)))
// Tables relating active max Q to active min Q
static int kf_low_motion_minq_8[AV1_QINDEX_RANGE];
static int kf_high_motion_minq_8[AV1_QINDEX_RANGE];
static int arfgf_low_motion_minq_8[AV1_QINDEX_RANGE];
static int arfgf_high_motion_minq_8[AV1_QINDEX_RANGE];
static int inter_minq_8[AV1_QINDEX_RANGE];
static int rtc_minq_8[AV1_QINDEX_RANGE];

static int kf_low_motion_minq_10[AV1_QINDEX_RANGE];
static int kf_high_motion_minq_10[AV1_QINDEX_RANGE];
static int arfgf_low_motion_minq_10[AV1_QINDEX_RANGE];
static int arfgf_high_motion_minq_10[AV1_QINDEX_RANGE];
static int inter_minq_10[AV1_QINDEX_RANGE];
static int rtc_minq_10[AV1_QINDEX_RANGE];
static int kf_low_motion_minq_12[AV1_QINDEX_RANGE];
static int kf_high_motion_minq_12[AV1_QINDEX_RANGE];
static int arfgf_low_motion_minq_12[AV1_QINDEX_RANGE];
static int arfgf_high_motion_minq_12[AV1_QINDEX_RANGE];
static int inter_minq_12[AV1_QINDEX_RANGE];
static int rtc_minq_12[AV1_QINDEX_RANGE];
#define FRAME_OVERHEAD_BITS 200
#define ASSIGN_MINQ_TABLE(bit_depth, name)                   \
  do {                                                       \
    switch (bit_depth) {                                     \
      case AOM_BITS_8: name = name##_8; break;               \
      case AOM_BITS_10: name = name##_10; break;             \
      case AOM_BITS_12: name = name##_12; break;             \
      default:                                               \
        assert(0 &&                                          \
               "bit_depth should be AOM_BITS_8, AOM_BITS_10" \
               " or AOM_BITS_12");                           \
        name = NULL;                                         \
    }                                                        \
  } while (0)


#ifdef STRICT_RC
static int kf_high = 3200;
#else
static int kf_high = 5000;
#endif
static int kf_low = 400;

static const int16_t dc_qlookup_QTX[AV1_QINDEX_RANGE] = {
  4,    8,    8,    9,    10,  11,  12,  12,  13,  14,  15,   16,   17,   18,
  19,   19,   20,   21,   22,  23,  24,  25,  26,  26,  27,   28,   29,   30,
  31,   32,   32,   33,   34,  35,  36,  37,  38,  38,  39,   40,   41,   42,
  43,   43,   44,   45,   46,  47,  48,  48,  49,  50,  51,   52,   53,   53,
  54,   55,   56,   57,   57,  58,  59,  60,  61,  62,  62,   63,   64,   65,
  66,   66,   67,   68,   69,  70,  70,  71,  72,  73,  74,   74,   75,   76,
  77,   78,   78,   79,   80,  81,  81,  82,  83,  84,  85,   85,   87,   88,
  90,   92,   93,   95,   96,  98,  99,  101, 102, 104, 105,  107,  108,  110,
  111,  113,  114,  116,  117, 118, 120, 121, 123, 125, 127,  129,  131,  134,
  136,  138,  140,  142,  144, 146, 148, 150, 152, 154, 156,  158,  161,  164,
  166,  169,  172,  174,  177, 180, 182, 185, 187, 190, 192,  195,  199,  202,
  205,  208,  211,  214,  217, 220, 223, 226, 230, 233, 237,  240,  243,  247,
  250,  253,  257,  261,  265, 269, 272, 276, 280, 284, 288,  292,  296,  300,
  304,  309,  313,  317,  322, 326, 330, 335, 340, 344, 349,  354,  359,  364,
  369,  374,  379,  384,  389, 395, 400, 406, 411, 417, 423,  429,  435,  441,
  447,  454,  461,  467,  475, 482, 489, 497, 505, 513, 522,  530,  539,  549,
  559,  569,  579,  590,  602, 614, 626, 640, 654, 668, 684,  700,  717,  736,
  755,  775,  796,  819,  843, 869, 896, 925, 955, 988, 1022, 1058, 1098, 1139,
  1184, 1232, 1282, 1336,
};
static const int16_t dc_qlookup_10_QTX[AV1_QINDEX_RANGE] = {
  4,    9,    10,   13,   15,   17,   20,   22,   25,   28,   31,   34,   37,
  40,   43,   47,   50,   53,   57,   60,   64,   68,   71,   75,   78,   82,
  86,   90,   93,   97,   101,  105,  109,  113,  116,  120,  124,  128,  132,
  136,  140,  143,  147,  151,  155,  159,  163,  166,  170,  174,  178,  182,
  185,  189,  193,  197,  200,  204,  208,  212,  215,  219,  223,  226,  230,
  233,  237,  241,  244,  248,  251,  255,  259,  262,  266,  269,  273,  276,
  280,  283,  287,  290,  293,  297,  300,  304,  307,  310,  314,  317,  321,
  324,  327,  331,  334,  337,  343,  350,  356,  362,  369,  375,  381,  387,
  394,  400,  406,  412,  418,  424,  430,  436,  442,  448,  454,  460,  466,
  472,  478,  484,  490,  499,  507,  516,  525,  533,  542,  550,  559,  567,
  576,  584,  592,  601,  609,  617,  625,  634,  644,  655,  666,  676,  687,
  698,  708,  718,  729,  739,  749,  759,  770,  782,  795,  807,  819,  831,
  844,  856,  868,  880,  891,  906,  920,  933,  947,  961,  975,  988,  1001,
  1015, 1030, 1045, 1061, 1076, 1090, 1105, 1120, 1137, 1153, 1170, 1186, 1202,
  1218, 1236, 1253, 1271, 1288, 1306, 1323, 1342, 1361, 1379, 1398, 1416, 1436,
  1456, 1476, 1496, 1516, 1537, 1559, 1580, 1601, 1624, 1647, 1670, 1692, 1717,
  1741, 1766, 1791, 1817, 1844, 1871, 1900, 1929, 1958, 1990, 2021, 2054, 2088,
  2123, 2159, 2197, 2236, 2276, 2319, 2363, 2410, 2458, 2508, 2561, 2616, 2675,
  2737, 2802, 2871, 2944, 3020, 3102, 3188, 3280, 3375, 3478, 3586, 3702, 3823,
  3953, 4089, 4236, 4394, 4559, 4737, 4929, 5130, 5347,
};
static const int16_t dc_qlookup_12_QTX[AV1_QINDEX_RANGE] = {
  4,     12,    18,    25,    33,    41,    50,    60,    70,    80,    91,
  103,   115,   127,   140,   153,   166,   180,   194,   208,   222,   237,
  251,   266,   281,   296,   312,   327,   343,   358,   374,   390,   405,
  421,   437,   453,   469,   484,   500,   516,   532,   548,   564,   580,
  596,   611,   627,   643,   659,   674,   690,   706,   721,   737,   752,
  768,   783,   798,   814,   829,   844,   859,   874,   889,   904,   919,
  934,   949,   964,   978,   993,   1008,  1022,  1037,  1051,  1065,  1080,
  1094,  1108,  1122,  1136,  1151,  1165,  1179,  1192,  1206,  1220,  1234,
  1248,  1261,  1275,  1288,  1302,  1315,  1329,  1342,  1368,  1393,  1419,
  1444,  1469,  1494,  1519,  1544,  1569,  1594,  1618,  1643,  1668,  1692,
  1717,  1741,  1765,  1789,  1814,  1838,  1862,  1885,  1909,  1933,  1957,
  1992,  2027,  2061,  2096,  2130,  2165,  2199,  2233,  2267,  2300,  2334,
  2367,  2400,  2434,  2467,  2499,  2532,  2575,  2618,  2661,  2704,  2746,
  2788,  2830,  2872,  2913,  2954,  2995,  3036,  3076,  3127,  3177,  3226,
  3275,  3324,  3373,  3421,  3469,  3517,  3565,  3621,  3677,  3733,  3788,
  3843,  3897,  3951,  4005,  4058,  4119,  4181,  4241,  4301,  4361,  4420,
  4479,  4546,  4612,  4677,  4742,  4807,  4871,  4942,  5013,  5083,  5153,
  5222,  5291,  5367,  5442,  5517,  5591,  5665,  5745,  5825,  5905,  5984,
  6063,  6149,  6234,  6319,  6404,  6495,  6587,  6678,  6769,  6867,  6966,
  7064,  7163,  7269,  7376,  7483,  7599,  7715,  7832,  7958,  8085,  8214,
  8352,  8492,  8635,  8788,  8945,  9104,  9275,  9450,  9639,  9832,  10031,
  10245, 10465, 10702, 10946, 11210, 11482, 11776, 12081, 12409, 12750, 13118,
  13501, 13913, 14343, 14807, 15290, 15812, 16356, 16943, 17575, 18237, 18949,
  19718, 20521, 21387,
};
static const int16_t ac_qlookup_QTX[AV1_QINDEX_RANGE] = {
  4,    8,    9,    10,   11,   12,   13,   14,   15,   16,   17,   18,   19,
  20,   21,   22,   23,   24,   25,   26,   27,   28,   29,   30,   31,   32,
  33,   34,   35,   36,   37,   38,   39,   40,   41,   42,   43,   44,   45,
  46,   47,   48,   49,   50,   51,   52,   53,   54,   55,   56,   57,   58,
  59,   60,   61,   62,   63,   64,   65,   66,   67,   68,   69,   70,   71,
  72,   73,   74,   75,   76,   77,   78,   79,   80,   81,   82,   83,   84,
  85,   86,   87,   88,   89,   90,   91,   92,   93,   94,   95,   96,   97,
  98,   99,   100,  101,  102,  104,  106,  108,  110,  112,  114,  116,  118,
  120,  122,  124,  126,  128,  130,  132,  134,  136,  138,  140,  142,  144,
  146,  148,  150,  152,  155,  158,  161,  164,  167,  170,  173,  176,  179,
  182,  185,  188,  191,  194,  197,  200,  203,  207,  211,  215,  219,  223,
  227,  231,  235,  239,  243,  247,  251,  255,  260,  265,  270,  275,  280,
  285,  290,  295,  300,  305,  311,  317,  323,  329,  335,  341,  347,  353,
  359,  366,  373,  380,  387,  394,  401,  408,  416,  424,  432,  440,  448,
  456,  465,  474,  483,  492,  501,  510,  520,  530,  540,  550,  560,  571,
  582,  593,  604,  615,  627,  639,  651,  663,  676,  689,  702,  715,  729,
  743,  757,  771,  786,  801,  816,  832,  848,  864,  881,  898,  915,  933,
  951,  969,  988,  1007, 1026, 1046, 1066, 1087, 1108, 1129, 1151, 1173, 1196,
  1219, 1243, 1267, 1292, 1317, 1343, 1369, 1396, 1423, 1451, 1479, 1508, 1537,
  1567, 1597, 1628, 1660, 1692, 1725, 1759, 1793, 1828,
};
static const int16_t ac_qlookup_10_QTX[AV1_QINDEX_RANGE] = {
  4,    9,    11,   13,   16,   18,   21,   24,   27,   30,   33,   37,   40,
  44,   48,   51,   55,   59,   63,   67,   71,   75,   79,   83,   88,   92,
  96,   100,  105,  109,  114,  118,  122,  127,  131,  136,  140,  145,  149,
  154,  158,  163,  168,  172,  177,  181,  186,  190,  195,  199,  204,  208,
  213,  217,  222,  226,  231,  235,  240,  244,  249,  253,  258,  262,  267,
  271,  275,  280,  284,  289,  293,  297,  302,  306,  311,  315,  319,  324,
  328,  332,  337,  341,  345,  349,  354,  358,  362,  367,  371,  375,  379,
  384,  388,  392,  396,  401,  409,  417,  425,  433,  441,  449,  458,  466,
  474,  482,  490,  498,  506,  514,  523,  531,  539,  547,  555,  563,  571,
  579,  588,  596,  604,  616,  628,  640,  652,  664,  676,  688,  700,  713,
  725,  737,  749,  761,  773,  785,  797,  809,  825,  841,  857,  873,  889,
  905,  922,  938,  954,  970,  986,  1002, 1018, 1038, 1058, 1078, 1098, 1118,
  1138, 1158, 1178, 1198, 1218, 1242, 1266, 1290, 1314, 1338, 1362, 1386, 1411,
  1435, 1463, 1491, 1519, 1547, 1575, 1603, 1631, 1663, 1695, 1727, 1759, 1791,
  1823, 1859, 1895, 1931, 1967, 2003, 2039, 2079, 2119, 2159, 2199, 2239, 2283,
  2327, 2371, 2415, 2459, 2507, 2555, 2603, 2651, 2703, 2755, 2807, 2859, 2915,
  2971, 3027, 3083, 3143, 3203, 3263, 3327, 3391, 3455, 3523, 3591, 3659, 3731,
  3803, 3876, 3952, 4028, 4104, 4184, 4264, 4348, 4432, 4516, 4604, 4692, 4784,
  4876, 4972, 5068, 5168, 5268, 5372, 5476, 5584, 5692, 5804, 5916, 6032, 6148,
  6268, 6388, 6512, 6640, 6768, 6900, 7036, 7172, 7312,
};
static const int16_t ac_qlookup_12_QTX[AV1_QINDEX_RANGE] = {
  4,     13,    19,    27,    35,    44,    54,    64,    75,    87,    99,
  112,   126,   139,   154,   168,   183,   199,   214,   230,   247,   263,
  280,   297,   314,   331,   349,   366,   384,   402,   420,   438,   456,
  475,   493,   511,   530,   548,   567,   586,   604,   623,   642,   660,
  679,   698,   716,   735,   753,   772,   791,   809,   828,   846,   865,
  884,   902,   920,   939,   957,   976,   994,   1012,  1030,  1049,  1067,
  1085,  1103,  1121,  1139,  1157,  1175,  1193,  1211,  1229,  1246,  1264,
  1282,  1299,  1317,  1335,  1352,  1370,  1387,  1405,  1422,  1440,  1457,
  1474,  1491,  1509,  1526,  1543,  1560,  1577,  1595,  1627,  1660,  1693,
  1725,  1758,  1791,  1824,  1856,  1889,  1922,  1954,  1987,  2020,  2052,
  2085,  2118,  2150,  2183,  2216,  2248,  2281,  2313,  2346,  2378,  2411,
  2459,  2508,  2556,  2605,  2653,  2701,  2750,  2798,  2847,  2895,  2943,
  2992,  3040,  3088,  3137,  3185,  3234,  3298,  3362,  3426,  3491,  3555,
  3619,  3684,  3748,  3812,  3876,  3941,  4005,  4069,  4149,  4230,  4310,
  4390,  4470,  4550,  4631,  4711,  4791,  4871,  4967,  5064,  5160,  5256,
  5352,  5448,  5544,  5641,  5737,  5849,  5961,  6073,  6185,  6297,  6410,
  6522,  6650,  6778,  6906,  7034,  7162,  7290,  7435,  7579,  7723,  7867,
  8011,  8155,  8315,  8475,  8635,  8795,  8956,  9132,  9308,  9484,  9660,
  9836,  10028, 10220, 10412, 10604, 10812, 11020, 11228, 11437, 11661, 11885,
  12109, 12333, 12573, 12813, 13053, 13309, 13565, 13821, 14093, 14365, 14637,
  14925, 15213, 15502, 15806, 16110, 16414, 16734, 17054, 17390, 17726, 18062,
  18414, 18766, 19134, 19502, 19886, 20270, 20670, 21070, 21486, 21902, 22334,
  22766, 23214, 23662, 24126, 24590, 25070, 25551, 26047, 26559, 27071, 27599,
  28143, 28687, 29247,
};

// Table that converts 0-63 Q-range values passed in outside to the Qindex
// range used internally.
static const int quantizer_to_qindex[] = {
  0,   4,   8,   12,  16,  20,  24,  28,  32,  36,  40,  44,  48,
  52,  56,  60,  64,  68,  72,  76,  80,  84,  88,  92,  96,  100,
  104, 108, 112, 116, 120, 124, 128, 132, 136, 140, 144, 148, 152,
  156, 160, 164, 168, 172, 176, 180, 184, 188, 192, 196, 200, 204,
  208, 212, 216, 220, 224, 228, 232, 236, 240, 244, 249, 255,
};

static inline int clamp(int value, int low, int high) {
  return value < low ? low : (value > high ? high : value);
}

static inline double fclamp(double value, double low, double high) {
  return value < low ? low : (value > high ? high : value);
}

int av1_frame_is_intra_only(const AV1_COMMON *cm) {
  return cm->current_frame.frame_type == AV1_KEY_FRAME ||
         cm->current_frame.frame_type == AV1_INTRA_ONLY_FRAME;
}

// How many times less pixels there are to encode given the current scaling.
// Temporary replacement for rcf_mult and rate_thresh_mult.
static double resize_rate_factor(const FrameDimensionCfg *const frm_dim_cfg,
                                 int width, int height) {
  return (double)(frm_dim_cfg->width * frm_dim_cfg->height) / (width * height);
}

// Functions to compute the active minq lookup table entries based on a
// formulaic approach to facilitate easier adjustment of the Q tables.
// The formulae were derived from computing a 3rd order polynomial best
// fit to the original data (after plotting real maxq vs minq (not q index))
static int get_minq_index(double maxq, double x3, double x2, double x1,
                          aom_bit_depth_t bit_depth) {
  const double minqtarget = AOMMIN(((x3 * maxq + x2) * maxq + x1) * maxq, maxq);

  // Special case handling to deal with the step from q2.0
  // down to lossless mode represented by q 1.0.
  if (minqtarget <= 2.0) return 0;

  return av1_find_qindex(minqtarget, bit_depth, 0, AV1_QINDEX_RANGE - 1);
}

static void init_minq_luts(int *kf_low_m, int *kf_high_m, int *arfgf_low,
                           int *arfgf_high, int *inter, int *rtc,
                           aom_bit_depth_t bit_depth) {
  int i;
  for (i = 0; i < AV1_QINDEX_RANGE; i++) {
    const double maxq = av1_convert_qindex_to_q(i, bit_depth);
    kf_low_m[i] = get_minq_index(maxq, 0.000001, -0.0004, 0.150, bit_depth);
    kf_high_m[i] = get_minq_index(maxq, 0.0000021, -0.00125, 0.45, bit_depth);
    arfgf_low[i] = get_minq_index(maxq, 0.0000015, -0.0009, 0.30, bit_depth);
    arfgf_high[i] = get_minq_index(maxq, 0.0000021, -0.00125, 0.55, bit_depth);
    inter[i] = get_minq_index(maxq, 0.00000271, -0.00113, 0.90, bit_depth);
    rtc[i] = get_minq_index(maxq, 0.00000271, -0.00113, 0.70, bit_depth);
  }
}

void av1_rc_init_minq_luts(void) {
  init_minq_luts(kf_low_motion_minq_8, kf_high_motion_minq_8,
                 arfgf_low_motion_minq_8, arfgf_high_motion_minq_8,
                 inter_minq_8, rtc_minq_8, AOM_BITS_8);
  init_minq_luts(kf_low_motion_minq_10, kf_high_motion_minq_10,
                 arfgf_low_motion_minq_10, arfgf_high_motion_minq_10,
                 inter_minq_10, rtc_minq_10, AOM_BITS_10);
  init_minq_luts(kf_low_motion_minq_12, kf_high_motion_minq_12,
                 arfgf_low_motion_minq_12, arfgf_high_motion_minq_12,
                 inter_minq_12, rtc_minq_12, AOM_BITS_12);
}

int16_t av1_dc_quant_QTX(int qindex, int delta, aom_bit_depth_t bit_depth) {
  const int q_clamped = clamp(qindex + delta, 0, AV1_MAXQ);
  switch (bit_depth) {
    case AOM_BITS_8: return dc_qlookup_QTX[q_clamped];
    case AOM_BITS_10: return dc_qlookup_10_QTX[q_clamped];
    case AOM_BITS_12: return dc_qlookup_12_QTX[q_clamped];
    default:
      assert(0 && "bit_depth should be AOM_BITS_8, AOM_BITS_10 or AOM_BITS_12");
      return -1;
  }
}

int16_t av1_ac_quant_QTX(int qindex, int delta, aom_bit_depth_t bit_depth) {
  const int q_clamped = clamp(qindex + delta, 0, AV1_MAXQ);
  switch (bit_depth) {
    case AOM_BITS_8: return ac_qlookup_QTX[q_clamped];
    case AOM_BITS_10: return ac_qlookup_10_QTX[q_clamped];
    case AOM_BITS_12: return ac_qlookup_12_QTX[q_clamped];
    default:
      assert(0 && "bit_depth should be AOM_BITS_8, AOM_BITS_10 or AOM_BITS_12");
      return -1;
  }
}


// These functions use formulaic calculations to make playing with the
// quantizer tables easier. If necessary they can be replaced by lookup
// tables if and when things settle down in the experimental bitstream
double av1_convert_qindex_to_q(int qindex, aom_bit_depth_t bit_depth) {
  // Convert the index to a real Q value (scaled down to match old Q values)
  switch (bit_depth) {
    case AOM_BITS_8: return av1_ac_quant_QTX(qindex, 0, bit_depth) / 4.0;
    case AOM_BITS_10: return av1_ac_quant_QTX(qindex, 0, bit_depth) / 16.0;
    case AOM_BITS_12: return av1_ac_quant_QTX(qindex, 0, bit_depth) / 64.0;
    default:
      assert(0 && "bit_depth should be AOM_BITS_8, AOM_BITS_10 or AOM_BITS_12");
      return -1.0;
  }
}


int av1_quantizer_to_qindex(int quantizer) {
  return quantizer_to_qindex[quantizer];
}

int av1_qindex_to_quantizer(int qindex) {
  int quantizer;

  for (quantizer = 0; quantizer < 64; ++quantizer)
    if (quantizer_to_qindex[quantizer] >= qindex) return quantizer;

  return 63;
}

int av1_rc_bits_per_mb(AV1_FRAME_TYPE frame_type, int qindex,
                       double correction_factor, aom_bit_depth_t bit_depth,
                       const int is_screen_content_type) {
  const double q = av1_convert_qindex_to_q(qindex, bit_depth);
  int enumerator = frame_type == AV1_KEY_FRAME ? 2000000 : 1500000;
  if (is_screen_content_type) {
    enumerator = frame_type == AV1_KEY_FRAME ? 1000000 : 750000;
  }

  assert(correction_factor <= MAX_BPB_FACTOR &&
         correction_factor >= MIN_BPB_FACTOR);

  // q based adjustment to baseline enumerator
  return (int)(enumerator * correction_factor / q);
}

int av1_estimate_bits_at_q(AV1_FRAME_TYPE frame_type, int q, int mbs,
                           double correction_factor, aom_bit_depth_t bit_depth,
                           const int is_screen_content_type) {
  const int bpm = (int)(av1_rc_bits_per_mb(frame_type, q, correction_factor,
                                           bit_depth, is_screen_content_type));
  return AOMMAX(FRAME_OVERHEAD_BITS,
                (int)((uint64_t)bpm * mbs) >> BPER_MB_NORMBITS);
}

int av1_rc_clamp_pframe_target_size(const AV1_COMP *const cpi, int target,
                                    FRAME_UPDATE_TYPE frame_update_type) {
  const AV1_RATE_CONTROL *rc = &cpi->rc;
  const AV1EncoderConfig *oxcf = &cpi->oxcf;
  const int min_frame_target =
      AOMMAX(rc->min_frame_bandwidth, rc->avg_frame_bandwidth >> 5);
  // Clip the frame target to the minimum setup value.
  if (frame_update_type == OVERLAY_UPDATE ||
      frame_update_type == INTNL_OVERLAY_UPDATE) {
    // If there is an active ARF at this location use the minimum
    // bits on this frame even if it is a constructed arf.
    // The active maximum quantizer insures that an appropriate
    // number of bits will be spent if needed for constructed ARFs.
    target = min_frame_target;
  } else if (target < min_frame_target) {
    target = min_frame_target;
  }

  // Clip the frame target to the maximum allowed value.
  if (target > rc->max_frame_bandwidth) target = rc->max_frame_bandwidth;
  if (oxcf->rc_cfg.max_inter_bitrate_pct) {
    const int max_rate =
        rc->avg_frame_bandwidth * oxcf->rc_cfg.max_inter_bitrate_pct / 100;
    target = AOMMIN(target, max_rate);
  }

  return target;
}

int av1_rc_clamp_iframe_target_size(const AV1_COMP *const cpi, int target) {
  const AV1_RATE_CONTROL *rc = &cpi->rc;
  const RateControlCfg *const rc_cfg = &cpi->oxcf.rc_cfg;
  if (rc_cfg->max_intra_bitrate_pct) {
    const int max_rate =
        rc->avg_frame_bandwidth * rc_cfg->max_intra_bitrate_pct / 100;
    target = AOMMIN(target, max_rate);
  }
  if (target > rc->max_frame_bandwidth) target = rc->max_frame_bandwidth;
  return target;
}

// Update the buffer level for higher temporal layers, given the encoded current
// temporal layer.
static void update_layer_buffer_level(AV1_SVC *svc, int encoded_frame_size) {
  const int current_temporal_layer = svc->temporal_layer_id;
  for (int i = current_temporal_layer + 1; i < svc->number_temporal_layers;
       ++i) {
    const int layer =
        LAYER_IDS_TO_IDX(svc->spatial_layer_id, i, svc->number_temporal_layers);
    AV1_LAYER_CONTEXT *lc = &svc->layer_context[layer];
    AV1_RATE_CONTROL *lrc = &lc->rc;
    lrc->bits_off_target +=
        (int)(lc->target_bandwidth / lc->framerate) - encoded_frame_size;
    // Clip buffer level to maximum buffer size for the layer.
    lrc->bits_off_target =
        AOMMIN(lrc->bits_off_target, lrc->maximum_buffer_size);
    lrc->buffer_level = lrc->bits_off_target;
  }
}
// Update the buffer level: leaky bucket model.
static void update_buffer_level(AV1_COMP *cpi, int encoded_frame_size) {
  const AV1_COMMON *const cm = &cpi->common;
  AV1_RATE_CONTROL *const rc = &cpi->rc;

  // Non-viewable frames are a special case and are treated as pure overhead.
  if (!cm->show_frame)
    rc->bits_off_target -= encoded_frame_size;
  else
    rc->bits_off_target += rc->avg_frame_bandwidth - encoded_frame_size;

  // Clip the buffer level to the maximum specified buffer size.
  rc->bits_off_target = AOMMIN(rc->bits_off_target, rc->maximum_buffer_size);
  rc->buffer_level = rc->bits_off_target;

  if (cpi->use_svc) update_layer_buffer_level(&cpi->svc, encoded_frame_size);
}

void av1_rc_init(const AV1EncoderConfig *oxcf, int pass, AV1_RATE_CONTROL *rc) {
  const RateControlCfg *const rc_cfg = &oxcf->rc_cfg;
  int i;

  if (pass == 0 && rc_cfg->mode == AOM_CBR) {
    rc->avg_frame_qindex[AV1_KEY_FRAME] = rc_cfg->worst_allowed_q;
    rc->avg_frame_qindex[AV1_INTER_FRAME] = rc_cfg->worst_allowed_q;
  } else {
    rc->avg_frame_qindex[AV1_KEY_FRAME] =
        (rc_cfg->worst_allowed_q + rc_cfg->best_allowed_q) / 2;
    rc->avg_frame_qindex[AV1_INTER_FRAME] =
        (rc_cfg->worst_allowed_q + rc_cfg->best_allowed_q) / 2;
  }

  rc->last_q[AV1_KEY_FRAME] = rc_cfg->best_allowed_q;
  rc->last_q[AV1_INTER_FRAME] = rc_cfg->worst_allowed_q;

  rc->buffer_level = rc->starting_buffer_level;
  rc->bits_off_target = rc->starting_buffer_level;

  rc->rolling_target_bits = rc->avg_frame_bandwidth;
  rc->rolling_actual_bits = rc->avg_frame_bandwidth;

  rc->total_actual_bits = 0;
  rc->total_target_bits = 0;

  rc->frames_since_key = 8;  // Sensible default for first frame.
  rc->this_key_frame_forced = 0;
  rc->next_key_frame_forced = 0;

  rc->frames_till_gf_update_due = 0;
  rc->ni_av_qi = rc_cfg->worst_allowed_q;
  rc->ni_tot_qi = 0;
  rc->ni_frames = 0;

  rc->tot_q = 0.0;
  rc->avg_q = av1_convert_qindex_to_q(rc_cfg->worst_allowed_q,
                                      AOM_BITS_8);//ToDo: Add dynamic control

  for (i = 0; i < AV1_RATE_FACTOR_LEVELS; ++i) {
    rc->rate_correction_factors[i] = 0.7;
  }
  rc->rate_correction_factors[AV1_KF_STD] = 1.0;
  rc->avg_frame_low_motion = 0;

  rc->resize_state = AV1_ORIG;
  rc->resize_avg_qp = 0;
  rc->resize_buffer_underflow = 0;
  rc->resize_count = 0;
}

int av1_rc_drop_frame(AV1_COMP *cpi) {
  const AV1EncoderConfig *oxcf = &cpi->oxcf;
  AV1_RATE_CONTROL *const rc = &cpi->rc;

  if (!oxcf->rc_cfg.drop_frames_water_mark) {
    return 0;
  } else {
    if (rc->buffer_level < 0) {
      // Always drop if buffer is below 0.
      return 1;
    } else {
      // If buffer is below drop_mark, for now just drop every other frame
      // (starting with the next frame) until it increases back over drop_mark.
      int drop_mark = (int)(oxcf->rc_cfg.drop_frames_water_mark *
                            rc->optimal_buffer_level / 100);
      if ((rc->buffer_level > drop_mark) && (rc->decimation_factor > 0)) {
        --rc->decimation_factor;
      } else if (rc->buffer_level <= drop_mark && rc->decimation_factor == 0) {
        rc->decimation_factor = 1;
      }
      if (rc->decimation_factor > 0) {
        if (rc->decimation_count > 0) {
          --rc->decimation_count;
          return 1;
        } else {
          rc->decimation_count = rc->decimation_factor;
          return 0;
        }
      } else {
        rc->decimation_count = 0;
        return 0;
      }
    }
  }
}

static int adjust_q_cbr(const AV1_COMP *cpi, int q, int active_worst_quality) {
  const AV1_RATE_CONTROL *const rc = &cpi->rc;
  const AV1_COMMON *const cm = &cpi->common;
  const int max_delta = 16;
  const int change_avg_frame_bandwidth =
      abs(rc->avg_frame_bandwidth - rc->prev_avg_frame_bandwidth) >
      0.1 * (rc->avg_frame_bandwidth);
  // If resolution changes or avg_frame_bandwidth significantly changed,
  // then set this flag to indicate change in target bits per macroblock.
  const int change_target_bits_mb =
      cm->prev_frame.has_prev_frame &&
      (cm->width != cm->prev_frame.width ||
       cm->height != cm->prev_frame.height || change_avg_frame_bandwidth);
  // Apply some control/clamp to QP under certain conditions.
  // ToDo (important): Add control to provide refresh_frame_flags
  if (cm->current_frame.frame_type != AV1_KEY_FRAME && !cpi->use_svc &&
      rc->frames_since_key > 1 && !change_target_bits_mb /*&&
      (!cpi->oxcf.rc_cfg.gf_cbr_boost_pct ||
       !(refresh_frame_flags->alt_ref_frame ||
         refresh_frame_flags->golden_frame))*/) {
    // Make sure q is between oscillating Qs to prevent resonance.
    if (rc->rc_1_frame * rc->rc_2_frame == -1 &&
        rc->q_1_frame != rc->q_2_frame) {
      q = clamp(q, AOMMIN(rc->q_1_frame, rc->q_2_frame),
                AOMMAX(rc->q_1_frame, rc->q_2_frame));
    }
    // Adjust Q base on source content change from scene detection.
    if (cpi->sf.rt_sf.check_scene_detection && rc->prev_avg_source_sad > 0 &&
        rc->frames_since_key > 10) {
      const int bit_depth = cm->seq_params.bit_depth;
      double delta =
          (double)rc->avg_source_sad / (double)rc->prev_avg_source_sad - 1.0;
      // Push Q downwards if content change is decreasing and buffer level
      // is stable (at least 1/4-optimal level), so not overshooting. Do so
      // only for high Q to avoid excess overshoot.
      // Else reduce decrease in Q from previous frame if content change is
      // increasing and buffer is below max (so not undershooting).
      if (delta < 0.0 && rc->buffer_level > (rc->optimal_buffer_level >> 2) &&
          q > (rc->worst_quality >> 1)) {
        double q_adj_factor = 1.0 + 0.5 * tanh(4.0 * delta);
        double q_val = av1_convert_qindex_to_q(q, bit_depth);
        q += av1_compute_qdelta(rc, q_val, q_val * q_adj_factor, bit_depth);
      } else if (rc->q_1_frame - q > 0 && delta > 0.1 &&
                 rc->buffer_level < AOMMIN(rc->maximum_buffer_size,
                                           rc->optimal_buffer_level << 1)) {
        q = (3 * q + rc->q_1_frame) >> 2;
      }
    }
    // Limit the decrease in Q from previous frame.
    if (rc->q_1_frame - q > max_delta) q = rc->q_1_frame - max_delta;
  }
  // For single spatial layer: if resolution has increased push q closer
  // to the active_worst to avoid excess overshoot.
  if (cpi->svc.number_spatial_layers <= 1 && cm->prev_frame.has_prev_frame &&
      (cm->width * cm->height >
       1.5 * cm->prev_frame.width * cm->prev_frame.height))
    q = (q + active_worst_quality) >> 1;
  return AOMMAX(AOMMIN(q, cpi->rc.worst_quality), cpi->rc.best_quality);
}

/*!\brief Gets a rate vs Q correction factor
 *
 * This function returns the current value of a correction factor used to
 * dynamilcally adjust the relationship between Q and the expected number
 * of bits for the frame.
 *
 * \ingroup rate_control
 * \param[in]   cpi                   Top level encoder instance structure
 * \param[in]   width                 Frame width
 * \param[in]   height                Frame height
 *
 * \return Returns a correction factor for the current frame
 */
static double get_rate_correction_factor(const AV1_COMP *cpi, int width,
                                         int height) {
  const AV1_RATE_CONTROL *const rc = &cpi->rc;
  double rcf;

  if (cpi->common.current_frame.frame_type == AV1_KEY_FRAME) {
    rcf = rc->rate_correction_factors[AV1_KF_STD];
  } /*else if (is_stat_consumption_stage(cpi)) {
     ToDo: Add support for lap & two stage encode
  } */ else {
    //ToDo: Add support for AV1_GF_ARF_STD case if gf_cbr_boost_pct >20
    rcf = rc->rate_correction_factors[AV1_INTER_NORMAL];
  }
  rcf *= resize_rate_factor(&cpi->oxcf.frm_dim_cfg, width, height);
  return fclamp(rcf, MIN_BPB_FACTOR, MAX_BPB_FACTOR);
}

/*!\brief Sets a rate vs Q correction factor
 *
 * This function updates the current value of a correction factor used to
 * dynamilcally adjust the relationship between Q and the expected number
 * of bits for the frame.
 *
 * \ingroup rate_control
 * \param[in]   cpi                   Top level encoder instance structure
 * \param[in]   factor                New correction factor
 * \param[in]   width                 Frame width
 * \param[in]   height                Frame height
 *
 * \return None but updates the rate correction factor for the
 *         current frame type in cpi->rc.
 */
static void set_rate_correction_factor(AV1_COMP *cpi, double factor, int width,
                                       int height) {
  AV1_RATE_CONTROL *const rc = &cpi->rc;

  // Normalize RCF to account for the size-dependent scaling factor.
  factor /= resize_rate_factor(&cpi->oxcf.frm_dim_cfg, width, height);

  factor = fclamp(factor, MIN_BPB_FACTOR, MAX_BPB_FACTOR);

  if (cpi->common.current_frame.frame_type == AV1_KEY_FRAME) {
    rc->rate_correction_factors[AV1_KF_STD] = factor;
  } /* else if (is_stat_consumption_stage(cpi)) {
    ToDo: Add support for lap & two stage encode
  } */else {
    //ToDo: Add support for AV1_GF_ARF_STD case if gf_cbr_boost_pct >20
    rc->rate_correction_factors[AV1_INTER_NORMAL] = factor;
  }
}

static int av1_get_MBs(int width, int height) {
  const int aligned_width = ALIGN_POWER_OF_TWO(width, 3);
  const int aligned_height = ALIGN_POWER_OF_TWO(height, 3);
  const int mi_cols = aligned_width >> MI_SIZE_LOG2;
  const int mi_rows = aligned_height >> MI_SIZE_LOG2;

  const int mb_cols = (mi_cols + 2) >> 2;
  const int mb_rows = (mi_rows + 2) >> 2;
  return mb_rows * mb_cols;
}

void av1_rc_update_rate_correction_factors(AV1_COMP *cpi, int width,
                                           int height) {
  const AV1_COMMON *const cm = &cpi->common;
  int correction_factor = 100;
  double rate_correction_factor =
      get_rate_correction_factor(cpi, width, height);
  double adjustment_limit;
  const int MBs = av1_get_MBs(width, height);

  int projected_size_based_on_q = 0;

  // Do not update the rate factors for arf overlay frames.
  if (cpi->rc.is_src_frame_alt_ref) return;

  // Work out how big we would have expected the frame to be at this Q given
  // the current correction factor.
  // Stay in double to avoid int overflow when values are large
  //ToDo: add support for CYCLIC_REFRESH_AQ
  projected_size_based_on_q = av1_estimate_bits_at_q(
        cm->current_frame.frame_type, cm->quant_params.base_qindex, MBs,
        rate_correction_factor, cm->seq_params.bit_depth,
        cpi->is_screen_content_type);
  // Work out a size correction factor.
  if (projected_size_based_on_q > FRAME_OVERHEAD_BITS)
    correction_factor = (int)((100 * (int64_t)cpi->rc.projected_frame_size) /
                              projected_size_based_on_q);

  // More heavily damped adjustment used if we have been oscillating either side
  // of target.
  if (correction_factor > 0) {
    adjustment_limit =
        0.25 + 0.5 * AOMMIN(1, fabs(log10(0.01 * correction_factor)));
  } else {
    adjustment_limit = 0.75;
  }

  cpi->rc.q_2_frame = cpi->rc.q_1_frame;
  cpi->rc.q_1_frame = cm->quant_params.base_qindex;
  cpi->rc.rc_2_frame = cpi->rc.rc_1_frame;
  if (correction_factor > 110)
    cpi->rc.rc_1_frame = -1;
  else if (correction_factor < 90)
    cpi->rc.rc_1_frame = 1;
  else
    cpi->rc.rc_1_frame = 0;

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

  set_rate_correction_factor(cpi, rate_correction_factor, width, height);
}

// Calculate rate for the given 'q'.
static int get_bits_per_mb(const AV1_COMP *cpi, int use_cyclic_refresh,
                           double correction_factor, int q) {
  const AV1_COMMON *const cm = &cpi->common;
  //ToDo: add support for av1_cyclic_refresh_rc_bits_per_mb
  return av1_rc_bits_per_mb(cm->current_frame.frame_type, q,
                                  correction_factor, cm->seq_params.bit_depth,
                                  cpi->is_screen_content_type);
}

/*!\brief Searches for a Q index value predicted to give an average macro
 * block rate closest to the target value.
 *
 * Similar to find_qindex_by_rate() function, but returns a q index with a
 * rate just above or below the desired rate, depending on which of the two
 * rates is closer to the desired rate.
 * Also, respects the selected aq_mode when computing the rate.
 *
 * \ingroup rate_control
 * \param[in]   desired_bits_per_mb   Target bits per mb
 * \param[in]   cpi                   Top level encoder instance structure
 * \param[in]   correction_factor     Current Q to rate correction factor
 * \param[in]   best_qindex           Min allowed Q value.
 * \param[in]   worst_qindex          Max allowed Q value.
 *
 * \return Returns a correction factor for the current frame
 */
static int find_closest_qindex_by_rate(int desired_bits_per_mb,
                                       const AV1_COMP *cpi,
                                       double correction_factor,
                                       int best_qindex, int worst_qindex) {
  const int use_cyclic_refresh = 0;

  // Find 'qindex' based on 'desired_bits_per_mb'.
  assert(best_qindex <= worst_qindex);
  int low = best_qindex;
  int high = worst_qindex;
  while (low < high) {
    const int mid = (low + high) >> 1;
    const int mid_bits_per_mb =
        get_bits_per_mb(cpi, use_cyclic_refresh, correction_factor, mid);
    if (mid_bits_per_mb > desired_bits_per_mb) {
      low = mid + 1;
    } else {
      high = mid;
    }
  }
  assert(low == high);

  // Calculate rate difference of this q index from the desired rate.
  const int curr_q = low;
  const int curr_bits_per_mb =
      get_bits_per_mb(cpi, use_cyclic_refresh, correction_factor, curr_q);
  const int curr_bit_diff = (curr_bits_per_mb <= desired_bits_per_mb)
                                ? desired_bits_per_mb - curr_bits_per_mb
                                : INT_MAX;
  assert((curr_bit_diff != INT_MAX && curr_bit_diff >= 0) ||
         curr_q == worst_qindex);

  // Calculate rate difference for previous q index too.
  const int prev_q = curr_q - 1;
  int prev_bit_diff;
  if (curr_bit_diff == INT_MAX || curr_q == best_qindex) {
    prev_bit_diff = INT_MAX;
  } else {
    const int prev_bits_per_mb =
        get_bits_per_mb(cpi, use_cyclic_refresh, correction_factor, prev_q);
    assert(prev_bits_per_mb > desired_bits_per_mb);
    prev_bit_diff = prev_bits_per_mb - desired_bits_per_mb;
  }

  // Pick one of the two q indices, depending on which one has rate closer to
  // the desired rate.
  return (curr_bit_diff <= prev_bit_diff) ? curr_q : prev_q;
}

int av1_rc_regulate_q(const AV1_COMP *cpi, int target_bits_per_frame,
                      int active_best_quality, int active_worst_quality,
                      int width, int height) {
  const int MBs = av1_get_MBs(width, height);
  const double correction_factor =
      get_rate_correction_factor(cpi, width, height);
  const int target_bits_per_mb =
      (int)(((uint64_t)target_bits_per_frame << BPER_MB_NORMBITS) / MBs);

  int q =
      find_closest_qindex_by_rate(target_bits_per_mb, cpi, correction_factor,
                                  active_best_quality, active_worst_quality);
  if (cpi->oxcf.rc_cfg.mode == AOM_CBR)
    return adjust_q_cbr(cpi, q, active_worst_quality);

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

static int get_kf_active_quality(const AV1_RATE_CONTROL *const rc, int q,
                                 aom_bit_depth_t bit_depth) {
  int *kf_low_motion_minq;
  int *kf_high_motion_minq;
  ASSIGN_MINQ_TABLE(bit_depth, kf_low_motion_minq);
  ASSIGN_MINQ_TABLE(bit_depth, kf_high_motion_minq);
  return get_active_quality(q, rc->kf_boost, kf_low, kf_high,
                            kf_low_motion_minq, kf_high_motion_minq);
}

// Adjust active_worst_quality level based on buffer level.
static int calc_active_worst_quality_no_stats_cbr(const AV1_COMP *cpi) {
  // Adjust active_worst_quality: If buffer is above the optimal/target level,
  // bring active_worst_quality down depending on fullness of buffer.
  // If buffer is below the optimal level, let the active_worst_quality go from
  // ambient Q (at buffer = optimal level) to worst_quality level
  // (at buffer = critical level).
  const AV1_COMMON *const cm = &cpi->common;
  const AV1_RATE_CONTROL *rc = &cpi->rc;
  // Buffer level below which we push active_worst to worst_quality.
  int64_t critical_level = rc->optimal_buffer_level >> 3;
  int64_t buff_lvl_step = 0;
  int adjustment = 0;
  int active_worst_quality;
  int ambient_qp;
  if (cm->current_frame.frame_type == AV1_KEY_FRAME) return rc->worst_quality;
  // For ambient_qp we use minimum of avg_frame_qindex[AV1_KEY_FRAME/AV1_INTER_FRAME]
  // for the first few frames following key frame. These are both initialized
  // to worst_quality and updated with (3/4, 1/4) average in postencode_update.
  // So for first few frames following key, the qp of that key frame is weighted
  // into the active_worst_quality setting.
  ambient_qp = (cm->current_frame.frame_number < 5)
                   ? AOMMIN(rc->avg_frame_qindex[AV1_INTER_FRAME],
                            rc->avg_frame_qindex[AV1_KEY_FRAME])
                   : rc->avg_frame_qindex[AV1_INTER_FRAME];
  active_worst_quality = AOMMIN(rc->worst_quality, ambient_qp * 5 / 4);
  if (rc->buffer_level > rc->optimal_buffer_level) {
    // Adjust down.
    // Maximum limit for down adjustment, ~30%.
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

// Calculate the active_best_quality level.
static int calc_active_best_quality_no_stats_cbr(const AV1_COMP *cpi,
                                                 int active_worst_quality,
                                                 int width, int height) {
  const AV1_COMMON *const cm = &cpi->common;
  const AV1_RATE_CONTROL *const rc = &cpi->rc;
  const CurrentFrame *const current_frame = &cm->current_frame;
  int *rtc_minq;
  const int bit_depth = cm->seq_params.bit_depth;
  int active_best_quality = rc->best_quality;
  ASSIGN_MINQ_TABLE(bit_depth, rtc_minq);

  if (av1_frame_is_intra_only(cm)) {
    // Handle the special case for key frames forced when we have reached
    // the maximum key frame interval. Here force the Q to a range
    // based on the ambient Q to reduce the risk of popping.
    if (rc->this_key_frame_forced) {
      int qindex = rc->last_boosted_qindex;
      double last_boosted_q = av1_convert_qindex_to_q(qindex, bit_depth);
      int delta_qindex = av1_compute_qdelta(rc, last_boosted_q,
                                            (last_boosted_q * 0.75), bit_depth);
      active_best_quality = AOMMAX(qindex + delta_qindex, rc->best_quality);
    } else if (current_frame->frame_number > 0) {
      // not first frame of one pass and kf_boost is set
      double q_adj_factor = 1.0;
      double q_val;
      active_best_quality =
          get_kf_active_quality(rc, rc->avg_frame_qindex[AV1_KEY_FRAME], bit_depth);
      // Allow somewhat lower kf minq with small image formats.
      if ((width * height) <= (352 * 288)) {
        q_adj_factor -= 0.25;
      }
      // Convert the adjustment factor to a qindex delta
      // on active_best_quality.
      q_val = av1_convert_qindex_to_q(active_best_quality, bit_depth);
      active_best_quality +=
          av1_compute_qdelta(rc, q_val, q_val * q_adj_factor, bit_depth);
    }
  } /* else if (!rc->is_src_frame_alt_ref && !cpi->use_svc &&
             cpi->oxcf.rc_cfg.gf_cbr_boost_pct &&
             (refresh_frame_flags->golden_frame ||
              refresh_frame_flags->alt_ref_frame)) {
	      //ToDo: Add support for the special case
  } */else {
    // Use the lower of active_worst_quality and recent/average Q.
    AV1_FRAME_TYPE frame_type =
        (current_frame->frame_number > 1) ? AV1_INTER_FRAME : AV1_KEY_FRAME;
    if (rc->avg_frame_qindex[frame_type] < active_worst_quality)
      active_best_quality = rtc_minq[rc->avg_frame_qindex[frame_type]];
    else
      active_best_quality = rtc_minq[active_worst_quality];
  }
  return active_best_quality;
}

/*!\brief Picks q and q bounds given CBR rate control parameters in \c cpi->rc.
 *
 * Handles the special case when using:
 * - Constant bit-rate mode: \c cpi->oxcf.rc_cfg.mode == \ref AOM_CBR, and
 * - 1-pass encoding without LAP (look-ahead processing), so 1st pass stats are
 * NOT available.
 *
 * \ingroup rate_control
 * \param[in]       cpi          Top level encoder structure
 * \param[in]       width        Coded frame width
 * \param[in]       height       Coded frame height
 * \param[out]      bottom_index Bottom bound for q index (best quality)
 * \param[out]      top_index    Top bound for q index (worst quality)
 * \return Returns selected q index to be used for encoding this frame.
 */
static int rc_pick_q_and_bounds_no_stats_cbr(const AV1_COMP *cpi, int width,
                                             int height, int *bottom_index,
                                             int *top_index) {
  const AV1_COMMON *const cm = &cpi->common;
  const AV1_RATE_CONTROL *const rc = &cpi->rc;
  const CurrentFrame *const current_frame = &cm->current_frame;
  int q;
  const int bit_depth = cm->seq_params.bit_depth;
  int active_worst_quality = calc_active_worst_quality_no_stats_cbr(cpi);
  int active_best_quality = calc_active_best_quality_no_stats_cbr(
      cpi, active_worst_quality, width, height);
  assert(cpi->oxcf.rc_cfg.mode == AOM_CBR);

  // Clip the active best and worst quality values to limits
  active_best_quality =
      clamp(active_best_quality, rc->best_quality, rc->worst_quality);
  active_worst_quality =
      clamp(active_worst_quality, active_best_quality, rc->worst_quality);

  *top_index = active_worst_quality;
  *bottom_index = active_best_quality;

  // Limit Q range for the adaptive loop.
  if (current_frame->frame_type == AV1_KEY_FRAME && !rc->this_key_frame_forced &&
      current_frame->frame_number != 0) {
    int qdelta = 0;
    qdelta = av1_compute_qdelta_by_rate(&cpi->rc, current_frame->frame_type,
                                        active_worst_quality, 2.0,
                                        cpi->is_screen_content_type, bit_depth);
    *top_index = active_worst_quality + qdelta;
    *top_index = AOMMAX(*top_index, *bottom_index);
  }

  // Special case code to try and match quality with forced key frames
  if (current_frame->frame_type == AV1_KEY_FRAME && rc->this_key_frame_forced) {
    q = rc->last_boosted_qindex;
  } else {
    q = av1_rc_regulate_q(cpi, rc->this_frame_target, active_best_quality,
                          active_worst_quality, width, height);
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

//derived from aom's av1_rc_pick_q_and_bounds()
int av1_rc_pick_q_and_bounds(const AV1_COMP *cpi, AV1_RATE_CONTROL *rc, int width,
                             int height, /* int gf_index,*/ int *bottom_index,
                             int *top_index) {
  int q;
  q = rc_pick_q_and_bounds_no_stats_cbr(cpi, width, height, bottom_index,
                                            top_index);
  //ToDo
  //Add look ahead processing support
  return q;
}
//Fixme: Check the req for svc enablement?
#if 0
void av1_check_initial_width(AV1_COMP *cpi, int use_highbitdepth,
                             int subsampling_x, int subsampling_y) {
 AV1_COMMON *const cm = &cpi->common;
  SequenceHeader *const seq_params = &cm->seq_params;
  InitialDimensions *const initial_dimensions = &cpi->initial_dimensions;

  if (!initial_dimensions->width ||
      seq_params->use_highbitdepth != use_highbitdepth ||
      seq_params->subsampling_x != subsampling_x ||
      seq_params->subsampling_y != subsampling_y) {
    seq_params->subsampling_x = subsampling_x;
    seq_params->subsampling_y = subsampling_y;
    seq_params->use_highbitdepth = use_highbitdepth;

    av1_set_speed_features_framesize_independent(cpi, cpi->oxcf.speed);
    av1_set_speed_features_framesize_dependent(cpi, cpi->oxcf.speed);

    if (!is_stat_generation_stage(cpi)) {
      alloc_altref_frame_buffer(cpi);
      alloc_util_frame_buffers(cpi);
    }
    init_ref_frame_bufs(cpi);
    init_motion_estimation(cpi);  // TODO(agrange) This can be removed.

    initial_dimensions->width = cm->width;
    initial_dimensions->height = cm->height;
    cpi->initial_mbs = cm->mi_params.MBs;
  }
}

// Returns 1 if the assigned width or height was <= 0.
int av1_set_size_literal(AV1_COMP *cpi, int width, int height) {
  AV1_COMMON *cm = &cpi->common;
  InitialDimensions *const initial_dimensions = &cpi->initial_dimensions;
  av1_check_initial_width(cpi, cm->seq_params.use_highbitdepth,
                          cm->seq_params.subsampling_x,
                          cm->seq_params.subsampling_y);
  
  if (width <= 0 || height <= 0) return 1;
  
  cm->width = width;   
  cm->height = height;
  
  if (initial_dimensions->width && initial_dimensions->height &&
      (cm->width > initial_dimensions->width ||
       cm->height > initial_dimensions->height)) {
    initial_dimensions->width = initial_dimensions->height = 0;
  }
  update_frame_size(cpi);

  return 0;
}
#endif
void av1_rc_compute_frame_size_bounds(const AV1_COMP *cpi, int frame_target,
                                      int *frame_under_shoot_limit,
                                      int *frame_over_shoot_limit) {
  if (cpi->oxcf.rc_cfg.mode == AOM_Q) {
    *frame_under_shoot_limit = 0;
    *frame_over_shoot_limit = INT_MAX;
  } else {
    // For very small rate targets where the fractional adjustment
    // may be tiny make sure there is at least a minimum range.
    assert(cpi->sf.hl_sf.recode_tolerance <= 100);
    const int tolerance = (int)AOMMAX(
        100, ((int64_t)cpi->sf.hl_sf.recode_tolerance * frame_target) / 100);
    *frame_under_shoot_limit = AOMMAX(frame_target - tolerance, 0);
    *frame_over_shoot_limit =
        AOMMIN(frame_target + tolerance, cpi->rc.max_frame_bandwidth);
  }
}

void av1_rc_set_frame_target(AV1_COMP *cpi, int target, int width, int height) {
  AV1_RATE_CONTROL *const rc = &cpi->rc;

  rc->this_frame_target = target;

  // Target rate per SB64 (including partial SB64s.
  rc->sb64_target_rate =
      (int)(((int64_t)rc->this_frame_target << 12) / (width * height));
}

void av1_rc_postencode_update(AV1_COMP *cpi, uint64_t bytes_used) {
  const AV1_COMMON *const cm = &cpi->common;
  const CurrentFrame *const current_frame = &cm->current_frame;
  AV1_RATE_CONTROL *const rc = &cpi->rc;

  const int qindex = cm->quant_params.base_qindex;

  // Update rate control heuristics
  rc->projected_frame_size = (int)(bytes_used << 3);

  // Post encode loop adjustment of Q prediction.
  av1_rc_update_rate_correction_factors(cpi, cm->width, cm->height);

  //Fixme(Important): make else case default for all p frames in non-svc case???
  //
  // Keep a record of last Q and ambient average Q.
  if (current_frame->frame_type == AV1_KEY_FRAME) {
    rc->last_q[AV1_KEY_FRAME] = qindex;
    rc->avg_frame_qindex[AV1_KEY_FRAME] =
        ROUND_POWER_OF_TWO(3 * rc->avg_frame_qindex[AV1_KEY_FRAME] + qindex, 2);
  } else {
    //ToDo: expose refresh_frame_flags to user
    if ((cpi->use_svc && cpi->oxcf.rc_cfg.mode == AOM_CBR) /* ||
        (!rc->is_src_frame_alt_ref &&
         !(refresh_frame_flags->golden_frame || is_intrnl_arf ||
           refresh_frame_flags->alt_ref_frame))*/) {
      rc->last_q[AV1_INTER_FRAME] = qindex;
      rc->avg_frame_qindex[AV1_INTER_FRAME] =
          ROUND_POWER_OF_TWO(3 * rc->avg_frame_qindex[AV1_INTER_FRAME] + qindex, 2);
      rc->ni_frames++;
      rc->tot_q += av1_convert_qindex_to_q(qindex, cm->seq_params.bit_depth);
      rc->avg_q = rc->tot_q / rc->ni_frames;
      // Calculate the average Q for normal inter frames (not key or GFU
      // frames).
      rc->ni_tot_qi += qindex;
      rc->ni_av_qi = rc->ni_tot_qi / rc->ni_frames;
    }
  }

  //ToDo: Add refresh_frame_flag support
  // Keep record of last boosted (KF/GF/ARF) Q value.
  // If the current frame is coded at a lower Q then we also update it.
  // If all mbs in this group are skipped only update if the Q value is
  // better than that already stored.
  // This is used to help set quality in forced key frames to reduce popping
  if ((qindex < rc->last_boosted_qindex) ||
      (current_frame->frame_type == AV1_KEY_FRAME) /*||
      (!rc->constrained_gf_group &&
       (refresh_frame_flags->alt_ref_frame || is_intrnl_arf ||
        (refresh_frame_flags->golden_frame && !rc->is_src_frame_alt_ref)))*/) {
    rc->last_boosted_qindex = qindex;
  }
  if (current_frame->frame_type == AV1_KEY_FRAME) rc->last_kf_qindex = qindex;

  update_buffer_level(cpi, rc->projected_frame_size);
  rc->prev_avg_frame_bandwidth = rc->avg_frame_bandwidth;

  //Fixme: Important for multi-res videos?
  // Rolling monitors of whether we are over or underspending used to help
  // regulate min and Max Q in two pass.
  //if (av1_frame_scaled(cm))
  //  rc->this_frame_target = (int)(rc->this_frame_target /
  //                               resize_rate_factor(&cpi->oxcf.frm_dim_cfg,
  //                                                   cm->width, cm->height));
  if (current_frame->frame_type != AV1_KEY_FRAME) {
    rc->rolling_target_bits = (int)ROUND_POWER_OF_TWO_64(
        rc->rolling_target_bits * 3 + rc->this_frame_target, 2);
    rc->rolling_actual_bits = (int)ROUND_POWER_OF_TWO_64(
        rc->rolling_actual_bits * 3 + rc->projected_frame_size, 2);
  }

  // Actual bits spent
  rc->total_actual_bits += rc->projected_frame_size;
  rc->total_target_bits += cm->show_frame ? rc->avg_frame_bandwidth : 0;

  if (current_frame->frame_type == AV1_KEY_FRAME) rc->frames_since_key = 0;
  // if (current_frame->frame_number == 1 && cm->show_frame)
  /*
  rc->this_frame_target =
      (int)(rc->this_frame_target / resize_rate_factor(&cpi->oxcf.frm_dim_cfg,
  cm->width, cm->height));
      */
}

void av1_rc_postencode_update_drop_frame(AV1_COMP *cpi) {
  // Update buffer level with zero size, update frame counters, and return.
  update_buffer_level(cpi, 0);
  cpi->rc.frames_since_key++;
  cpi->rc.frames_to_key--;
  cpi->rc.rc_2_frame = 0;
  cpi->rc.rc_1_frame = 0;
}

int av1_find_qindex(double desired_q, aom_bit_depth_t bit_depth,
                    int best_qindex, int worst_qindex) {
  assert(best_qindex <= worst_qindex);
  int low = best_qindex;
  int high = worst_qindex;
  while (low < high) {
    const int mid = (low + high) >> 1;
    const double mid_q = av1_convert_qindex_to_q(mid, bit_depth);
    if (mid_q < desired_q) {
      low = mid + 1;
    } else {
      high = mid;
    }
  }
  assert(low == high);
  assert(av1_convert_qindex_to_q(low, bit_depth) >= desired_q ||
         low == worst_qindex);
  return low;
}

int av1_compute_qdelta(const AV1_RATE_CONTROL *rc, double qstart, double qtarget,
                       aom_bit_depth_t bit_depth) {
  const int start_index =
      av1_find_qindex(qstart, bit_depth, rc->best_quality, rc->worst_quality);
  const int target_index =
      av1_find_qindex(qtarget, bit_depth, rc->best_quality, rc->worst_quality);
  return target_index - start_index;
}

// Find q_index for the desired_bits_per_mb, within [best_qindex, worst_qindex],
// assuming 'correction_factor' is 1.0.
// To be precise, 'q_index' is the smallest integer, for which the corresponding
// bits per mb <= desired_bits_per_mb.
// If no such q index is found, returns 'worst_qindex'.
static int find_qindex_by_rate(int desired_bits_per_mb,
                               aom_bit_depth_t bit_depth, AV1_FRAME_TYPE frame_type,
                               const int is_screen_content_type,
                               int best_qindex, int worst_qindex) {
  assert(best_qindex <= worst_qindex);
  int low = best_qindex;
  int high = worst_qindex;
  while (low < high) {
    const int mid = (low + high) >> 1;
    const int mid_bits_per_mb = av1_rc_bits_per_mb(
        frame_type, mid, 1.0, bit_depth, is_screen_content_type);
    if (mid_bits_per_mb > desired_bits_per_mb) {
      low = mid + 1;
    } else {
      high = mid;
    }
  }
  assert(low == high);
  assert(av1_rc_bits_per_mb(frame_type, low, 1.0, bit_depth,
                            is_screen_content_type) <= desired_bits_per_mb ||
         low == worst_qindex);
  return low;
}

int av1_compute_qdelta_by_rate(const AV1_RATE_CONTROL *rc, AV1_FRAME_TYPE frame_type,
                               int qindex, double rate_target_ratio,
                               const int is_screen_content_type,
                               aom_bit_depth_t bit_depth) {
  // Look up the current projected bits per block for the base index
  const int base_bits_per_mb = av1_rc_bits_per_mb(
      frame_type, qindex, 1.0, bit_depth, is_screen_content_type);

  // Find the target bits per mb based on the base value and given ratio.
  const int target_bits_per_mb = (int)(rate_target_ratio * base_bits_per_mb);

  const int target_index = find_qindex_by_rate(
      target_bits_per_mb, bit_depth, frame_type, is_screen_content_type,
      rc->best_quality, rc->worst_quality);
  return target_index - qindex;
}

void av1_rc_update_framerate(AV1_COMP *cpi, int width, int height) {
  const AV1EncoderConfig *const oxcf = &cpi->oxcf;
  AV1_RATE_CONTROL *const rc = &cpi->rc;
  int vbr_max_bits;
  const int MBs = av1_get_MBs(width, height);

  rc->avg_frame_bandwidth =
      (int)(oxcf->rc_cfg.target_bandwidth / cpi->framerate);
  rc->min_frame_bandwidth =
      (int)(rc->avg_frame_bandwidth * oxcf->rc_cfg.vbrmin_section / 100);

  rc->min_frame_bandwidth =
      AOMMAX(rc->min_frame_bandwidth, FRAME_OVERHEAD_BITS);

  // A maximum bitrate for a frame is defined.
  // The baseline for this aligns with HW implementations that
  // can support decode of 1080P content up to a bitrate of MAX_MB_RATE bits
  // per 16x16 MB (averaged over a frame). However this limit is extended if
  // a very high rate is given on the command line or the the rate cannnot
  // be acheived because of a user specificed max q (e.g. when the user
  // specifies lossless encode.
  vbr_max_bits =
      (int)(((int64_t)rc->avg_frame_bandwidth * oxcf->rc_cfg.vbrmax_section) /
            100);
  rc->max_frame_bandwidth =
      AOMMAX(AOMMAX((MBs * MAX_MB_RATE), MAXRATE_1080P), vbr_max_bits);

}

void av1_set_target_rate(AV1_COMP *cpi, int width, int height) {
  AV1_RATE_CONTROL *const rc = &cpi->rc;
  int target_rate = rc->base_frame_target;

  av1_rc_set_frame_target(cpi, target_rate, width, height);
}

int av1_calc_pframe_target_size_one_pass_cbr(
    const AV1_COMP *cpi, FRAME_UPDATE_TYPE frame_update_type) {
  const AV1EncoderConfig *oxcf = &cpi->oxcf;
  const AV1_RATE_CONTROL *rc = &cpi->rc;
  const RateControlCfg *rc_cfg = &oxcf->rc_cfg;
  const int64_t diff = rc->optimal_buffer_level - rc->buffer_level;
  const int64_t one_pct_bits = 1 + rc->optimal_buffer_level / 100;
  int min_frame_target =
      AOMMAX(rc->avg_frame_bandwidth >> 4, FRAME_OVERHEAD_BITS);
  int target;

  //ToDo: Add support for rc_cfg->gf_cbr_boost_pct
  target = rc->avg_frame_bandwidth;
  if (cpi->use_svc) {
    // Note that for layers, avg_frame_bandwidth is the cumulative
    // per-frame-bandwidth. For the target size of this frame, use the
    // layer average frame size (i.e., non-cumulative per-frame-bw).
    int layer =
        LAYER_IDS_TO_IDX(cpi->svc.spatial_layer_id, cpi->svc.temporal_layer_id,
                         cpi->svc.number_temporal_layers);
    const AV1_LAYER_CONTEXT *lc = &cpi->svc.layer_context[layer];
    target = lc->avg_frame_size;
    min_frame_target = AOMMAX(lc->avg_frame_size >> 4, FRAME_OVERHEAD_BITS);
  }
  if (diff > 0) {
    // Lower the target bandwidth for this frame.
    const int pct_low =
        (int)AOMMIN(diff / one_pct_bits, rc_cfg->under_shoot_pct);
    target -= (target * pct_low) / 200;
  } else if (diff < 0) {
    // Increase the target bandwidth for this frame.
    const int pct_high =
        (int)AOMMIN(-diff / one_pct_bits, rc_cfg->over_shoot_pct);
    target += (target * pct_high) / 200;
  }
  if (rc_cfg->max_inter_bitrate_pct) {
    const int max_rate =
        rc->avg_frame_bandwidth * rc_cfg->max_inter_bitrate_pct / 100;
    target = AOMMIN(target, max_rate);
  }
  return AOMMAX(min_frame_target, target);
}

int av1_calc_iframe_target_size_one_pass_cbr(const AV1_COMP *cpi) {
  const AV1_RATE_CONTROL *rc = &cpi->rc;
  int target;
  if (cpi->common.current_frame.frame_number == 0) {
    target = ((rc->starting_buffer_level / 2) > INT_MAX)
                 ? INT_MAX
                 : (int)(rc->starting_buffer_level / 2);
  } else {
    int kf_boost = 32;
    double framerate = cpi->framerate;

    kf_boost = AOMMAX(kf_boost, (int)(2 * framerate - 16));
    if (rc->frames_since_key < framerate / 2) {
      kf_boost = (int)(kf_boost * rc->frames_since_key / (framerate / 2));
    }
    target = ((16 + kf_boost) * rc->avg_frame_bandwidth) >> 4;
  }
  return av1_rc_clamp_iframe_target_size(cpi, target);
}


#define DEFAULT_KF_BOOST_RT 2300
#define DEFAULT_GF_BOOST_RT 2000

void av1_get_one_pass_rt_params(AV1_COMP *cpi,
		                AV1_FRAME_TYPE frame_type) {
  AV1_RATE_CONTROL *const rc = &cpi->rc;
  AV1_COMMON *const cm = &cpi->common;
  GF_GROUP *const gf_group = &cpi->gf_group;
  AV1_SVC *const svc = &cpi->svc;
  int target;
  const int layer =
      LAYER_IDS_TO_IDX(svc->spatial_layer_id, svc->temporal_layer_id,
                       svc->number_temporal_layers);
  // Turn this on to explicitly set the reference structure rather than
  // relying on internal/default structure.
  if (cpi->use_svc) {
    av1_update_temporal_layer_framerate(cpi);
    av1_restore_layer_context(cpi);
  }
  //Fixme(Important): evaluate the svc use case
  // Set frame type.
  //if ((!cpi->use_svc && rc->frames_to_key == 0) ||
  //    (cpi->use_svc && svc->spatial_layer_id == 0 &&
  //     svc->current_superframe % cpi->oxcf.kf_cfg.key_freq_max == 0) ||
  //    (frame_flags & FRAMEFLAGS_KEY)) {
  if (frame_type == AV1_KEY_FRAME) {
    //rc->this_key_frame_forced =
    //    cm->current_frame.frame_number != 0 && rc->frames_to_key == 0;
    rc->frames_to_key = cpi->oxcf.kf_cfg.key_freq_max;
    rc->kf_boost = DEFAULT_KF_BOOST_RT;
    gf_group->update_type[gf_group->index] = KF_UPDATE;
    gf_group->frame_type[gf_group->index] = AV1_KEY_FRAME;
    gf_group->refbuf_state[gf_group->index] = REFBUF_RESET;
    if (cpi->use_svc) {
      if (cm->current_frame.frame_number > 0)
        av1_svc_reset_temporal_layers(cpi, 1);
      svc->layer_context[layer].is_key_frame = 1;
    }
  } else {
    gf_group->update_type[gf_group->index] = LF_UPDATE;
    gf_group->frame_type[gf_group->index] = AV1_INTER_FRAME;
    gf_group->refbuf_state[gf_group->index] = REFBUF_UPDATE;
    //Fixme (Important): Fix the key-frame setting?
    if (cpi->use_svc) {
      AV1_LAYER_CONTEXT *lc = &svc->layer_context[layer];
      lc->is_key_frame =
          svc->spatial_layer_id == 0
              ? 0
              : svc->layer_context[svc->temporal_layer_id].is_key_frame;
    }
  }
  // ToDo: Check for scene change, for non-SVC for now.
  //if (!cpi->use_svc && cpi->sf.rt_sf.check_scene_detection)
  //  rc_scene_detection_onepass_rt(cpi);

#if 0
  //Fix (important): Add res change support?
  if (resize_pending_params->width && resize_pending_params->height &&
             (cpi->common.width != resize_pending_params->width ||
              cpi->common.height != resize_pending_params->height)) {
    resize_reset_rc(cpi, resize_pending_params->width,
                    resize_pending_params->height, cm->width, cm->height);
  }
#endif
  // Set the GF interval and update flag.
  //set_gf_interval_update_onepass_rt(cpi, frame_params->frame_type);

  // Set target size.
  if (frame_type == AV1_KEY_FRAME) {
    target = av1_calc_iframe_target_size_one_pass_cbr(cpi);
  } else {
    target = av1_calc_pframe_target_size_one_pass_cbr(
        cpi, gf_group->update_type[gf_group->index]);
  }

  av1_rc_set_frame_target(cpi, target, cm->width, cm->height);
  rc->base_frame_target = target;
  cm->current_frame.frame_type = frame_type;
}

//Fixme: Check the requirement for resize_reset_rc which
//is only used for REAL_TIME mode enc in aom encoder.
//But we may need it for non-svc dynamic scaling use cases.
//static void resize_reset_rc(AV1_COMP *cpi, int resize_width, int resize_height,

int av1_encodedframe_overshoot_cbr(AV1_COMP *cpi, int *q) {
  AV1_COMMON *const cm = &cpi->common;
  AV1_RATE_CONTROL *const rc = &cpi->rc;
  AV1_SPEED_FEATURES *const sf = &cpi->sf;
  int thresh_qp = 7 * (rc->worst_quality >> 3);
  // Lower thresh_qp for video (more overshoot at lower Q) to be
  // more conservative for video.
  if (cpi->oxcf.tune_cfg.content != AOM_CONTENT_SCREEN)
    thresh_qp = 3 * (rc->worst_quality >> 2);
  if (sf->rt_sf.overshoot_detection_cbr == FAST_DETECTION_MAXQ &&
      cm->quant_params.base_qindex < thresh_qp) {
    double rate_correction_factor =
        cpi->rc.rate_correction_factors[AV1_INTER_NORMAL];
    const int target_size = cpi->rc.avg_frame_bandwidth;
    double new_correction_factor;
    int target_bits_per_mb;
    double q2;
    int enumerator;
    *q = (3 * cpi->rc.worst_quality + *q) >> 2;
    // Adjust avg_frame_qindex, buffer_level, and rate correction factors, as
    // these parameters will affect QP selection for subsequent frames. If they
    // have settled down to a very different (low QP) state, then not adjusting
    // them may cause next frame to select low QP and overshoot again.
    cpi->rc.avg_frame_qindex[AV1_INTER_FRAME] = *q;
    rc->buffer_level = rc->optimal_buffer_level;
    rc->bits_off_target = rc->optimal_buffer_level;
    // Reset rate under/over-shoot flags.
    cpi->rc.rc_1_frame = 0;
    cpi->rc.rc_2_frame = 0;
    // Adjust rate correction factor.
    target_bits_per_mb =
        (int)(((uint64_t)target_size << BPER_MB_NORMBITS) / cm->mi_params.MBs);
    // Rate correction factor based on target_bits_per_mb and qp (==max_QP).
    // This comes from the inverse computation of vp9_rc_bits_per_mb().
    q2 = av1_convert_qindex_to_q(*q, cm->seq_params.bit_depth);
    enumerator = 1800000;  // Factor for inter frame.
    enumerator += (int)(enumerator * q2) >> 12;
    new_correction_factor = (double)target_bits_per_mb * q2 / enumerator;
    if (new_correction_factor > rate_correction_factor) {
      rate_correction_factor =
          AOMMIN(2.0 * rate_correction_factor, new_correction_factor);
      if (rate_correction_factor > MAX_BPB_FACTOR)
        rate_correction_factor = MAX_BPB_FACTOR;
      cpi->rc.rate_correction_factors[AV1_INTER_NORMAL] = rate_correction_factor;
    }
    return 1;
  } else {
    return 0;
  }
}
