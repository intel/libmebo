libbrc_sources = []
libbrc_headers = []

if LIBMEBO_ENABLE_VP9 
  libbrc_sources += [
      'vp9/libvpx_derived/libvpx_vp9_svc_layercontext.c',
      'vp9/libvpx_derived/libvpx_vp9_ratectrl.c',
      'vp9/libvpx_derived/libvpx_vp9_rtc.c',
      'vp9/libvpx_derived/libvpx_vp9_picklpf.c',
  ]
endif
if LIBMEBO_ENABLE_VP8
  libbrc_sources += [
      'vp8/libvpx_derived/libvpx_vp8_ratectrl.c',
      'vp8/libvpx_derived/libvpx_vp8_rtc.c',
  ]
endif
if LIBMEBO_ENABLE_AV1
  libbrc_sources += [ ]
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
  libbrc_headers += [ ]
endif

ldflags = ['-lm']
add_global_link_arguments(ldflags, language : 'c')

libbrc  = shared_library('brc',
  libbrc_sources  + libbrc_headers,
  c_args : libmebo_args,
  include_directories: [configinc, libbrcinc],
  install:true
)

libbrc_dep = declare_dependency (link_with: libbrc,
   include_directories: [configinc, libbrcinc],
   )
