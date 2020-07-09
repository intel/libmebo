libbrc_sources = [
  'vp9/libvpx_derived/vp9_svc_layercontext.c',
  'vp9/libvpx_derived/vp9_ratectrl.c',
  'vp9/libvpx_derived/ratectrl_rtc.c',
]

libbrc_headers = [
  'vp9/libvpx_derived/vp9_svc_layercontext.h',
  'vp9/libvpx_derived/vp9_ratectrl.h',
  'vp9/libvpx_derived/ratectrl_rtc.h',
  'vp9/libvpx_derived/vp9_common.h',
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