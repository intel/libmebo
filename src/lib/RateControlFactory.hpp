#pragma once
#include "../Handlers/AV1RateControlHandler.hpp"
#include "../Handlers/LibMeboControlHandler.hpp"
#include "../Handlers/VP8RateControlHandler.hpp"
#include "../Handlers/VP9RateControlHandler.hpp"
#include <memory>
#include <iostream>

class Libmebo_brc_factory {
public:
  static std::unique_ptr<LibMeboBrc> create(unsigned int id) {
    std::cout<<"creating Libmebo_brc_factory object"<<std::endl;
    LibMeboCodecType codecType = LIBMEBO_CODEC_UNKNOWN;
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
          static_cast<LibMeboBrcAlgorithmID>(
              id));
    case LIBMEBO_CODEC_VP9:
      return std::make_unique<LibmeboBrc_VP9>(
          static_cast<LibMeboBrcAlgorithmID>(id));
    case LIBMEBO_CODEC_AV1:
      std::cout<<"creating AV1 object"<<std::endl;
      return std::make_unique<LibmeboBrc_AV1>(
          static_cast<LibMeboBrcAlgorithmID>(id));
    case LIBMEBO_CODEC_UNKNOWN:
      return std::make_unique<LibmeboBrc_AV1>(
          static_cast<LibMeboBrcAlgorithmID>(id));
      break;
    }
  }
};