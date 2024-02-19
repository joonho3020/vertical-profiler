project('profiler', 'cpp')


add_global_arguments('-O3', language : 'cpp')

root_dir = meson.source_root()
conda_path=root_dir + '/../chipyard/.conda-env'
riscv_tools_path=conda_path + '/riscv-tools'
riscv_lib_path=riscv_tools_path + '/lib'
riscv_hdr_path='../chipyard/.conda-env/riscv-tools/include'
protobuf_install_path=root_dir + '/protobuf/install'
protobuf_hdr_path = 'protobuf/install/include'
protobuf_lib_path = protobuf_install_path + '/lib/libprotobuf.a'

lib_deps = declare_dependency(link_args: ['-L' + conda_path, '-l:libdwarf.so', '-l:libelf.so', '-lpthread'])
spike_lib_deps = declare_dependency(link_args:['-ldl', '-L' + riscv_lib_path, '-Wl,-rpath,' + riscv_lib_path, '-lriscv', '-lfesvr', '-ldisasm', '-lfdt'])
protobuf_lib_deps = declare_dependency(link_args: [protobuf_lib_path])

spike_hdr_incs = include_directories(riscv_hdr_path)
protobuf_hdr_incs = include_directories(protobuf_hdr_path)

string_parser_lib = library('string_parser_lib',
  'lib/string_parser.cc',
  dependencies : [lib_deps])

objdump_parser_lib = library('objdump_parser_lib',
  'src/objdump_parser.cc',
  link_with : string_parser_lib,
  dependencies : [lib_deps])

tracerv_lib = library('tracerv_lib',
  [
    'src/tracerv_dwarf.cc',
    'src/tracerv_elf.cc',
    'src/tracerv_processing.cc',
    'src/trace_tracker.cc'
  ],
  dependencies : [lib_deps])

spike_top_lib = library('spike_top_lib',
  [
    'spike-top/processor_lib.cc',
    'spike-top/sim_lib.cc',
    'spike-top/ganged_devices.cc',
    'spike-top/arch-state.pb.cc'
  ],
  link_with : [
    string_parser_lib
  ],
  dependencies : [
    lib_deps, spike_lib_deps, protobuf_lib_deps
  ],
  include_directories : [
    spike_hdr_incs,
    protobuf_hdr_incs
  ])

perfetto_trace_lib = library('perfetto_trace_lib',
  [
    'src/perfetto_trace.cc'
  ])

executable('spike_lib_main',
  [
    'spike-top/spike_lib.cc'
  ],
  link_with : [spike_top_lib],
  dependencies : [lib_deps, spike_lib_deps, protobuf_lib_deps],
  include_directories : [
    spike_hdr_incs,
    protobuf_hdr_incs
  ])

executable('profiler_main',
  [
    'src/profiler_main.cc',
    'src/profiler.cc',
    'src/thread_pool.cc',
    'src/disasm.cc',
    'src/callstack_info.cc',
    'src/stack_unwinder.cc',
    'src/perfetto_trace.cc'
  ],
  link_with : [
    tracerv_lib,
    string_parser_lib,
    objdump_parser_lib,
    spike_top_lib
  ],
  dependencies : [
    lib_deps, spike_lib_deps
  ],
  include_directories : [
    spike_hdr_incs,
    protobuf_hdr_incs
  ])

executable('stack_unwinder_main',
  [
    'src/stack_unwinder_main.cc',
    'src/stack_unwinder.cc'
  ],
  link_with : [
    string_parser_lib,
    tracerv_lib
  ],
  dependencies : [lib_deps])

string_parser_test = executable('test_string_parser',
  'test/test_string_parser.cc',
  link_with : string_parser_lib)

test('string_parser test', string_parser_test)

objdump_parser_test = executable('test_objdump_parser',
  'test/test_objdump_parser.cc',
  link_with : [
    string_parser_lib,
    objdump_parser_lib
  ])
test('objdump_parser test', objdump_parser_test)

perfetto_trace_test = executable('test_perfetto_trace',
  'test/test_perfetto_trace.cc',
  link_with : [
    perfetto_trace_lib
  ])
test('perfetto_trace test', perfetto_trace_test)
