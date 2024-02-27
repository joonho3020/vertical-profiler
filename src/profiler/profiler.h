#ifndef __PROFILER_H__
#define __PROFILER_H__

#include <string>
#include <vector>
#include <map>
#include <riscv/cfg.h>
#include <riscv/debug_module.h>
#include <riscv/devices.h>
#include <riscv/log_file.h>
#include <riscv/processor.h>
#include <riscv/simif.h>
#include <riscv/sim.h>
#include <riscv/processor.h>

#include "../spike-top/sim_lib.h"
#include "stack_unwinder.h"
#include "types.h"
#include "objdump_parser.h"
#include "thread_pool.h"
#include "disam.h"
#include "callstack_info.h"
#include "perfetto_trace.h"
#include "profiler_state.h"

/* #define PROFILER_DEBUG */

namespace profiler {

class profiler_t : public sim_lib_t {
public:
  profiler_t(std::vector<std::pair<std::string, std::string>> objdump_paths,
      std::vector<std::pair<std::string, std::string>> dwarf_paths,
      const cfg_t *cfg, bool halted,
      std::vector<std::pair<reg_t, abstract_mem_t*>> mems,
      std::vector<device_factory_t*> plugin_device_factories,
      const std::vector<std::string>& args,
      const debug_module_config_t &dm_config,
      const char *log_path,
      bool dtb_enabled,
      const char *dtb_file,
      bool socket_enabled,
      FILE *cmd_file,
      bool checkpoint,
      const char* rtl_tracefile_name,
      std::string prof_tracedir,
      FILE *stackfile,
      FILE *proflogfile);

  ~profiler_t();


public:
  // user facing APIs
  virtual int run() override;
  void process_callstack();
  void add_kernel_func_to_profile(function_t* f, bool rewind_at_exit);

  FILE* get_prof_logfile() { return prof_logfile; }

  profiler_state_t* pstate;

private:
  bool user_space_addr(addr_t va);
  std::map<std::string, objdump_parser_t*> objdumps;
  disassembler_t disasm;

public:
  // APIs for updating the profiler state
  objdump_parser_t* get_objdump_parser(std::string oname);
  void step_until_insn(std::string type);
  void submit_packet(perfetto::packet_t pkt);

private:
  // Stuff for output logging and tracing
  std::string prof_tracedir;
  uint64_t trace_idx = 0;
  std::string spiketrace_filename(uint64_t idx);
  threadpool_t<trace_t, std::string> loggers;
  void submit_trace_to_threadpool(trace_t& trace);

  FILE* prof_logfile;

  std::vector<perfetto::packet_t> packet_traces;
  threadpool_t<std::vector<perfetto::packet_t>, FILE*> packet_loggers;
  const uint32_t PACKET_TRACE_FLUSH_THRESHOLD = 1000;
  FILE* proflog_tp;
  void submit_packet_trace_to_threadpool();

private:
  // Stuff for stack unwinding
  stack_unwinder_t* stack_unwinder;
};

} // namespace profiler_t

#endif //__PROFILER_H__
