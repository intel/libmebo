/*
 *  Copyright (c) 2020 The WebM project authors. All Rights Reserved.
 *  Copyright (c) 2020 Intel corporation. All Rights Reserved.
 *  Copyright (c) 2020 Sreerenj Balachandran<sreerenj.balachandran@intel.com>
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "libvpx_vp8_ratectrl.h"

#define MIN_BPB_FACTOR 0.01
#define MAX_BPB_FACTOR 50

#define VPXMIN(x, y) (((x) < (y)) ? (x) : (y))
#define VPXMAX(x, y) (((x) > (y)) ? (x) : (y))

/* Bits Per MB at different Q (Multiplied by 512) */
#define BPER_MB_NORMBITS 9

/* Work in progress recalibration of baseline rate tables based on
 * the assumption that bits per mb is inversely proportional to the
 * quantizer value.
 */
static const int vp8_bits_per_mb[2][VP8_QINDEX_RANGE] = {
  /* Intra case 450000/Qintra */
  {
      1125000, 900000, 750000, 642857, 562500, 500000, 450000, 450000, 409090,
      375000,  346153, 321428, 300000, 281250, 264705, 264705, 250000, 236842,
      225000,  225000, 214285, 214285, 204545, 204545, 195652, 195652, 187500,
      180000,  180000, 173076, 166666, 160714, 155172, 150000, 145161, 140625,
      136363,  132352, 128571, 125000, 121621, 121621, 118421, 115384, 112500,
      109756,  107142, 104651, 102272, 100000, 97826,  97826,  95744,  93750,
      91836,   90000,  88235,  86538,  84905,  83333,  81818,  80357,  78947,
      77586,   76271,  75000,  73770,  72580,  71428,  70312,  69230,  68181,
      67164,   66176,  65217,  64285,  63380,  62500,  61643,  60810,  60000,
      59210,   59210,  58441,  57692,  56962,  56250,  55555,  54878,  54216,
      53571,   52941,  52325,  51724,  51136,  50561,  49450,  48387,  47368,
      46875,   45918,  45000,  44554,  44117,  43269,  42452,  41666,  40909,
      40178,   39473,  38793,  38135,  36885,  36290,  35714,  35156,  34615,
      34090,   33582,  33088,  32608,  32142,  31468,  31034,  30405,  29801,
      29220,   28662,
  },
  /* Inter case 285000/Qinter */
  {
      712500, 570000, 475000, 407142, 356250, 316666, 285000, 259090, 237500,
      219230, 203571, 190000, 178125, 167647, 158333, 150000, 142500, 135714,
      129545, 123913, 118750, 114000, 109615, 105555, 101785, 98275,  95000,
      91935,  89062,  86363,  83823,  81428,  79166,  77027,  75000,  73076,
      71250,  69512,  67857,  66279,  64772,  63333,  61956,  60638,  59375,
      58163,  57000,  55882,  54807,  53773,  52777,  51818,  50892,  50000,
      49137,  47500,  45967,  44531,  43181,  41911,  40714,  39583,  38513,
      37500,  36538,  35625,  34756,  33928,  33139,  32386,  31666,  30978,
      30319,  29687,  29081,  28500,  27941,  27403,  26886,  26388,  25909,
      25446,  25000,  24568,  23949,  23360,  22800,  22265,  21755,  21268,
      20802,  20357,  19930,  19520,  19127,  18750,  18387,  18037,  17701,
      17378,  17065,  16764,  16473,  16101,  15745,  15405,  15079,  14766,
      14467,  14179,  13902,  13636,  13380,  13133,  12895,  12666,  12445,
      12179,  11924,  11632,  11445,  11220,  11003,  10795,  10594,  10401,
      10215,  10035,
  }
};

static const int kf_boost_qadjustment[VP8_QINDEX_RANGE] = {
  128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 142,
  143, 144, 145, 146, 147, 148, 149, 150, 151, 152, 153, 154, 155, 156, 157,
  158, 159, 160, 161, 162, 163, 164, 165, 166, 167, 168, 169, 170, 171, 172,
  173, 174, 175, 176, 177, 178, 179, 180, 181, 182, 183, 184, 185, 186, 187,
  188, 189, 190, 191, 192, 193, 194, 195, 196, 197, 198, 199, 200, 200, 201,
  201, 202, 203, 203, 203, 204, 204, 205, 205, 206, 206, 207, 207, 208, 208,
  209, 209, 210, 210, 211, 211, 212, 212, 213, 213, 214, 214, 215, 215, 216,
  216, 217, 217, 218, 218, 219, 219, 220, 220, 220, 220, 220, 220, 220, 220,
  220, 220, 220, 220, 220, 220, 220, 220,
};

/* #define GFQ_ADJUSTMENT (Q+100) */
#define GFQ_ADJUSTMENT vp8_gf_boost_qadjustment[Q]
static const int vp8_gf_boost_qadjustment[VP8_QINDEX_RANGE] = {
  80,  82,  84,  86,  88,  90,  92,  94,  96,  97,  98,  99,  100, 101, 102,
  103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117,
  118, 119, 120, 121, 122, 123, 124, 125, 126, 127, 128, 129, 130, 131, 132,
  133, 134, 135, 136, 137, 138, 139, 140, 141, 142, 143, 144, 145, 146, 147,
  148, 149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159, 160, 161, 162,
  163, 164, 165, 166, 167, 168, 169, 170, 171, 172, 173, 174, 175, 176, 177,
  178, 179, 180, 181, 182, 183, 184, 184, 185, 185, 186, 186, 187, 187, 188,
  188, 189, 189, 190, 190, 191, 191, 192, 192, 193, 193, 194, 194, 194, 194,
  195, 195, 196, 196, 197, 197, 198, 198
};

