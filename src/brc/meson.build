libbrc_sources = []
libbrc_headers = []

if LIBMEBO_ENABLE_VP9 
  libbrc_sources += [
      'vp9/libvpx_derived/libvpx_vp9_svc_layercontext.cpp',
      'vp9/libvpx_derived/libvpx_vp9_ratectrl.cpp',
      'vp9/libvpx_derived/libvpx_vp9_rtc.cpp',
      'vp9/libvpx_derived/libvpx_vp9_picklpf.cpp',
  ]
endif
if LIBMEBO_ENABLE_VP8
  libbrc_sources += [
      'vp8/libvpx_derived/libvpx_vp8_ratectrl.cpp',
      'vp8/libvpx_derived/libvpx_vp8_rtc.cpp',
  ]
endif
if LIBMEBO_ENABLE_AV1
  libbrc_sources += [
      'av1/aom_derived/aom_av1_ratectrl.cpp',
      'av1/aom_derived/aom_av1_svc_layercontext.cpp',
      'av1/aom_derived/aom_av1_rtc.cpp',
  ]
endif

if LIBMEBO_ENABLE_VP9 
  libbrc_headers += [
      'vp9/libvpx_derived/libvpx_vp9_svc_layercontext.h',
      'vp9/libvpx_derived/libvpx_vp9_ratectrl.h',
      'vp9/libvpx_derived/libvpx_vp9_rtc.h',
      'vp9/libvpx_derived/libvpx_vp9_common.h',
      'vp9/libvpx_derived/libvpx_vp9_picklpf.h',
  ]
endif
if LIBMEBO_ENABLE_VP8
  libbrc_headers += [
      'vp8/libvpx_derived/libvpx_vp8_common.h',
      'vp8/libvpx_derived/libvpx_vp8_ratectrl.h',
      'vp8/libvpx_derived/libvpx_vp8_rtc.h',
  ]
endif
if LIBMEBO_ENABLE_AV1
  libbrc_headers += [
      'av1/aom_derived/aom_av1_common.h',
      'av1/aom_derived/aom_av1_svc_layercontext.h',
      'av1/aom_derived/aom_av1_ratectrl.h',
      'av1/aom_derived/aom_av1_rtc.h',
  ]
endif

ldflags = ['-lm']
add_global_link_arguments(ldflags, language : 'cpp')

libbrc  = static_library('libbrc',
  libbrc_sources  + libbrc_headers,
  c_args : libmebo_args,
  include_directories: [configinc, libbrcinc],
)

libbrc_dep = declare_dependency (link_with: libbrc,
   include_directories: [configinc, libbrcinc],
   )
