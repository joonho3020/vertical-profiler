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

objdump_parser_lib = library('objdump_parser_lib',
  'src/objdump_parser.cc',
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


string_parser_test = executable('test_string_parser',
  'test/test_string_parser.cc',
  link_with : string_parser_lib)

test('string_parser test', string_parser_test)

file_splitter_test = executable('test_file_splitter',
  'test/test_file_splitter.cc',
  link_with : [
    file_splitter_lib,
    string_parser_lib
    ])

test('file_splitter test', file_splitter_test)

objdump_parser_test = executable('objdump_parser_test',
  'test/test_objdump_parser.cc',
  link_with : [
    string_parser_lib,
    objdump_parser_lib
  ])
test('objdump_parser test', objdump_parser_test)