/*
const int vp8_gf_boost_qadjustment[VP8_QINDEX_RANGE] =
{
    100,101,102,103,104,105,105,106,
    106,107,107,108,109,109,110,111,
    112,113,114,115,116,117,118,119,
    120,121,122,123,124,125,126,127,
    128,129,130,131,132,133,134,135,
    136,137,138,139,140,141,142,143,
    144,145,146,147,148,149,150,151,
    152,153,154,155,156,157,158,159,
    160,161,162,163,164,165,166,167,
    168,169,170,170,171,171,172,172,
    173,173,173,174,174,174,175,175,
    175,176,176,176,177,177,177,177,
    178,178,179,179,180,180,181,181,
    182,182,183,183,184,184,185,185,
    186,186,187,187,188,188,189,189,
    190,190,191,191,192,192,193,193,
};
*/

static const int kf_gf_boost_qlimits[VP8_QINDEX_RANGE] = {
  150, 155, 160, 165, 170, 175, 180, 185, 190, 195, 200, 205, 210, 215, 220,
  225, 230, 235, 240, 245, 250, 255, 260, 265, 270, 275, 280, 285, 290, 295,
  300, 305, 310, 320, 330, 340, 350, 360, 370, 380, 390, 400, 410, 420, 430,
  440, 450, 460, 470, 480, 490, 500, 510, 520, 530, 540, 550, 560, 570, 580,
  590, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600,
  600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600,
  600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600,
  600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600,
  600, 600, 600, 600, 600, 600, 600, 600,
};

static const int gf_adjust_table[101] = {
  100, 115, 130, 145, 160, 175, 190, 200, 210, 220, 230, 240, 260, 270, 280,
  290, 300, 310, 320, 330, 340, 350, 360, 370, 380, 390, 400, 400, 400, 400,
  400, 400, 400, 400, 400, 400, 400, 400, 400, 400, 400, 400, 400, 400, 400,
  400, 400, 400, 400, 400, 400, 400, 400, 400, 400, 400, 400, 400, 400, 400,
  400, 400, 400, 400, 400, 400, 400, 400, 400, 400, 400, 400, 400, 400, 400,
  400, 400, 400, 400, 400, 400, 400, 400, 400, 400, 400, 400, 400, 400, 400,
  400, 400, 400, 400, 400, 400, 400, 400, 400, 400, 400,
};

static const int gf_intra_usage_adjustment[20] = {
  125, 120, 115, 110, 105, 100, 95, 85, 80, 75,
  70,  65,  60,  55,  50,  50,  50, 50, 50, 50,
};

static const int gf_interval_table[101] = {
  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,
  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  8,  8,  8,
  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,
  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,
  9,  9,  9,  10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
  10, 10, 10, 10, 10, 10, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11,
};

static const unsigned int prior_key_frame_weight[KEY_FRAME_CONTEXT] = { 1, 2, 3, 4, 5};

/* Shift down with rounding */
#define ROUND_POWER_OF_TWO(value, n) (((value) + (1 << ((n)-1))) >> (n))
#define ROUND64_POWER_OF_TWO(value, n) (((value) + (1ULL << ((n)-1))) >> (n))

static void vp8_adjust_key_frame_context(VP8_COMP *cpi);

void
libvpx_vp8_rc_postencode_update (VP8_COMP *cpi, uint64_t size)
{
  VP8_COMMON *const cm = &cpi->common;
  int Q = cm->base_qindex;

  cpi->total_byte_count += (size);
  cpi->projected_frame_size = (int)(size) << 3;

  //Fimxe: Add this field in cpi structure? in vp8 it is the code in encode routine
  //if (!active_worst_qchanged)
  libvpx_vp8_update_rate_correction_factors(cpi, 2);

  cpi->last_q[cm->frame_type] = cm->base_qindex;

  if (cm->frame_type == VP8_KEY_FRAME) {
    vp8_adjust_key_frame_context(cpi);
  }

  /* Keep a record of ambient average Q. */
  if (cm->frame_type != VP8_KEY_FRAME) {
    cpi->avg_frame_qindex =
        (2 + 3 * cpi->avg_frame_qindex + cm->base_qindex) >> 2;
  }

  /* Keep a record from which we can calculate the average Q excluding
   * GF updates and key frames
   */
  if (cm->frame_type != VP8_KEY_FRAME) {
    cpi->ni_frames++;

    /* Calculate the average Q for normal inter frames (not key or GFU
     * frames).
     */
    /* Damp value for first few frames */
    if (cpi->ni_frames > 150) {
      cpi->ni_tot_qi += Q;
      cpi->ni_av_qi = (cpi->ni_tot_qi / cpi->ni_frames);
    }
    /* For one pass, early in the clip ... average the current frame Q
     * value with the worstq entered by the user as a dampening measure
     */
    else {
      cpi->ni_tot_qi += Q;
      cpi->ni_av_qi =
          ((cpi->ni_tot_qi / cpi->ni_frames) + cpi->worst_quality + 1) / 2;
    }

    /* If the average Q is higher than what was used in the last
     * frame (after going through the recode loop to keep the frame
     * size within range) then use the last frame value - 1. The -1
     * is designed to stop Q and hence the data rate, from
     * progressively falling away during difficult sections, but at
     * the same time reduce the number of itterations around the
     * recode loop.
     */
    if (Q > cpi->ni_av_qi) cpi->ni_av_qi = Q - 1;
  }

  /* Update the buffer level variable. */
  /* Non-viewable frames are a special case and are treated as pure overhead. */
  if (!cm->show_frame) {
    cpi->bits_off_target -= cpi->projected_frame_size;
  } else {
    cpi->bits_off_target +=
        cpi->av_per_frame_bandwidth - cpi->projected_frame_size;
  }

  /* Clip the buffer level to the maximum specified buffer size */
  if (cpi->bits_off_target > cpi->oxcf.maximum_buffer_size) {
    cpi->bits_off_target = cpi->oxcf.maximum_buffer_size;
  }
  // If the frame dropper is not enabled, don't let the buffer level go below
  // some threshold, given here by -|maximum_buffer_size|. For now we only do
  // this for screen content input.
  if (cpi->drop_frames_allowed == 0 && cpi->oxcf.screen_content_mode &&
      cpi->bits_off_target < -cpi->oxcf.maximum_buffer_size) {
    cpi->bits_off_target = -cpi->oxcf.maximum_buffer_size;
  }

  /* Rolling monitors of whether we are over or underspending used to
   * help regulate min and Max Q in two pass.
   */
  cpi->rolling_target_bits = (int)ROUND64_POWER_OF_TWO(
      (int64_t)cpi->rolling_target_bits * 3 + cpi->this_frame_target, 2);
  cpi->rolling_actual_bits = (int)ROUND64_POWER_OF_TWO(
      (int64_t)cpi->rolling_actual_bits * 3 + cpi->projected_frame_size, 2);
  cpi->long_rolling_target_bits = (int)ROUND64_POWER_OF_TWO(
      (int64_t)cpi->long_rolling_target_bits * 31 + cpi->this_frame_target, 5);
  cpi->long_rolling_actual_bits = (int)ROUND64_POWER_OF_TWO(
      (int64_t)cpi->long_rolling_actual_bits * 31 + cpi->projected_frame_size,
      5);
  /* Actual bits spent */
  cpi->total_actual_bits += cpi->projected_frame_size;

  cpi->buffer_level = cpi->bits_off_target;

  //Fixme: temporal svc is not yet enabled

  /* Dont increment frame counters if this was an altref buffer update
   * not a real frame
   */
  if (cm->show_frame) {
    cm->current_video_frame++;
    cpi->frames_since_key++;
  }
}

