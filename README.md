# LibMebo: Library for Media Encode bitrate-control-algorithm Orchestration.

LibMebo is an open source library for orchestrating the bitrate algorithms for an encoder pipeline. The encoder itself could be running on hardware or software.

See the [explainer](https://github.com/intel/libmebo/blob/master/explainer.md) for more info.

## License

libmebo is available under the terms the 3-Clause BSD License

## Sources

libmebo source code is available at <https://github.com/intel/libmebo>

### Details

Libmebo supports BRC algorithms for VP8, VP9 & AV1(experimental) codecs. Currently, we are focussing on video-conferencing applications and the only supported BRC model is Constant Bit Rate mode. The VP8 & VP9 brc algorithms are derived from the libvpx library [1]. A Work-in-progress algorithm is available for AV1 which is derived from the aom reference implementation [2]. Libmebo can support multiple implementations for the same codec too. We have already implemented the concept of using software brc with hardware encoder in chromium as part of the ChromeOS project [3]. Also, we have a sample middleware implementation to showcase the libmebo usage [4].

[1] https://chromium.googlesource.com/webm/libvpx/ \
[2] https://aomedia.googlesource.com/aom/ \
[3] https://bugs.chromium.org/p/chromium/issues/detail?id=1060775 \
[4] https://cgit.freedesktop.org/~sree/gstreamer-vaapi/commit/?h=libmebo&id=4d9af2f25bad1d3345ef629beddce11c28e17024 \

### How to build & install

Download the source: \
  git clone https://github.com/intel/libmebo \
\
cd libmbo\
meson build  [--prefix=InstallationDirectory] \
ninja -C build install \

### ToDo

-- Collect feedback from other open source media projects \
-- Add a common brc engine and interface for various codecs \
-- Add BRC algorithms for more codecs \
-- Add interface for binary blob brc algorithms.
