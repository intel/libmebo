#include "libmebo_interface.h"
#include "src/lib/RateControlFactory.hpp"
#include <dlfcn.h>
#include <iostream>
#include <memory>
#include <functional>


#ifdef USE_LIBMEBO
#include "av1encode.h"
#endif

struct FunctionPtrstoLibmebo FnPtrsToLibmebo;

int InitFuncPtrs(struct Av1InputParameters ips,
                 struct FunctionPtrstoLibmebo *pFnPtrsToLibmebo) {
  pFnPtrsToLibmebo->handle = dlopen(ips.libpath, RTLD_LAZY);
  if (!pFnPtrsToLibmebo->handle) {
    std::cout << "Cannot open the library libmebo.so\n";
    return kMainHandleLibError;
  }

  pFnPtrsToLibmebo->ptrCreateLibmeboController =
      (createLibmeboRateController_t)dlsym(pFnPtrsToLibmebo->handle,
                                           "libmebo_create_rate_controller");
  if (!pFnPtrsToLibmebo->ptrCreateLibmeboController) {
    std::cout << "Cannot load symbol 'libmebo_create_rate_controller' from "
                 "Libmebo.so\n";
    return kLibMeboControllerSymbolError;
  }

  pFnPtrsToLibmebo->ptrInit_rate_controller = (init_rate_controller_t)dlsym(
      pFnPtrsToLibmebo->handle, "libmebo_init_rate_controller");
  if (!pFnPtrsToLibmebo->ptrInit_rate_controller) {
    std::cout << "Cannot load symbol 'libmebo_init_rate_controller' from "
                 "Libmebo.so\n";
    return kInitRateControllerSymbolError;
  }

  pFnPtrsToLibmebo->ptrUpdate_rate_control = (update_rate_control_t)dlsym(
      pFnPtrsToLibmebo->handle, "libmebo_update_rate_controller_config");
  if (!pFnPtrsToLibmebo->ptrInit_rate_controller) {
    std::cout << "Cannot load symbol 'libmebo_update_rate_controller_config' "
                 "from Libmebo.so\n";
    return kUpdateRateControllerSymbolError;
  }

  pFnPtrsToLibmebo->ptrPost_encode_update = (post_encode_update_t)dlsym(
      pFnPtrsToLibmebo->handle, "libmebo_post_encode_update");
  if (!pFnPtrsToLibmebo->ptrPost_encode_update) {
    std::cout
        << "Cannot load symbol 'libmebo_post_encode_update' from Libmebo.so\n";
    return kPostEncodeSymbolSymbolError;
  }

  pFnPtrsToLibmebo->ptrCompute_qp =
      (compute_qp_t)dlsym(pFnPtrsToLibmebo->handle, "libmebo_compute_qp");
  if (!pFnPtrsToLibmebo->ptrCompute_qp) {
    std::cout << "Cannot load symbol 'libmebo_compute_qp' from Libmebo.so\n";
    return kComputeQpSybmolError;
  }

  pFnPtrsToLibmebo->ptrGet_qp =
      (get_qp_t)dlsym(pFnPtrsToLibmebo->handle, "libmebo_get_qp");
  if (!pFnPtrsToLibmebo->ptrGet_qp) {
    std::cout << "Cannot load symbol 'libmebo_get_qp' from Libmebo.so\n";
    return kGetQPSymbolError;
  }

  pFnPtrsToLibmebo->ptrGet_loop_filter_level = (get_loop_filter_level_t)dlsym(
      pFnPtrsToLibmebo->handle, "libmebo_get_loop_filter_level");
  if (!pFnPtrsToLibmebo->ptrGet_loop_filter_level) {
    std::cout << "Cannot load symbol 'libmebo_get_loop_filter_level' from "
                 "Libmebo.so\n";
    return kGetLoopFilterSymbolError;
  }
  std::cout << "All symbols from libmebo.so loaded successfully\n";
  return kNoError;
}

void InitRcCofnig(const struct Av1InputParameters ips,
                  LibMeboRateControllerConfig *rc_config) {
  constexpr int kMinQP = 10;
  constexpr int kMaxQP = 56;
  rc_config->width = ips.width;
  rc_config->height = ips.height;
  // third_party/webrtc/modules/video_coding/codecs/av1/libaom_av1_encoder.cc
  rc_config->max_quantizer = kMaxQP;
  rc_config->min_quantizer = kMinQP;

  rc_config->buf_initial_sz = 600;
  rc_config->buf_optimal_sz = 500;
  rc_config->target_bandwidth = ips.target_bitrate;
  rc_config->buf_sz = 1000;
  rc_config->undershoot_pct = 25;
  rc_config->overshoot_pct = 50;
  rc_config->max_intra_bitrate_pct = 300;
  rc_config->max_inter_bitrate_pct = 50;
  rc_config->framerate = 60;
  rc_config->layer_target_bitrate[0] = ips.target_bitrate;

  rc_config->ts_rate_decimator[0] = 1;
  rc_config->ss_number_layers = 1;
  rc_config->ts_number_layers = 1;
  rc_config->max_quantizers[0] = kMaxQP;
  rc_config->min_quantizers[0] = kMinQP;
  rc_config->scaling_factor_num[0] = 1;
  rc_config->scaling_factor_den[0] = 1;
}

