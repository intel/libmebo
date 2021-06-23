project('LibMebo', 'c',
  version : '0.1.0',
  meson_version : '>= 0.30')

LIBMEBO_ENABLE_VP8 = get_option('with_vp8') != 'no'
LIBMEBO_ENABLE_VP9 = get_option('with_vp9') != 'no'
LIBMEBO_ENABLE_AV1 = get_option('with_av1') != 'no'

cdata = configuration_data()
cdata.set10('LIBMEBO_ENABLE_VP8', LIBMEBO_ENABLE_VP8)
cdata.set10('LIBMEBO_ENABLE_VP9', LIBMEBO_ENABLE_VP9)
cdata.set10('LIBMEBO_ENABLE_AV1', LIBMEBO_ENABLE_AV1)
configure_file(output: 'libmebo_config.h', configuration: cdata)

libmebo_args = ['-DHAVE_LIBMEBO_CONFIG_H']
configinc = include_directories ('.')
libbrcinc = include_directories ('src')
libmebo_inc = include_directories('src/lib')
libmebo_install_dir = '@0@/libmebo'.format(get_option('libdir'))

pkgconfig = import ('pkgconfig')

subdir('src')
subdir('test')

doxygen = find_program('doxygen', required: false)
if get_option('enable_docs') and doxygen.found()
  subdir('docs')
endif
