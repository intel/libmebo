libbrc_controller_sources = []
libbrc_controller_headers = []

# install_headers ('libmebo.h', subdir : 'libmebo')

libbrc_controller_sources += [
    'LibMeboControlHandler.cpp',
    'AV1RateControlHandler.cpp',
    'VP8RateControlHandler.cpp',
    'VP9RateControlHandler.cpp'
]

libbrc_controller_headers +=[
    'LibMeboControlHandler.hpp',
    'AV1RateControlHandler.hpp',
    'VP8RateControlHandler.hpp',
    'VP9RateControlHandler.hpp'
]

libaom_av1_rc_dir = join_paths(meson.current_source_dir(), 'av1', 'aom_derived', 'libs/')
libaom_av1_rc = meson.get_compiler('cpp').find_library('aom_av1_rc', dirs:libaom_av1_rc_dir, required :false)

incdir = include_directories('../brc/av1/aom_derived/lib_header')

libbrc_controller  = static_library('libbrc_controller',
  libbrc_controller_sources  + libbrc_controller_headers,
  c_args : libmebo_args,
  include_directories: [configinc, libbrcinc, incdir],
)

 libbrc_controller_dep = declare_dependency (link_with: libbrc_controller,
   include_directories: [configinc, libbrcinc, incdir],
   dependencies: libaom_av1_rc,
 )
  