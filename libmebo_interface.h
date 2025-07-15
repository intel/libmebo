#pragma once
#ifdef __cplusplus
extern "C" {
#endif
#ifdef LIBVA_UTILS
#include "av1encode.h"
#endif
#include "src/lib/libmebo.hpp"


#ifdef __cplusplus
constexpr int LIBMEBO_SUCCESS = 0;
constexpr int LIBMEBO_FAILURE = -1;
#else
#define LIBMEBO_SUCCESS (0)
#define LIBMEBO_FAILURE (-1)
#endif

// Conditional definition for client integration
#ifndef USE_LIBMEBO
// Define the struct if client hasn't provided it
struct Av1InputParameters {
    const char* srcyuv;      // CPP.TY.6: const for read-only data
    const char* recyuv;
    const char* output;
    const char* libpath;
    
    // CPP.IA.1: Use explicit sized types
    uint32_t profile;
    uint32_t order_hint_bits;
    uint32_t enable_cdef;
    uint32_t width;
    uint32_t height;
    uint32_t LDB;
    uint32_t frame_rate_extN;
    uint32_t frame_rate_extD;
    uint32_t level;
    uint32_t bit_rate;
    uint8_t MinBaseQIndex;
    uint8_t MaxBaseQIndex;
    uint32_t intra_period;
    uint32_t ip_period;
    uint32_t RateControlMethod;
    uint32_t BRefType;
    int encode_syncmode;
    int calc_psnr;
    int frame_count;
    int frame_width_aligned;
    int frame_height_aligned;
    uint32_t base_qindex;
    int bit_depth;
    int target_bitrate;
    int vbr_max_bitrate;
    int buffer_size;
    int initial_buffer_fullness;
    int enswbrc;
};
#else
// Client (libva-utils) will provide the definition
struct Av1InputParameters;  // Forward declaration when client provides it
#endif


[[nodiscard]] void* CreateInitLibmebo(const struct Av1InputParameters ips) ;
[[nodiscard]] void getLoopfilterfromRc(void *libmebo_session, int lfdata[4]);
[[nodiscard]] void post_encode_update(void *libmebo_session, unsigned int consumed_bytes);
[[nodiscard]] int getQPfromRatectrl(void *libmebo_session, int current_frame_type);
void DestroyLibmebo(void* session_handle);
typedef void *(*createLibmeboRateController_t)(LibMeboCodecType CodecType,
                                               LibMeboBrcAlgorithmID algo_id);
typedef void *(*init_rate_controller_t)(LibMeboRateController *rc,
                                        LibMeboRateControllerConfig *rc_config);
typedef LibMeboStatus (*update_rate_control_t)(
    LibMeboRateController *rc, LibMeboRateControllerConfig *rc_cfg);
typedef LibMeboStatus (*post_encode_update_t)(LibMeboRateController *rc,
                                              uint64_t encoded_frame_size);
typedef LibMeboStatus (*compute_qp_t)(LibMeboRateController *rc,
                                      LibMeboRCFrameParams *rc_frame_params);
typedef LibMeboStatus (*get_qp_t)(LibMeboRateController *rc, int *qp);

typedef LibMeboStatus (*get_loop_filter_level_t)(LibMeboRateController *rc,
                                                 int *filter_level);
struct FunctionPtrstoLibmebo {
  void *handle;
  LibMeboRateController *libmebo_brc ;
  createLibmeboRateController_t ptrCreateLibmeboController;
  init_rate_controller_t ptrInit_rate_controller;
  update_rate_control_t ptrUpdate_rate_control;
  post_encode_update_t ptrPost_encode_update;
  compute_qp_t ptrCompute_qp ;
  get_qp_t ptrGet_qp ;
  get_loop_filter_level_t ptrGet_loop_filter_level ;
};


typedef enum ErrorsLoadingFunc {

  kNoError = 0,
  kMainHandleLibError = -1,
  kLibMeboControllerSymbolError = -2,
  kInitRateControllerSymbolError = -3,
  kUpdateRateControllerSymbolError = -4,
  kPostEncodeSymbolSymbolError = -5,
  kComputeQpSybmolError = -6,
  kGetQPSymbolError = -7,
  kGetLoopFilterSymbolError = -8,

} ErrosLibmeboSymbols;


#ifdef __cplusplus
}
#endif
