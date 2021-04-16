libbrc_sources = [
  'vp9/libvpx_derived/libvpx_vp9_svc_layercontext.c',
  'vp9/libvpx_derived/libvpx_vp9_ratectrl.c',
  'vp9/libvpx_derived/libvpx_vp9_rtc.c',
  'vp9/libvpx_derived/libvpx_vp9_picklpf.c',
  'vp8/libvpx_derived/libvpx_vp8_ratectrl.c',
  'vp8/libvpx_derived/libvpx_vp8_rtc.c',
  'av1/aom_derived/aom_av1_ratectrl.c',
  'av1/aom_derived/aom_av1_svc_layercontext.c',
  'av1/aom_derived/aom_av1_rtc.c',
]

libbrc_headers = [
  'vp9/libvpx_derived/libvpx_vp9_svc_layercontext.h',
  'vp9/libvpx_derived/libvpx_vp9_ratectrl.h',
  'vp9/libvpx_derived/libvpx_vp9_rtc.h',
  'vp9/libvpx_derived/libvpx_vp9_common.h',
  'vp9/libvpx_derived/libvpx_vp9_picklpf.h',
  'vp8/libvpx_derived/libvpx_vp8_common.h',
  'vp8/libvpx_derived/libvpx_vp8_ratectrl.h',
  'vp8/libvpx_derived/libvpx_vp8_rtc.h',
  'av1/aom_derived/aom_av1_common.h',
  'av1/aom_derived/aom_av1_svc_layercontext.h',
  'av1/aom_derived/aom_av1_ratectrl.h',
  'av1/aom_derived/aom_av1_rtc.h',
]

ldflags = ['-lm']
add_global_link_arguments(ldflags, language : 'c')

libbrc  = static_library('libbrc',
  libbrc_sources  + libbrc_headers,
  include_directories: [libbrcinc],
)

libbrc_dep = declare_dependency (link_with: libbrc,
   include_directories: [libbrcinc],
   )
