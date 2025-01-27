#pragma once
#include "../src/Handlers/AV1RateControlHandler.hpp"
#include "../src/Handlers/LibMeboControlHandler.hpp"
#include "../src/Handlers/VP8RateControlHandler.hpp"
#include "../src/Handlers/VP9RateControlHandler.hpp"
#include <memory>

class Libmebo_brc_factory {
public:
  static std::unique_ptr<LibmeboBrc> create(unsigned int id) {
    LibMeboCodecType codecType;
    switch (static_cast<LibMeboCodecType>(id)) {
    case LIBMEBO_CODEC_VP8:
      codecType = LIBMEBO_CODEC_VP8;
      break;
    case LIBMEBO_CODEC_VP9:
      codecType = LIBMEBO_CODEC_VP9;
      break;
    case LIBMEBO_CODEC_AV1:
      codecType = LIBMEBO_CODEC_AV1;
      break;
    case LIBMEBO_CODEC_UNKNOWN:
      codecType = LIBMEBO_CODEC_AV1; // defult to av1 -only av1 is implemented
      break;
    }

    switch (codecType) {
    case LIBMEBO_CODEC_VP8:
      return std::make_unique<LibmeboBrc_VP8>(
          static_cast<LibmeboBrcAlgorithmID>(
              id)); // this is calling construcotr.
    case LIBMEBO_CODEC_VP9:
      return std::make_unique<LibmeboBrc_VP9>(
          static_cast<LibmeboBrcAlgorithmID>(id));
    case LIBMEBO_CODEC_AV1:
      return std::make_unique<LibmeboBrc_AV1>(
          static_cast<LibmeboBrcAlgorithmID>(id));
    case LIBMEBO_CODEC_UNKNOWN:
      return std::make_unique<LibmeboBrc_AV1>(
          static_cast<LibmeboBrcAlgorithmID>(id));
      break;
    }
  }
};