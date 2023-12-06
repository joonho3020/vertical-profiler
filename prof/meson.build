project('profiler', 'cpp')

root_dir = meson.source_root()
conda_path=root_dir + '/../chipyard/.conda-env'

lib_deps = declare_dependency(
  link_args: ['-L' + conda_path, '-l:libdwarf.so', '-l:libelf.so']
  )

string_parser_lib = library('string_parser_lib',
  'src/string_parser.cc',
  dependencies : [lib_deps])

file_splitter_lib = library('file_splitter_lib',
  'src/file_splitter.cc',
  link_with : string_parser_lib,
  dependencies : [lib_deps])

executable('file_splitter',
  'src/file_splitter_main.cc',
  link_with : [
    string_parser_lib,
    file_splitter_lib
  ])

executable('main',
  [
    'src/main.cc',
    'src/trace_tracker.cc',
    'src/tracerv_dwarf.cc',
    'src/tracerv_elf.cc',
    'src/tracerv_processing.cc'
  ],
  link_with : [
    string_parser_lib,
    file_splitter_lib,
  ],
  dependencies : [lib_deps])