void libvpx_vp8_new_framerate(VP8_COMP *cpi, double framerate) {
  if (framerate < .1) framerate = 30;

  cpi->framerate = framerate;
  cpi->output_framerate = framerate;
  cpi->per_frame_bandwidth =
      (int)(cpi->oxcf.target_bandwidth / cpi->output_framerate);
  cpi->av_per_frame_bandwidth = cpi->per_frame_bandwidth;
  cpi->min_frame_bandwidth = (int)(cpi->av_per_frame_bandwidth *
                                   cpi->oxcf.two_pass_vbrmin_section / 100);
  /* Set Maximum gf/arf interval */
  cpi->max_gf_interval = ((int)(cpi->output_framerate / 2.0) + 2);

  if (cpi->max_gf_interval < 12) cpi->max_gf_interval = 12;
}

static int estimate_bits_at_q(int frame_kind, int Q, int MBs,
                              double correction_factor) {
  int Bpm = (int)(.5 + correction_factor * vp8_bits_per_mb[frame_kind][Q]);

  /* Attempt to retain reasonable accuracy without overflow. The cutoff is
   * chosen such that the maximum product of Bpm and MBs fits 31 bits. The
   * largest Bpm takes 20 bits.
   */
  if (MBs > (1 << 11)) {
    return (Bpm >> BPER_MB_NORMBITS) * MBs;
  } else {
    return (Bpm * MBs) >> BPER_MB_NORMBITS;
  }
}

//Done
static void calc_iframe_target_size(VP8_COMP *cpi) {
  /* boost defaults to half second */
  int kf_boost;
  uint64_t target;

  /* First Frame is a special case */
  if (cpi->common.current_video_frame == 0) {
    /* 1 Pass there is no information on which to base size so use
     * bandwidth per second * fraction of the initial buffer
     * level
     */
    target = cpi->oxcf.starting_buffer_level / 2;

    if (target > cpi->oxcf.target_bandwidth * 3 / 2) {
      target = cpi->oxcf.target_bandwidth * 3 / 2;
    }
  } else {
    /* ToDo: if this keyframe was forced, use a more recent Q estimate */
    //int Q = (cpi->common.frame_flags & VP8_FRAMEFLAGS_KEY) ? cpi->avg_frame_qindex
    //                                                   : cpi->ni_av_qi;
    //int Q = cpi->ni_av_qi;
    int Q = cpi->avg_frame_qindex;
    int initial_boost = 32; /* |3.0 * per_frame_bandwidth| */
    /* Boost depends somewhat on frame rate: only used for 1 layer case. */
    if (cpi->oxcf.number_of_layers == 1) {
      kf_boost = VPXMAX(initial_boost, (int)(2 * cpi->output_framerate - 16));
    } else {
      /* Initial factor: set target size to: |3.0 * per_frame_bandwidth|. */
      kf_boost = initial_boost;
    }

    /* adjustment up based on q: this factor ranges from ~1.2 to 2.2. */
    kf_boost = kf_boost * kf_boost_qadjustment[Q] / 100;

    /* frame separation adjustment ( down) */
    if (cpi->frames_since_key < cpi->output_framerate / 2) {
      kf_boost =
          (int)(kf_boost * cpi->frames_since_key / (cpi->output_framerate / 2));
    }

    /* Minimal target size is |2* per_frame_bandwidth|. */
    if (kf_boost < 16) kf_boost = 16;

    target = ((16 + kf_boost) * cpi->per_frame_bandwidth) >> 4;
  }

  if (cpi->oxcf.rc_max_intra_bitrate_pct) {
    unsigned int max_rate =
        cpi->per_frame_bandwidth * cpi->oxcf.rc_max_intra_bitrate_pct / 100;

    if (target > max_rate) target = max_rate;
  }

  cpi->this_frame_target = (int)target;

  /* TODO: if we separate rate targeting from Q targeting, move this.
   * Reset the active worst quality to the baseline value for key frames.
   */
  if (cpi->pass != 2) cpi->active_worst_quality = cpi->worst_quality;

}

/* Do the best we can to define the parameters for the next GF based on what
 * information we have available.
 */
