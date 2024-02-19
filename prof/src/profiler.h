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

namespace Profiler {

class Profiler : public sim_lib_t {
public:
  Profiler(std::vector<std::pair<std::string, std::string>> objdump_paths,
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

  ~Profiler();


public:
  // user facing APIs
  virtual int run() override;
  void process_callstack();
  void add_kernel_func_to_profile(Function* f, bool rewind_at_exit);

  FILE* get_prof_logfile() { return prof_logfile; }

private:
  inline bool found_registered_func_start_addr(addr_t va);
  inline bool found_registered_func_end_addr(addr_t va);
  bool   user_space_addr(addr_t va);

private:
  std::map<std::string, ObjdumpParser*> objdumps;
  Disassembler disasm;

  // Profiler state
  std::map<addr_t, Function*> prof_pc_to_func;
  std::vector<addr_t>         func_pc_prof_start;
  std::vector<addr_t>         func_pc_prof_exit;

  std::map<pid_t, std::vector<CallStackInfo>> fstacks;
  std::map<reg_t, std::string> asid_to_bin;

  // fork : add a new pid to bin mapping. the binary should be from the parent pid
  // exec : update the existing pid to bin mapping
  std::map<pid_t, std::string> pid_to_bin;


  // NOTE : There can be times when cur_pid and the pid from Spike does not
  // match. This is because we are updating cur_pid whenever CFS makes a
  // scheduling decision. However, the kernel has to perform the
  // "context_switch" function after the scheduling decision is made to
  // update the PID. Hence in this case, the value of cur_pid is one step
  // ahead of the functional sim's PID.
  pid_t cur_pid = 0;
  reg_t insn_retired = 0;

public:
  // APIs for updating the profiler state
  ObjdumpParser* get_objdump_parser(std::string oname);
  std::vector<CallStackInfo>& get_callstack(pid_t pid);
  std::map<reg_t, std::string>& get_asid2bin_map();
  std::map<pid_t, std::string>& get_pid2bin_map();
  void  set_curpid(pid_t pid);
  pid_t get_curpid();

  void step_until_insn(std::string type, trace_t& trace);

  reg_t get_insn_retired() { return insn_retired; }

  void submit_packet(Perfetto::TracePacket pkt);


private:
  // Stuff for output logging and tracing
  std::string prof_tracedir;
  uint64_t trace_idx = 0;
  std::string spiketrace_filename(uint64_t idx);
  ThreadPool<trace_t, std::string> loggers;
  void submit_trace_to_threadpool(trace_t& trace);

  FILE* prof_logfile;

  std::vector<Perfetto::TracePacket> packet_traces;
  ThreadPool<std::vector<Perfetto::TracePacket>, FILE*> packet_loggers;
  const uint32_t PACKET_TRACE_FLUSH_THRESHOLD = 1000;
  FILE* proflog_tp;
  void submit_packet_trace_to_threadpool();

private:
  // Stuff for stack unwinding
  StackUnwinder* stack_unwinder;
};

} // namespace Profiler

#endif //__PROFILER_H__
