
#include <string>
#include <vector>
#include <map>

#include "types.h"
#include "objdump_parser.h"
#include "profiler.h"

#include <riscv/cfg.h>
#include <riscv/debug_module.h>
#include <riscv/devices.h>
#include <riscv/log_file.h>
#include <riscv/processor.h>
#include <riscv/simif.h>
#include <riscv/sim.h>
#include <riscv/sim_lib.h>


profiler_t::profiler_t(std::vector<std::string> objdump_paths,
      const cfg_t *cfg, bool halted,
      std::vector<std::pair<reg_t, abstract_mem_t*>> mems,
      std::vector<device_factory_t*> plugin_device_factories,
      const std::vector<std::string>& args,
      const debug_module_config_t &dm_config,
      const char *log_path,
      bool dtb_enabled,
      const char *dtb_file,
      bool socket_enabled,
      FILE *cmd_file)
{
  for (auto& p: objdump_paths) {
    objdumps.insert({p, new ObjdumpParser(p)});
  }

  spike = new sim_lib_t(cfg, halted, mems, plugin_device_factories, args,
                        dm_config, log_path, dtb_enabled, dtb_file,
                        socket_enabled, cmd_file);
}

profiler_t::~profiler_t() {
  for (auto& o : objdumps) {
    delete o.second;
  }
  objdumps.clear();
}

std::string profiler_t::find_user_space_binary(addr_t inst_va) {
}

int profiler_t::run() {
  spike->init();
  while (spike->target_running()) {
    spike->advance(1);
  }
  auto rc = spike->stop_sim();
  return rc;
}
