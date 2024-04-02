libmebo_sources = [
  'libmebo.cpp',
]

libmebo_headers = [
  'libmebo.h',
]

install_headers ('libmebo.h', subdir : 'libmebo')

libmebo  = shared_library('mebo',
  libmebo_sources,
  c_args : libmebo_args,
  include_directories: [configinc, libbrcinc],
  dependencies: [libbrc_dep],
  version : libmebo_soname_version,
  soversion : libmebo_version_major,
  install : true,
  install_dir : libmebo_install_dir,)

libmebo_dep_internal = declare_dependency(link_with: libmebo,
  include_directories : [configinc, libbrcinc],
  dependencies : [libbrc_dep])

pkgconfig.generate(
  name : 'libmebo',
  description : 'Libraray for Media Encode Bitrate-control-algorithm Orchestration',
  version : meson.project_version(),
  libraries : libmebo,
)
