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

#include "types.h"
#include "objdump_parser.h"

class profiler_t {
public:
  profiler_t(std::vector<std::string> objdump_paths,
      const cfg_t *cfg, bool halted,
      std::vector<std::pair<reg_t, abstract_mem_t*>> mems,
      std::vector<device_factory_t*> plugin_device_factories,
      const std::vector<std::string>& args,
      const debug_module_config_t &dm_config,
      const char *log_path,
      bool dtb_enabled,
      const char *dtb_file,
      bool socket_enabled,
      FILE *cmd_file);

  ~profiler_t();

  std::string find_user_space_binary(addr_t inst_va);

  int run();

  sim_lib_t* spike;

private:
  std::map<std::string, ObjdumpParser*> objdumps;
  std::map<reg_t, std::string> asid_to_bin;
};

#endif //__PROFILER_H__
