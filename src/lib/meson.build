libmebo_sources = [
  'libmebo.c',
]

libmebo_headers = [
  'libmebo.h',
]

install_headers ('libmebo.h', subdir : 'libmebo')

libmebo  = shared_library('mebo',
  libmebo_sources,
  include_directories: [libbrcinc],
  dependencies: [libbrc_dep],
  install : true,
  install_dir : libmebo_install_dir,)

pkgconfig.generate(
  name : 'libmebo',
  description : 'Libraray for Media Encode Bitrate-control-algorithm Orchestration',
  version : '0.1.0',
  libraries : libmebo,
)
