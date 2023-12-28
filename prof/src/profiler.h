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

#include "types.h"
#include "objdump_parser.h"
#include "thread_pool.h"
#include "disam.h"
#include "callstack_info.h"

namespace Profiler {

class Profiler : public sim_lib_t {
public:
  Profiler(std::vector<std::pair<std::string, std::string>> objdump_paths,
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
      const char* prof_outdir);

  ~Profiler();

  std::string find_launched_binary(processor_t* proc);

  virtual int run() override;

private:
  const addr_t MAX_FILENAME_SIZE = 200;

  bool user_space_addr(addr_t va);

  bool find_kernel_function(addr_t inst_va, std::string fname);
  bool find_kernel_do_execveat_common(addr_t inst_va); // search for exec system call
  bool find_kernel_set_mm_asid(addr_t inst_va);        // search for satp writes
                                                       //
  bool find_kernel_function_end(addr_t inst_va, std::string fname);

  void step_until_kernel_function(std::string fname, trace_t& trace);
  void step_until_insn(std::string type, trace_t& trace);

  state_t* get_state(int hartid);
  addr_t   get_va_pc(int hartid);

  std::map<std::string, ObjdumpParser*> objdumps;
  std::map<reg_t, std::string> asid_to_bin;
  std::map<std::string, unsigned int> riscv_abi;


  Disassembler disasm;


  bool called_by_do_execveat_common();
  CallStackInfo callstack_top();
  std::vector<CallStackInfo> fstack;

private:
  // Stuff for output logging
  std::string prof_outdir;
  uint64_t trace_idx = 0;
  void submit_trace_to_threadpool(trace_t& trace);
  ThreadPool loggers;
};

} // namespace Profiler

#endif //__PROFILER_H__