// RAII Session class - no globals
class LibmeboSession {
private:
    std::unique_ptr<FunctionPtrstoLibmebo, std::function<void(FunctionPtrstoLibmebo*)>> fnPtrs_;
    LibMeboRateController* controller_;

public:
    explicit LibmeboSession(const struct Av1InputParameters& ips) 
        : controller_(nullptr) {
        fnPtrs_ = std::unique_ptr<FunctionPtrstoLibmebo, std::function<void(FunctionPtrstoLibmebo*)>>(
            new FunctionPtrstoLibmebo{},
            [](FunctionPtrstoLibmebo* ptr) {
                if (ptr && ptr->handle) {
                    dlclose(ptr->handle);
                }
                delete ptr;
            }
        );
        
        const int result = InitFuncPtrs(ips, fnPtrs_.get());
        if (result != kNoError) {
            std::cout<<"Failed to initialize function pointers"<<std::endl;
        }
    }

    // Delete copy constructor and assignment operator
    LibmeboSession(const LibmeboSession&) = delete;
    LibmeboSession& operator=(const LibmeboSession&) = delete;

    [[nodiscard]] int createController(const struct Av1InputParameters& ips) {
        LibMeboRateControllerConfig rc_config;
        InitRcCofnig(ips, &rc_config);

        controller_ = static_cast<LibMeboRateController*>(
            fnPtrs_->ptrCreateLibmeboController(
                LIBMEBO_CODEC_AV1, static_cast<LibMeboBrcAlgorithmID>(2)));

        if (!controller_) {
            return kLibMeboControllerSymbolError;
        }

        const auto init_result = fnPtrs_->ptrInit_rate_controller(controller_, &rc_config);
        if (init_result == nullptr) {
            return kInitRateControllerSymbolError;
        }

        return kNoError;
    }

    FunctionPtrstoLibmebo* getFnPtrs() const { return fnPtrs_.get(); }
    LibMeboRateController* getController() const { return controller_; }
};


[[nodiscard]] void* CreateInitLibmebo(const struct Av1InputParameters ips) {
    auto session = std::make_unique<LibmeboSession>(ips);
    if (!session) {
        std::cout<<"session didnt create\n";
        return nullptr;
    }

    const int result = session->createController(ips);
    if (result != kNoError) {
        return nullptr;
    }
    // Transfer ownership to caller
    return session.release();

}

void DestroyLibmebo(void* session_handle) {
    if (session_handle) {
        delete static_cast<LibmeboSession*>(session_handle);
    }
}

/*
int CreateInitLibmebo(struct Av1InputParameters ips) {

  LibMeboRateControllerConfig rc_config;
  LibMeboCodecType CodecType = LIBMEBO_CODEC_AV1;
  unsigned int algo_id = 2;

  int result = InitFuncPtrs(ips, &FnPtrsToLibmebo);

  if (result != kNoError) {
    std::cout << " Cannot load lib or symbol, error code is " << result << "-"
              << dlerror() << std::endl;
  }

  InitRcCofnig(ips, &rc_config); // creating rc config.

  LibMeboRateController *libmebo_brc;
  libmebo_brc = reinterpret_cast<LibMeboRateController *>(
      FnPtrsToLibmebo.ptrCreateLibmeboController(
          CodecType, static_cast<LibMeboBrcAlgorithmID>(algo_id)));

  if (nullptr == libmebo_brc) {
    std::cout << "Libmebo_brc factory object creation failed \n";
  }

  std::cout
      << "till this time, created the constructor to libmebo_brc and av1\n";

  FnPtrsToLibmebo.ptrInit_rate_controller(libmebo_brc, &rc_config);
  return 0;
}
*/
/* this function is not required.*/
int myfunc_test(struct Av1InputParameters ips) {

  LibMeboRateControllerConfig rc_config;
  LibMeboCodecType CodecType = LIBMEBO_CODEC_AV1;
  unsigned int algo_id = 2;
  uint64_t frame_size_bytes = 600;
  LibMeboRCFrameParams rc_frame_params;
  int qp;
  int filter_level[4];

  rc_frame_params.frame_type = LIBMEBO_KEY_FRAME;

  int result = InitFuncPtrs(ips, &FnPtrsToLibmebo);

  if (result != kNoError) {
    std::cout << " Cannot load lib or symbol, error code is " << result << "-"
              << dlerror() << std::endl;
  }

  InitRcCofnig(ips, &rc_config); // creating rc config.

  LibMeboRateController *libmebo_brc;
  libmebo_brc = reinterpret_cast<LibMeboRateController *>(
      FnPtrsToLibmebo.ptrCreateLibmeboController(
          CodecType, static_cast<LibMeboBrcAlgorithmID>(algo_id)));

  if (nullptr == libmebo_brc) {
    std::cout << "Libmebo_brc factory object creation failed \n";
  }

  std::cout
      << "till this time, created the construcotr for libmebo_brc and forav1\n";

  FnPtrsToLibmebo.ptrInit_rate_controller(libmebo_brc, &rc_config);
  FnPtrsToLibmebo.ptrUpdate_rate_control(libmebo_brc, &rc_config);
  FnPtrsToLibmebo.ptrPost_encode_update(libmebo_brc, frame_size_bytes);
  FnPtrsToLibmebo.ptrCompute_qp(libmebo_brc, &rc_frame_params);
  FnPtrsToLibmebo.ptrGet_qp(libmebo_brc, &qp);
  FnPtrsToLibmebo.ptrGet_loop_filter_level(libmebo_brc, filter_level);
  return 0;
}
#if 0
int getQPfromRatectrl(struct Av1InputParameters ips, int current_frame_type) {
  LibMeboRCFrameParams rc_frame_params;
  int qp;
  LibMeboCodecType CodecType = LIBMEBO_CODEC_AV1;
  unsigned int algo_id = 2;
  const bool is_keyframe = current_frame_type;

    LibMeboRateController *libmebo_brc;
  libmebo_brc = reinterpret_cast<LibMeboRateController *>(
      FnPtrsToLibmebo.ptrCreateLibmeboController(
          CodecType, static_cast<LibMeboBrcAlgorithmID>(algo_id)));
  int result = InitFuncPtrs(ips, &FnPtrsToLibmebo);



  rc_frame_params.frame_type = LIBMEBO_KEY_FRAME; // default.
  if (rc_frame_params.frame_type == is_keyframe) {
    rc_frame_params.frame_type = LIBMEBO_KEY_FRAME;
  } else {
    rc_frame_params.frame_type = LIBMEBO_INTER_FRAME;
  }
  rc_frame_params.spatial_layer_id = 0;
  rc_frame_params.temporal_layer_id = 0;
  FnPtrsToLibmebo.ptrCompute_qp(libmebo_brc, &rc_frame_params);
  FnPtrsToLibmebo.ptrGet_qp(libmebo_brc, &qp);
  return (qp);
}