static void calc_gf_params(VP8_COMP *cpi) {
  int Q =
      (cpi->oxcf.fixed_q < 0) ? cpi->last_q[VP8_INTER_FRAME] : cpi->oxcf.fixed_q;
  int Boost = 0;

  int gf_frame_useage = 0; /* Golden frame useage since last GF */
  int tot_mbs = cpi->recent_ref_frame_usage[INTRA_FRAME] +
                cpi->recent_ref_frame_usage[LAST_FRAME] +
                cpi->recent_ref_frame_usage[GOLDEN_FRAME] +
                cpi->recent_ref_frame_usage[ALTREF_FRAME];

  int pct_gf_active = (100 * cpi->gf_active_count) /
                      (cpi->common.mb_rows * cpi->common.mb_cols);

  if (tot_mbs) {
    gf_frame_useage = (cpi->recent_ref_frame_usage[GOLDEN_FRAME] +
                       cpi->recent_ref_frame_usage[ALTREF_FRAME]) *
                      100 / tot_mbs;
  }

  if (pct_gf_active > gf_frame_useage) gf_frame_useage = pct_gf_active;

  /* Not two pass */
  if (cpi->pass != 2) {
    /* Single Pass lagged mode: TBD */
    if (0) {
    }

    /* Single Pass compression: Has to use current and historical data */
    else {
#if 0
            /* Experimental code */
            int index = cpi->one_pass_frame_index;
            int frames_to_scan = (cpi->max_gf_interval <= MAX_LAG_BUFFERS) ? cpi->max_gf_interval : MAX_LAG_BUFFERS;

            /* ************** Experimental code - incomplete */
            /*
            double decay_val = 1.0;
            double IIAccumulator = 0.0;
            double last_iiaccumulator = 0.0;
            double IIRatio;

            cpi->one_pass_frame_index = cpi->common.current_video_frame%MAX_LAG_BUFFERS;

            for ( i = 0; i < (frames_to_scan - 1); i++ )
            {
                if ( index < 0 )
                    index = MAX_LAG_BUFFERS;
                index --;

                if ( cpi->one_pass_frame_stats[index].frame_coded_error > 0.0 )
                {
                    IIRatio = cpi->one_pass_frame_stats[index].frame_intra_error / cpi->one_pass_frame_stats[index].frame_coded_error;

                    if ( IIRatio > 30.0 )
                        IIRatio = 30.0;
                }
                else
                    IIRatio = 30.0;

                IIAccumulator += IIRatio * decay_val;

                decay_val = decay_val * cpi->one_pass_frame_stats[index].frame_pcnt_inter;

                if (    (i > MIN_GF_INTERVAL) &&
                        ((IIAccumulator - last_iiaccumulator) < 2.0) )
                {
                    break;
                }
                last_iiaccumulator = IIAccumulator;
            }

            Boost = IIAccumulator*100.0/16.0;
            cpi->baseline_gf_interval = i;

            */
#else

      /*************************************************************/
      /* OLD code */

      /* Adjust boost based upon ambient Q */
      Boost = GFQ_ADJUSTMENT;

      /* Adjust based upon most recently measure intra useage */
      Boost = Boost *
              gf_intra_usage_adjustment[(cpi->this_frame_percent_intra < 15)
                                            ? cpi->this_frame_percent_intra
                                            : 14] /
              100;

      /* Adjust gf boost based upon GF usage since last GF */
      Boost = Boost * gf_adjust_table[gf_frame_useage] / 100;
#endif
    }


#if 0
    //Fixme
    /* golden frame boost without recode loop often goes awry.  be
     * safe by keeping numbers down.
     */
    if (!cpi->sf.recode_loop) {
      if (cpi->compressor_speed == 2) Boost = Boost / 2;
    }
#endif
    /* Apply an upper limit based on Q for 1 pass encodes */
    if (Boost > kf_gf_boost_qlimits[Q] && (cpi->pass == 0)) {
      Boost = kf_gf_boost_qlimits[Q];

      /* Apply lower limits to boost. */
    } else if (Boost < 110) {
      Boost = 110;
    }

    /* Note the boost used */
    cpi->last_boost = Boost;
  }

  /* Estimate next interval
   * This is updated once the real frame size/boost is known.
   */
  if (cpi->oxcf.fixed_q == -1) {
      /* 1 Pass */
      cpi->frames_till_gf_update_due = cpi->baseline_gf_interval;

      if (cpi->last_boost > 750) cpi->frames_till_gf_update_due++;

      if (cpi->last_boost > 1000) cpi->frames_till_gf_update_due++;

      if (cpi->last_boost > 1250) cpi->frames_till_gf_update_due++;

      if (cpi->last_boost >= 1500) cpi->frames_till_gf_update_due++;

      if (gf_interval_table[gf_frame_useage] > cpi->frames_till_gf_update_due) {
        cpi->frames_till_gf_update_due = gf_interval_table[gf_frame_useage];
      }

      if (cpi->frames_till_gf_update_due > cpi->max_gf_interval) {
        cpi->frames_till_gf_update_due = cpi->max_gf_interval;
      }
  } else {
    cpi->frames_till_gf_update_due = cpi->baseline_gf_interval;
  }
}

