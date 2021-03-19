headerdir = join_paths(meson.source_root(), 'src/lib')
footer = join_paths(meson.current_source_dir(), 'libmebo_footer.html')

libmebo_headers_doc = [
  'libmebo.h'
]

libmebo_doc_files = []
headers = ''
foreach h : libmebo_headers_doc
  libmebo_doc_files += join_paths (headerdir, h)
  headers += ' ' + join_paths (headerdir, h)
endforeach

config = configuration_data()
config.set('PACKAGE_VERSION', meson.project_version())
config.set('LIBMEBO_HEADER_DIR', headerdir)
config.set('LIBMEBO_HEADER_FILES', headers)
config.set('LIBMEBO_HTML_FOOTER', footer)
config.set('OUTDIR', meson.current_build_dir())

doxyfile = configure_file(
  input: 'Doxyfile.in',
  output: 'Doxyfile',
  configuration: config)

doc_install_dir = join_paths(get_option('datadir'), 'doc', meson.project_name())
custom_target(
  'doc',
  command: [ doxygen, doxyfile ],
  depend_files: libmebo_doc_files,
  input: doxyfile,
  output: 'html-out',
  install: true,
  install_dir: doc_install_dir)