int getQPfromRatectrl(int current_frame_type) {
  LibMeboRCFrameParams rc_frame_params;
  int qp;

  const bool is_keyframe = current_frame_type;

  rc_frame_params.frame_type = LIBMEBO_KEY_FRAME; // default.
  if (rc_frame_params.frame_type == is_keyframe) {
    rc_frame_params.frame_type = LIBMEBO_KEY_FRAME;
  } else {
    rc_frame_params.frame_type = LIBMEBO_INTER_FRAME;
  }
  rc_frame_params.spatial_layer_id = 0;
  rc_frame_params.temporal_layer_id = 0;
  FnPtrsToLibmebo.ptrCompute_qp(libmebo_brc, &rc_frame_params);
  FnPtrsToLibmebo.ptrGet_qp(libmebo_brc, &qp);
  return (qp);
}

void getLoopfilterfromRc(int lfdata[4]) {
  FnPtrsToLibmebo.ptrGet_loop_filter_level(libmebo_brc, lfdata);
}

void post_encode_update(unsigned int consumed_bytes) {
  FnPtrsToLibmebo.ptrPost_encode_update(libmebo_brc, consumed_bytes);
}
#endif

[[nodiscard]] int getQPfromRatectrl(void* session_handle, int current_frame_type) {
    if (!session_handle) {
        return -1;
    }

    auto* session = static_cast<LibmeboSession*>(session_handle);
    if (!session->getController()) {
        return -1;
    }

    LibMeboRCFrameParams rc_frame_params{};
    rc_frame_params.frame_type = (current_frame_type != 0) ? 
                                LIBMEBO_KEY_FRAME : LIBMEBO_INTER_FRAME;
    rc_frame_params.spatial_layer_id = 0;
    rc_frame_params.temporal_layer_id = 0;

    int qp = -1;
    const auto compute_result = session->getFnPtrs()->ptrCompute_qp(
        session->getController(), &rc_frame_params);
    if (compute_result != LIBMEBO_SUCCESS) {
        return -1;
    }

    const auto get_result = session->getFnPtrs()->ptrGet_qp(
        session->getController(), &qp);
    if (get_result != LIBMEBO_SUCCESS) {
        return -1;
    }

    return qp;
}

void getLoopfilterfromRc(void* session_handle, int lfdata[4]) {
    if (!session_handle) {
        return;
    }

    auto* session = static_cast<LibmeboSession*>(session_handle);
    if (!session->getController()) {
        return;
    }

    session->getFnPtrs()->ptrGet_loop_filter_level(session->getController(), lfdata);
}

void post_encode_update(void* session_handle, unsigned int consumed_bytes) {
    if (!session_handle) {
        return;
    }

    auto* session = static_cast<LibmeboSession*>(session_handle);
    if (!session->getController()) {
        return;
    }

    session->getFnPtrs()->ptrPost_encode_update(session->getController(), consumed_bytes);
}