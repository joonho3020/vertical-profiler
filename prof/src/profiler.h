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
#include <riscv/sim_lib.h>
#include <riscv/processor.h>

#include "stack_unwinder.h"
#include "types.h"
#include "objdump_parser.h"
#include "thread_pool.h"
#include "disam.h"
#include "callstack_info.h"

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
      const char* prof_outdir,
      FILE *stackfile);

  ~Profiler();

  virtual int run() override;
  void process_callstack();

  // TODO : error checking if the function exists
  void add_kernel_function(Function* f);

  unsigned int   get_riscv_abi_ireg(std::string rname);
  ObjdumpParser* get_objdump_parser(std::string oname);

  std::vector<CallStackInfo>& get_callstack();
  std::map<reg_t, std::string>& get_asid2bin_map();

  void step_until_insn(std::string type, trace_t& trace);

private:
  std::map<addr_t, Function*> prof_pc_to_func;
  std::vector<addr_t>         prof_pc_func_start;
  std::vector<addr_t>         prof_pc_func_exit;

  inline bool found_registered_func_start_addr(addr_t va);
  inline bool found_registered_func_end_addr(addr_t va);

  bool   user_space_addr(addr_t va);
  addr_t kernel_function_start_addr(std::string fname);
  addr_t kernel_function_end_addr(std::string fname);

  std::map<std::string, ObjdumpParser*> objdumps;
  std::map<std::string, unsigned int> riscv_abi;

  Disassembler disasm;

  std::vector<CallStackInfo> fstack;


  std::map<reg_t, std::string> asid_to_bin;

private:
  // Stuff for output logging
  std::string prof_outdir;
  uint64_t trace_idx = 0;
  void submit_trace_to_threadpool(trace_t& trace);
  std::string spiketrace_filename(uint64_t idx);
  ThreadPool loggers;

private:
  // Stuff for stack unwinding
  StackUnwinder* stack_unwinder;
};

} // namespace Profiler

#endif //__PROFILER_H__
