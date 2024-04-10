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
vp9_libbrc_headers = [] 
vp8_libbrc_headers = []
av1_headers = []

if LIBMEBO_ENABLE_VP9 
  vp9_libbrc_headers += [
      'vp9/libvpx_derived/libvpx_vp9_svc_layercontext.h',
      'vp9/libvpx_derived/libvpx_vp9_ratectrl.h',
      'vp9/libvpx_derived/libvpx_vp9_rtc.h',
      'vp9/libvpx_derived/libvpx_vp9_common.h',
      'vp9/libvpx_derived/libvpx_vp9_picklpf.h',
  ]
endif
if LIBMEBO_ENABLE_VP8
  vp8_libbrc_headers += [
      'vp8/libvpx_derived/libvpx_vp8_common.h',
      'vp8/libvpx_derived/libvpx_vp8_ratectrl.h',
      'vp8/libvpx_derived/libvpx_vp8_rtc.h',
  ]
endif
if LIBMEBO_ENABLE_AV1


libaom_av1_rc_dir = join_paths(meson.current_source_dir(), 'av1', 'aom_derived', 'libs/')
libaom_av1_rc = meson.get_compiler('cpp').find_library('aom_av1_rc', dirs:libaom_av1_rc_dir, required :false)

incdir = include_directories('av1/aom_derived/lib_header')
endif

ldflags = ['-lm']
add_global_link_arguments(ldflags, language : 'cpp')

libbrc  = static_library('libbrc',
  libbrc_sources  + vp9_libbrc_headers + vp8_libbrc_headers + av1_headers,
  c_args : libmebo_args,
  include_directories: [configinc, libbrcinc, incdir],
)

libbrc_dep = declare_dependency (link_with: libbrc,
   include_directories: [configinc, libbrcinc, incdir],
   dependencies: libaom_av1_rc,
   )
