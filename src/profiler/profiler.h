#ifndef __PROFILER_H__
#define __PROFILER_H__

#include <string>
#include <vector>
#include <map>
#include <chrono>

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
#include "callstack_info.h"
#include "perfetto_trace.h"
#include "profiler_state.h"
#include "logger.h"

/* #define PROFILER_DEBUG */


#define GET_TIME() std::chrono::high_resolution_clock::now()

#define MEASURE_TIME(S, E, T) \
  T += std::chrono::duration_cast<std::chrono::microseconds>(E - S).count()

#define PRINT_TIME_STAT(N, T) \
  pprintf("Time (s) %s: %f\n", N, T / (1000 * 1000))

#define INCREMENT_CNTR(C) \
  C++

#define PRINT_CNTR_STAT(N, X) \
  pprintf("Cntr %s: %" PRIu64 "\n", N, X)

#define MEASURE_AVG_TIME(S, E, T, C) \
  MEASURE_TIME(S, E, T);             \
  INCREMENT_CNTR(C);

#define PRINT_AVG_TIME_STAT(N, T, C) \
  pprintf("Avg (us) %s: %f / %" PRIu64 "  = %f\n", N, T, C, T/C)



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
      const char* rtl_tracefile_name,
      std::string prof_outdir);

  ~profiler_t();


public:
  // user facing APIs
  virtual int run() override;
  virtual int run_from_trace() override;

  void profile_kernel_func_at_exit(function_t* f, std::vector<addr_t> evas);
  void profile_kernel_func_at_pc  (function_t* f, addr_t pc, std::vector<addr_t> evas);

  objdump_parser_t* get_objdump_parser(std::string oname);

  profiler_state_t* pstate();
  logger_t* logger();

  void process_callstack();
  reg_t get_pc(int hartid);

private:
  FILE* gen_outfile(std::string outdir, std::string filename);
  bool user_space_addr(addr_t pc);
  std::string get_binary_name(addr_t pc);

  std::map<std::string, objdump_parser_t*> objdumps_;
  profiler_state_t* pstate_;
  logger_t* logger_;

  // Stuff for stack unwinding
  stack_unwinder_t* stack_unwinder_;

  uint64_t SPIKE_LOG_FLUSH_PERIOD = 10000;
};

} // namespace profiler_t

#endif //__PROFILER_H__