static void calc_pframe_target_size(VP8_COMP *cpi) {
  int min_frame_target;
  int old_per_frame_bandwidth = cpi->per_frame_bandwidth;

  min_frame_target = cpi->per_frame_bandwidth / 4;

  /* Normal frames (gf,and inter) */
  /* 1 pass */
  int Adjustment;
  /* Make rate adjustment to recover bits spent in key frame
   * Test to see if the key frame inter data rate correction
   * should still be in force
   */
  if (cpi->kf_overspend_bits > 0) {
        Adjustment = (cpi->kf_bitrate_adjustment <= cpi->kf_overspend_bits)
                         ? cpi->kf_bitrate_adjustment
                         : cpi->kf_overspend_bits;

        if (Adjustment > (cpi->per_frame_bandwidth - min_frame_target)) {
          Adjustment = (cpi->per_frame_bandwidth - min_frame_target);
        }

        cpi->kf_overspend_bits -= Adjustment;

        /* Calculate an inter frame bandwidth target for the next
         * few frames designed to recover any extra bits spent on
         * the key frame.
         */
        cpi->this_frame_target = cpi->per_frame_bandwidth - Adjustment;

        if (cpi->this_frame_target < min_frame_target) {
          cpi->this_frame_target = min_frame_target;
        }
  } else {
        cpi->this_frame_target = cpi->per_frame_bandwidth;
  }

  /* If appropriate make an adjustment to recover bits spent on a
   * recent GF
   */
  if ((cpi->gf_overspend_bits > 0) &&
      (cpi->this_frame_target > min_frame_target)) {
      Adjustment = (cpi->non_gf_bitrate_adjustment <= cpi->gf_overspend_bits)
                       ? cpi->non_gf_bitrate_adjustment
                       : cpi->gf_overspend_bits;

      if (Adjustment > (cpi->this_frame_target - min_frame_target)) {
        Adjustment = (cpi->this_frame_target - min_frame_target);
      }

      cpi->gf_overspend_bits -= Adjustment;
      cpi->this_frame_target -= Adjustment;
  }

  /* Apply small + and - boosts for non gf frames */
  if ((cpi->last_boost > 150) && (cpi->frames_till_gf_update_due > 0) &&
        (cpi->current_gf_interval >= (MIN_GF_INTERVAL << 1))) {
        /* % Adjustment limited to the range 1% to 10% */
        Adjustment = (cpi->last_boost - 100) >> 5;

        if (Adjustment < 1) {
          Adjustment = 1;
        } else if (Adjustment > 10) {
          Adjustment = 10;
        }

        /* Convert to bits */
        Adjustment = (cpi->this_frame_target * Adjustment) / 100;

        if (Adjustment > (cpi->this_frame_target - min_frame_target)) {
          Adjustment = (cpi->this_frame_target - min_frame_target);
        }

        if (cpi->frames_since_golden == (cpi->current_gf_interval >> 1)) {
          Adjustment = (cpi->current_gf_interval - 1) * Adjustment;
          // Limit adjustment to 10% of current target.
          if (Adjustment > (10 * cpi->this_frame_target) / 100) {
            Adjustment = (10 * cpi->this_frame_target) / 100;
          }
          cpi->this_frame_target += Adjustment;
        } else {
          cpi->this_frame_target -= Adjustment;
        }
  }

  /* Sanity check that the total sum of adjustments is not above the
   * maximum allowed That is that having allowed for KF and GF penalties
   * we have not pushed the current interframe target to low. If the
   * adjustment we apply here is not capable of recovering all the extra
   * bits we have spent in the KF or GF then the remainder will have to
   * be recovered over a longer time span via other buffer / rate control
   * mechanisms.
   */
  if (cpi->this_frame_target < min_frame_target) {
    cpi->this_frame_target = min_frame_target;
  }

  if (!cpi->common.refresh_alt_ref_frame) {
    /* Note the baseline target data rate for this inter frame. */
    cpi->inter_frame_target = cpi->this_frame_target;
  }

  /* One Pass specific code */
  if (cpi->pass == 0) {
    /* Adapt target frame size with respect to any buffering constraints: */
    if (cpi->buffered_mode) {
      int one_percent_bits = (int)(1 + cpi->oxcf.optimal_buffer_level / 100);

      if ((cpi->buffer_level < cpi->oxcf.optimal_buffer_level) ||
          (cpi->bits_off_target < cpi->oxcf.optimal_buffer_level)) {
        int percent_low = 0;

        /* Decide whether or not we need to adjust the frame data
         * rate target.
         *
         * If we are are below the optimal buffer fullness level
         * and adherence to buffering constraints is important to
         * the end usage then adjust the per frame target.
         */
        if (cpi->buffer_level < cpi->oxcf.optimal_buffer_level) {
          percent_low =
              (int)((cpi->oxcf.optimal_buffer_level - cpi->buffer_level) /
                    one_percent_bits);
        }
        /* Are we overshooting the long term clip data rate... */
        else if (cpi->bits_off_target < 0) {
          /* Adjust per frame data target downwards to compensate. */
          percent_low =
              (int)(100 * -cpi->bits_off_target / (cpi->total_byte_count * 8));
        }

        if (percent_low > cpi->oxcf.under_shoot_pct) {
          percent_low = cpi->oxcf.under_shoot_pct;
        } else if (percent_low < 0) {
          percent_low = 0;
        }

        /* lower the target bandwidth for this frame. */
        cpi->this_frame_target -= (cpi->this_frame_target * percent_low) / 200;

        /* Are we using allowing control of active_worst_allowed_q
         * according to buffer level.
         */
        if (cpi->auto_worst_q && cpi->ni_frames > 150) {
          int64_t critical_buffer_level;

          /* For streaming applications the most important factor is
           * cpi->buffer_level as this takes into account the
           * specified short term buffering constraints. However,
           * hitting the long term clip data rate target is also
           * important.
           */
           /* Take the smaller of cpi->buffer_level and
            * cpi->bits_off_target
            */
           critical_buffer_level = (cpi->buffer_level < cpi->bits_off_target)
                                        ? cpi->buffer_level
                                        : cpi->bits_off_target;

          /* Set the active worst quality based upon the selected
           * buffer fullness number.
           */
          if (critical_buffer_level < cpi->oxcf.optimal_buffer_level) {
            if (critical_buffer_level > (cpi->oxcf.optimal_buffer_level >> 2)) {
              int64_t qadjustment_range = cpi->worst_quality - cpi->ni_av_qi;
              int64_t above_base = (critical_buffer_level -
                                    (cpi->oxcf.optimal_buffer_level >> 2));

              /* Step active worst quality down from
               * cpi->ni_av_qi when (critical_buffer_level ==
               * cpi->optimal_buffer_level) to
               * cpi->worst_quality when
               * (critical_buffer_level ==
               *     cpi->optimal_buffer_level >> 2)
               */
              cpi->active_worst_quality =
                  cpi->worst_quality -
                  (int)((qadjustment_range * above_base) /
                        (cpi->oxcf.optimal_buffer_level * 3 >> 2));
            } else {
              cpi->active_worst_quality = cpi->worst_quality;
            }
          } else {
            cpi->active_worst_quality = cpi->ni_av_qi;
          }
        } else {
          cpi->active_worst_quality = cpi->worst_quality;
        }
      } else {
        int percent_high = 0;

        if (cpi->buffer_level > cpi->oxcf.optimal_buffer_level) {
          percent_high =
              (int)((cpi->buffer_level - cpi->oxcf.optimal_buffer_level) /
                    one_percent_bits);
        } else if (cpi->bits_off_target > cpi->oxcf.optimal_buffer_level) {
          percent_high =
              (int)((100 * cpi->bits_off_target) / (cpi->total_byte_count * 8));
        }

        if (percent_high > cpi->oxcf.over_shoot_pct) {
          percent_high = cpi->oxcf.over_shoot_pct;
        } else if (percent_high < 0) {
          percent_high = 0;
        }

        cpi->this_frame_target += (cpi->this_frame_target * percent_high) / 200;

        /* Are we allowing control of active_worst_allowed_q according
         * to buffer level.
         */
        if (cpi->auto_worst_q && cpi->ni_frames > 150) {
          /* When using the relaxed buffer model stick to the
           * user specified value
           */
          cpi->active_worst_quality = cpi->ni_av_qi;
        } else {
          cpi->active_worst_quality = cpi->worst_quality;
        }
      }

      /* Set active_best_quality to prevent quality rising too high */
      cpi->active_best_quality = cpi->best_quality;

      /* Worst quality obviously must not be better than best quality */
      if (cpi->active_worst_quality <= cpi->active_best_quality) {
        cpi->active_worst_quality = cpi->active_best_quality + 1;
      }

      if (cpi->active_worst_quality > 127) cpi->active_worst_quality = 127;
    }
    /* Unbuffered mode (eg. video conferencing) */
    else {
      /* Set the active worst quality */
      cpi->active_worst_quality = cpi->worst_quality;
    }

  }

  /* Adjust target frame size for Golden Frames: */
  if ((cpi->frames_till_gf_update_due == 0) && !cpi->drop_frame) {
    if (!cpi->gf_update_onepass_cbr) {
      int Q = (cpi->oxcf.fixed_q < 0) ? cpi->last_q[VP8_INTER_FRAME]
                                      : cpi->oxcf.fixed_q;

      int gf_frame_useage = 0; /* Golden frame useage since last GF */
      int tot_mbs = cpi->recent_ref_frame_usage[INTRA_FRAME] +
                    cpi->recent_ref_frame_usage[LAST_FRAME] +
                    cpi->recent_ref_frame_usage[GOLDEN_FRAME] +
                    cpi->recent_ref_frame_usage[ALTREF_FRAME];

      int pct_gf_active = (100 * cpi->gf_active_count) /
                          (cpi->common.mb_rows * cpi->common.mb_cols);

      if (tot_mbs) {
        gf_frame_useage = (cpi->recent_ref_frame_usage[GOLDEN_FRAME] +
                           cpi->recent_ref_frame_usage[ALTREF_FRAME]) *
                          100 / tot_mbs;
      }

      if (pct_gf_active > gf_frame_useage) gf_frame_useage = pct_gf_active;

      /* Is a fixed manual GF frequency being used */
      if (cpi->auto_gold) {
        /* For one pass throw a GF if recent frame intra useage is
         * low or the GF useage is high
         */
        if ((cpi->pass == 0) &&
            (cpi->this_frame_percent_intra < 15 || gf_frame_useage >= 5)) {
          cpi->common.refresh_golden_frame = 1;

          /* Two pass GF descision */
        } else if (cpi->pass == 2) {
          cpi->common.refresh_golden_frame = 1;
        }
      }

      if (cpi->common.refresh_golden_frame == 1) {
        if (cpi->auto_adjust_gold_quantizer) {
          calc_gf_params(cpi);
        }

        /* If we are using alternate ref instead of gf then do not apply the
         * boost It will instead be applied to the altref update Jims
         * modified boost
         */
        if (!cpi->source_alt_ref_active) {
          if (cpi->oxcf.fixed_q < 0) {
              int Boost = cpi->last_boost;
              int frames_in_section = cpi->frames_till_gf_update_due + 1;
              int allocation_chunks = (frames_in_section * 100) + (Boost - 100);
              int bits_in_section = cpi->inter_frame_target * frames_in_section;

              /* Normalize Altboost and allocations chunck down to
               * prevent overflow
               */
              while (Boost > 1000) {
                Boost /= 2;
                allocation_chunks /= 2;
              }

              /* Avoid loss of precision but avoid overflow */
              if ((bits_in_section >> 7) > allocation_chunks) {
                cpi->this_frame_target =
                    Boost * (bits_in_section / allocation_chunks);
              } else {
                cpi->this_frame_target =
                    (Boost * bits_in_section) / allocation_chunks;
              }
          } else {
            cpi->this_frame_target =
                (estimate_bits_at_q(1, Q, cpi->common.MBs, 1.0) *
                 cpi->last_boost) /
                100;
          }
        } else {
          /* If there is an active ARF at this location use the minimum
           * bits on this frame even if it is a contructed arf.
           * The active maximum quantizer insures that an appropriate
           * number of bits will be spent if needed for contstructed ARFs.
           */
          cpi->this_frame_target = 0;
        }

        cpi->current_gf_interval = cpi->frames_till_gf_update_due;
      }
    } else {
      // Special case for 1 pass CBR: fixed gf period.
      // TODO(marpan): Adjust this boost/interval logic.
      // If gf_cbr_boost_pct is small (below threshold) set the flag
      // gf_noboost_onepass_cbr = 1, which forces the gf to use the same
      // rate correction factor as last.
      cpi->gf_noboost_onepass_cbr = (cpi->oxcf.gf_cbr_boost_pct <= 100);
      cpi->baseline_gf_interval = cpi->gf_interval_onepass_cbr;
      // Skip this update if the zero_mvcount is low.
      if (cpi->zeromv_count > (cpi->common.MBs >> 1)) {
        cpi->common.refresh_golden_frame = 1;
        cpi->this_frame_target =
            (cpi->this_frame_target * (100 + cpi->oxcf.gf_cbr_boost_pct)) / 100;
      }
      cpi->frames_till_gf_update_due = cpi->baseline_gf_interval;
      cpi->current_gf_interval = cpi->frames_till_gf_update_due;
    }
  }

  cpi->per_frame_bandwidth = old_per_frame_bandwidth;
}

