libmebo_sources = [
  'libmebo.cpp',
  '../../libmebo_interface.cc',
]

libmebo_headers = [
  'libmebo.hpp',
  '../../libmebo_interface.h',
]

install_headers ('libmebo.hpp', subdir : 'libmebo')

libmebo  = shared_library('mebo',
  libmebo_sources,
  c_args : libmebo_args,
  include_directories: [configinc, libbrcinc],
  dependencies: [libbrc_dep , libbrc_controller_dep],
  version : libmebo_soname_version,
  soversion : libmebo_version_major,
  install : true,
  )

libmebo_dep_internal = declare_dependency(link_with: libmebo,
  include_directories : [configinc, libbrcinc],
  dependencies : [libbrc_dep, libbrc_controller_dep])

pkgconfig.generate(
  name : 'libmebo',
  description : 'Libraray for Media Encode Bitrate-control-algorithm Orchestration',
  version : meson.project_version(),
  libraries : libmebo,
)
