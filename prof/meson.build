project('profiler', 'cpp')


add_global_arguments('-O3', language : 'cpp')

root_dir = meson.source_root()
conda_path=root_dir + '/../chipyard/.conda-env'
riscv_tools_path=conda_path + '/riscv-tools'
riscv_lib_path=riscv_tools_path + '/lib'
riscv_hdr_path='../chipyard/.conda-env/riscv-tools/include'
protobuf_install_path=root_dir + '/protobuf/install'
protobuf_hdr_path = 'protobuf/install/include'

lib_deps = declare_dependency(
  link_args: ['-L' + conda_path, '-l:libdwarf.so', '-l:libelf.so', '-lpthread']
  )

spike_lib_deps = declare_dependency(
  link_args: ['-ldl', '-L' + riscv_lib_path, '-Wl,-rpath,' + riscv_lib_path, '-lriscv', '-lfesvr', '-ldisasm', '-lfdt']
  )

spike_hdr_incs = include_directories(riscv_hdr_path)
protobuf_hdr_incs = include_directories(protobuf_hdr_path)

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


executable('profiler_main',
  [
    'src/profiler_main.cc',
    'src/profiler.cc',
    'src/string_parser.cc',
    'src/objdump_parser.cc',
    'src/thread_pool.cc'
  ],
  dependencies : [
    lib_deps, spike_lib_deps
  ],
  include_directories : [
    spike_hdr_incs,
    protobuf_hdr_incs
  ])

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

objdump_parser_test = executable('test_objdump_parser',
  'test/test_objdump_parser.cc',
  link_with : [
    string_parser_lib,
    objdump_parser_lib
  ])
test('objdump_parser test', objdump_parser_test)
