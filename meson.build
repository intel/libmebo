project('LibMebo', 'c',
  version : '0.1.0',
  meson_version : '>= 0.30')

libbrcinc = include_directories ('src')
libmebo_install_dir = '@0@/libmebo'.format(get_option('libdir'))

pkgconfig = import ('pkgconfig')

subdir('src')
