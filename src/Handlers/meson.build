libbrc_controller_sources = []
libbrc_controller_headers = []

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

c_compiler = meson.get_compiler('c')
cpp_compiler = meson.get_compiler('cpp')


dl_dep_c = c_compiler.find_library('dl', required:true)
dl_dep_cpp = cpp_compiler.find_library('dl', required:true)


brc_controller  = shared_library('brc_controller',
  libbrc_controller_sources  + libbrc_controller_headers,  
  c_args : libmebo_args,
  include_directories: [configinc, libbrcinc],
  link_args: '-ldl',
  dependencies: [dl_dep_c, dl_dep_cpp, libbrc_dep],
  install:true,
)

 libbrc_controller_dep = declare_dependency (link_with: brc_controller,
   include_directories: [configinc, libbrcinc],
   dependencies: [dl_dep_c, dl_dep_cpp, libbrc_dep]
 )
  