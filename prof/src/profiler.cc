
#include <string>
#include <sys/types.h>
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
#include <riscv/mmu.h>

#include "types.h"
#include "objdump_parser.h"
#include "profiler.h"



profiler_t::profiler_t(
      std::vector<std::pair<std::string, std::string>> objdump_paths,
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
    objdumps.insert({p.first, new ObjdumpParser(p.second)});
  }

  spike = new sim_lib_t(cfg, halted, mems, plugin_device_factories, args,
                        dm_config, log_path, dtb_enabled, dtb_file,
                        socket_enabled, cmd_file);

  riscv_abi.insert({"x0" , 0 });
  riscv_abi.insert({"ra" , 1 });
  riscv_abi.insert({"sp" , 2 });
  riscv_abi.insert({"gp" , 3 });
  riscv_abi.insert({"tp" , 4 });
  riscv_abi.insert({"t0" , 5 });
  riscv_abi.insert({"t1" , 6 });
  riscv_abi.insert({"t2" , 7 });
  riscv_abi.insert({"s0" , 8 });
  riscv_abi.insert({"fp" , 8 });
  riscv_abi.insert({"s1" , 9 });
  riscv_abi.insert({"a0" , 10});
  riscv_abi.insert({"a1" , 11});
  riscv_abi.insert({"a2" , 12});
  riscv_abi.insert({"a3" , 13});
  riscv_abi.insert({"a4" , 14});
  riscv_abi.insert({"a5" , 15});
  riscv_abi.insert({"a6" , 16});
  riscv_abi.insert({"a7" , 17});
  riscv_abi.insert({"s2" , 18});
  riscv_abi.insert({"s3" , 19});
  riscv_abi.insert({"s4" , 20});
  riscv_abi.insert({"s5" , 21});
  riscv_abi.insert({"s6" , 22});
  riscv_abi.insert({"s7" , 23});
  riscv_abi.insert({"s8" , 24});
  riscv_abi.insert({"s9" , 25});
  riscv_abi.insert({"s10", 26});
  riscv_abi.insert({"s11", 27});
  riscv_abi.insert({"t3" , 28});
  riscv_abi.insert({"t4" , 29});
  riscv_abi.insert({"t5" , 30});
  riscv_abi.insert({"t6" , 31});
}

profiler_t::~profiler_t() {
  for (auto& o : objdumps) {
    delete o.second;
  }
  objdumps.clear();
}

std::string profiler_t::find_launched_binary(addr_t inst_va, processor_t* proc) {
  assert(!user_space_addr(inst_va));
  ObjdumpParser *obj = objdumps.find("kernel")->second;

  std::string farg_abi_reg = obj->func_args_reg(k_alloc_bprm,
                                                k_alloc_bprm_filename_arg);

  unsigned int reg_idx = riscv_abi[farg_abi_reg];
  state_t* state = proc->get_state();
  addr_t fname_addr = state->XPR[reg_idx];
  mmu_t* mmu = proc->get_mmu();

  std::cout << "find_launched_binary filename: 0x" << std::hex << fname_addr << std::endl;
  std::cout << "reg: " << farg_abi_reg << std::endl;

  uint8_t data;
  std::string fname;
  addr_t offset = 0;
  do {
    data = mmu->load<uint8_t>(fname_addr + offset);
    std::cout << data << ", ";
    fname.push_back((char)data);
    offset++;
  } while (data || offset < MAX_FILENAME_SIZE);

  std::cout << std::endl;
  std::cout << fname << std::endl;
  std::cout << "find_launched_binary done" << std::endl;
  return fname;
}

bool profiler_t::find_kernel_alloc_bprm(addr_t inst_va) {
  ObjdumpParser* obj = objdumps.find("kernel")->second;
  return (obj->get_func_start_va(k_alloc_bprm) == inst_va);
}

// Kernel takes up the upper virtual address
bool profiler_t::user_space_addr(addr_t va) {
  addr_t va_hi = va >> 32;
  return ((va_hi & 0xffffffff) == 0);
}

int profiler_t::run() {
  spike->init();
  while (spike->target_running()) {
    spike->advance(1);

    // FIXME : for single core
    processor_t* proc = spike->get_core(0);
    state_t* state = proc->get_state();
    addr_t va = state->pc;
    addr_t asid = proc->get_asid();

    if (user_space_addr(va)) {
      // print binary name for the current asid
      auto it = asid_to_bin.find(asid);
      if (it != asid_to_bin.end()) {
/* std::cout << it->second << std::endl; */
      }
    } else if (find_kernel_alloc_bprm(va)) {
      std::cout << "Found kernel_alloc_bprm" << std::endl;
      // map current asid with binary name
      spike->advance(1);
      std::string bin = find_launched_binary(va, proc);
      asid_to_bin.insert({asid, bin});
    } else {
      // do nothing
    }
  }
  auto rc = spike->stop_sim();
  return rc;
}