void libvpx_vp8_update_rate_correction_factors(VP8_COMP *cpi, int damp_var) {
  int Q = cpi->common.base_qindex;
  int correction_factor = 100;
  double rate_correction_factor;
  double adjustment_limit;

  int projected_size_based_on_q = 0;

  if (cpi->common.frame_type == VP8_KEY_FRAME) {
    rate_correction_factor = cpi->key_frame_rate_correction_factor;
  } else {
    if (cpi->oxcf.number_of_layers == 1 && !cpi->gf_noboost_onepass_cbr &&
        (cpi->common.refresh_alt_ref_frame ||
         cpi->common.refresh_golden_frame)) {
      rate_correction_factor = cpi->gf_rate_correction_factor;
    } else {
      rate_correction_factor = cpi->rate_correction_factor;
    }
  }

  /* Work out how big we would have expected the frame to be at this Q
   * given the current correction factor. Stay in double to avoid int
   * overflow when values are large
   */
  projected_size_based_on_q =
      (int)(((.5 + rate_correction_factor *
                       vp8_bits_per_mb[cpi->common.frame_type][Q]) *
             cpi->common.MBs) /
            (1 << BPER_MB_NORMBITS));

  /* Work out a size correction factor. */
  if (projected_size_based_on_q > 0) {
    correction_factor =
        (100 * cpi->projected_frame_size) / projected_size_based_on_q;
  }

  /* More heavily damped adjustment used if we have been oscillating
   * either side of target
   */
  switch (damp_var) {
    case 0: adjustment_limit = 0.75; break;
    case 1: adjustment_limit = 0.375; break;
    case 2:
    default: adjustment_limit = 0.25; break;
  }

  if (correction_factor > 102) {
    /* We are not already at the worst allowable quality */
    correction_factor =
        (int)(100.5 + ((correction_factor - 100) * adjustment_limit));
    rate_correction_factor =
        ((rate_correction_factor * correction_factor) / 100);

    /* Keep rate_correction_factor within limits */
    if (rate_correction_factor > MAX_BPB_FACTOR) {
      rate_correction_factor = MAX_BPB_FACTOR;
    }
  } else if (correction_factor < 99) {
    /* We are not already at the best allowable quality */
    correction_factor =
        (int)(100.5 - ((100 - correction_factor) * adjustment_limit));
    rate_correction_factor =
        ((rate_correction_factor * correction_factor) / 100);

    /* Keep rate_correction_factor within limits */
    if (rate_correction_factor < MIN_BPB_FACTOR) {
      rate_correction_factor = MIN_BPB_FACTOR;
    }
  }

  if (cpi->common.frame_type == VP8_KEY_FRAME) {
    cpi->key_frame_rate_correction_factor = rate_correction_factor;
  } else {
    if (cpi->oxcf.number_of_layers == 1 && !cpi->gf_noboost_onepass_cbr &&
        (cpi->common.refresh_alt_ref_frame ||
         cpi->common.refresh_golden_frame)) {
      cpi->gf_rate_correction_factor = rate_correction_factor;
    } else {
      cpi->rate_correction_factor = rate_correction_factor;
    }
  }
}

int libvpx_vp8_regulate_q(VP8_COMP *cpi, int target_bits_per_frame) {
  int Q = cpi->active_worst_quality;
  int i;
  int last_error = INT_MAX;
  int target_bits_per_mb;
  int bits_per_mb_at_this_q;
  double correction_factor;

  /* Select the appropriate correction factor based upon type of frame. */
  if (cpi->common.frame_type == VP8_KEY_FRAME) {
    correction_factor = cpi->key_frame_rate_correction_factor;
  } else {
    if (cpi->oxcf.number_of_layers == 1 && !cpi->gf_noboost_onepass_cbr &&
        (cpi->common.refresh_alt_ref_frame ||
         cpi->common.refresh_golden_frame)) {
      correction_factor = cpi->gf_rate_correction_factor;
    } else {
      correction_factor = cpi->rate_correction_factor;
    }
  }

  /* Calculate required scaling factor based on target frame size and
   * size of frame produced using previous Q
   */
  if (target_bits_per_frame >= (INT_MAX >> BPER_MB_NORMBITS)) {
    /* Case where we would overflow int */
    target_bits_per_mb = (target_bits_per_frame / cpi->common.MBs)
                         << BPER_MB_NORMBITS;
  } else {
    target_bits_per_mb =
        (target_bits_per_frame << BPER_MB_NORMBITS) / cpi->common.MBs;
  }

  i = cpi->active_best_quality;

  do {
     bits_per_mb_at_this_q =
         (int)(.5 +
              correction_factor * vp8_bits_per_mb[cpi->common.frame_type][i]);

     if (bits_per_mb_at_this_q <= target_bits_per_mb) {
       if ((target_bits_per_mb - bits_per_mb_at_this_q) <= last_error) {
         Q = i;
       } else {
         Q = i - 1;
       }

       break;
    } else {
      last_error = bits_per_mb_at_this_q - target_bits_per_mb;
    }
  } while (++i <= cpi->active_worst_quality);

  return Q;
}

static int estimate_keyframe_frequency(VP8_COMP *cpi) {
  int i;

  /* Average key frame frequency */
  int av_key_frame_frequency = 0;

  /* First key frame at start of sequence is a special case. We have no
   * frequency data.
   */
  if (cpi->key_frame_count == 1) {
    /* Assume a default of 1 kf every 2 seconds, or the max kf interval,
     * whichever is smaller.
     */
    int key_freq = cpi->oxcf.key_freq > 0 ? cpi->oxcf.key_freq : 1;
    av_key_frame_frequency = 1 + (int)cpi->output_framerate * 2;

    if (av_key_frame_frequency > key_freq) {
      av_key_frame_frequency = key_freq;
    }

    cpi->prior_key_frame_distance[KEY_FRAME_CONTEXT - 1] =
        av_key_frame_frequency;
  } else {
    unsigned int total_weight = 0;
    int last_kf_interval =
        (cpi->frames_since_key > 0) ? cpi->frames_since_key : 1;

    /* reset keyframe context and calculate weighted average of last
     * KEY_FRAME_CONTEXT keyframes
     */
    for (i = 0; i < KEY_FRAME_CONTEXT; ++i) {
      if (i < KEY_FRAME_CONTEXT - 1) {
        cpi->prior_key_frame_distance[i] = cpi->prior_key_frame_distance[i + 1];
      } else {
        cpi->prior_key_frame_distance[i] = last_kf_interval;
      }

      av_key_frame_frequency +=
          prior_key_frame_weight[i] * cpi->prior_key_frame_distance[i];
      total_weight += prior_key_frame_weight[i];
    }

    av_key_frame_frequency /= total_weight;
  }
  // TODO (marpan): Given the checks above, |av_key_frame_frequency|
  // should always be above 0. But for now we keep the sanity check in.
  if (av_key_frame_frequency == 0) av_key_frame_frequency = 1;
  return av_key_frame_frequency;
}

static void vp8_adjust_key_frame_context(VP8_COMP *cpi) {
  /* Do we have any key frame overspend to recover? */
  /* Two-pass overspend handled elsewhere. */
  if ((cpi->pass != 2) &&
      (cpi->projected_frame_size > cpi->per_frame_bandwidth)) {
    int overspend;

    /* Update the count of key frame overspend to be recovered in
     * subsequent frames. A portion of the KF overspend is treated as gf
     * overspend (and hence recovered more quickly) as the kf is also a
     * gf. Otherwise the few frames following each kf tend to get more
     * bits allocated than those following other gfs.
     */
    overspend = (cpi->projected_frame_size - cpi->per_frame_bandwidth);

    if (cpi->oxcf.number_of_layers > 1) {
      cpi->kf_overspend_bits += overspend;
    } else {
      cpi->kf_overspend_bits += overspend * 7 / 8;
      cpi->gf_overspend_bits += overspend * 1 / 8;
    }

    /* Work out how much to try and recover per frame. */
    cpi->kf_bitrate_adjustment =
        cpi->kf_overspend_bits / estimate_keyframe_frequency(cpi);
  }

  cpi->frames_since_key = 0;
  cpi->key_frame_count++;
}

void libvpx_vp8_compute_frame_size_bounds(VP8_COMP *cpi, int *frame_under_shoot_limit,
                                   int *frame_over_shoot_limit) {
  const int64_t this_frame_target = cpi->this_frame_target;
  int64_t over_shoot_limit, under_shoot_limit;

  if (cpi->common.frame_type == VP8_KEY_FRAME) {
    over_shoot_limit = this_frame_target * 9 / 8;
    under_shoot_limit = this_frame_target * 7 / 8;
  } else {
    if (cpi->oxcf.number_of_layers > 1 || cpi->common.refresh_alt_ref_frame ||
        cpi->common.refresh_golden_frame) {
      over_shoot_limit = this_frame_target * 9 / 8;
      under_shoot_limit = this_frame_target * 7 / 8;
    } else {
      /* For CBR take buffer fullness into account */
        if (cpi->buffer_level >= ((cpi->oxcf.optimal_buffer_level +
                                  cpi->oxcf.maximum_buffer_size) >>
                                  1)) {
          /* Buffer is too full so relax overshoot and tighten
           * undershoot
           */
          over_shoot_limit = this_frame_target * 12 / 8;
          under_shoot_limit = this_frame_target * 6 / 8;
        } else if (cpi->buffer_level <=
                   (cpi->oxcf.optimal_buffer_level >> 1)) {
          /* Buffer is too low so relax undershoot and tighten
           * overshoot
           */
          over_shoot_limit = this_frame_target * 10 / 8;
          under_shoot_limit = this_frame_target * 4 / 8;
        } else {
          over_shoot_limit = this_frame_target * 11 / 8;
          under_shoot_limit = this_frame_target * 5 / 8;
        }
    }
  }

    /* For very small rate targets where the fractional adjustment
     * (eg * 7/8) may be tiny make sure there is at least a minimum
     * range.
     */
    over_shoot_limit += 200;
    under_shoot_limit -= 200;
    if (under_shoot_limit < 0) under_shoot_limit = 0;
    if (under_shoot_limit > INT_MAX) under_shoot_limit = INT_MAX;
    if (over_shoot_limit > INT_MAX) over_shoot_limit = INT_MAX;
    *frame_under_shoot_limit = (int)under_shoot_limit;
    *frame_over_shoot_limit = (int)over_shoot_limit;
}

/* return of 0 means drop frame */
int libvpx_vp8_pick_frame_size(VP8_COMP *cpi) {
  VP8_COMMON *cm = &cpi->common;

  if (cm->frame_type == VP8_KEY_FRAME) {
    calc_iframe_target_size(cpi);
  } else {
    calc_pframe_target_size(cpi);

    /* Check if we're dropping the frame: */
    if (cpi->drop_frame) {
      cpi->drop_frame = 0;
      return 0;
    }
  }
  return 1;
}

// Libvpx: If this just encoded frame (mcomp/transform/quant, but before loopfilter and
// pack_bitstream) has large overshoot, and was not being encoded close to the
// max QP, then drop this frame and force next frame to be encoded at max QP.
// Allow this for screen_content_mode = 2, or if drop frames is allowed.
// TODO(marpan): Should do this exit condition during the encode_frame
// (i.e., halfway during the encoding of the frame) to save cycles.
//
// LibMebo: We are netither using screen_content_mode nor allowing drop frames
int libvpx_vp8_drop_encodedframe_overshoot(VP8_COMP *cpi, int Q) {
  cpi->frames_since_last_drop_overshoot++;
  return 0;
}
