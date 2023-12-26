
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
      FILE *cmd_file,
      bool checkpoint)
  : sim_lib_t(cfg, halted, mems, plugin_device_factories, args, dm_config,
          log_path, dtb_enabled, dtb_file, socket_enabled, cmd_file, checkpoint)
{
  for (auto& p: objdump_paths) {
    objdumps.insert({p.first, new ObjdumpParser(p.second)});
  }

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

std::string profiler_t::find_launched_binary(processor_t* proc) {
  ObjdumpParser *obj = objdumps.find("kernel")->second;
  std::string farg_abi_reg = obj->func_args_reg(k_alloc_bprm,
                                                k_alloc_bprm_filename_arg);
  unsigned int reg_idx = riscv_abi[farg_abi_reg];
  state_t* state = proc->get_state();
  mmu_t* mmu = proc->get_mmu();

  addr_t filename_ptr = state->XPR[reg_idx];
  addr_t filename_struct = mmu->load<uint64_t>(filename_ptr);

#ifdef DEBUG
  pprintf("%s ptr 0x%" PRIx64 " *ptr 0x%" PRIx64 "\n",
        farg_abi_reg.c_str(), filename_ptr, filename_struct);
#endif

  uint8_t data;
  std::string name;
  addr_t offset = 0;
  do {
    data = mmu->load<uint8_t>(filename_struct + offset);
    name.push_back((char)data);
    offset++;
  } while ((data != 0) && (offset < MAX_FILENAME_SIZE));

  pprintf("find_launced_binary done %s\n", name.c_str());
  return name;
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
  init();

  size_t INTERLEAVE = 5000;
  size_t INSN_PER_CKPT = 100000;
  size_t INSNS_PER_RTC_TICK = 100;

  while (target_running()) {
    std::string protobuf;
    take_ckpt(protobuf);
    run_for(INSN_PER_CKPT);

    bool rewind = false;
    size_t fwd_steps = 0;
    std::vector<reg_t> pctrace = this->pctrace();
    for (size_t i = 0, cnt = pctrace.size(); i < cnt; i++) {
      if (find_kernel_alloc_bprm(pctrace[i])) {
        rewind = true;
        fwd_steps = i;
#ifdef DEBUG
        pprintf("rewind! fwd_steps: %" PRIu64 "/ %" PRIu64 "\n", fwd_steps, cnt);
#endif
        break;
      }
    }

    if (rewind) {
      size_t cur_steps = 0;
      load_ckpt(protobuf, false);

      size_t fastfwd_steps = (fwd_steps < INTERLEAVE) ? fwd_steps : fwd_steps - INTERLEAVE;
      run_for(fastfwd_steps);
      cur_steps = fastfwd_steps;
#ifdef DEBUG
      std::vector<reg_t> rewind_pctrace = this->pctrace();
      for (size_t i = 0, cnt = rewind_pctrace.size(); i < cnt; i++) {
        if (pctrace[i] != rewind_pctrace[i]) {
          printf("trace mismatch: %lu orig: 0x%" PRIx64 " rwd: 0x%" PRIx64 "\n",
              i, pctrace[i], rewind_pctrace[i]);
          exit(1);
        }
        if (find_kernel_alloc_bprm(rewind_pctrace[i])) {
          printf("fastfwd too much: pc 0x%" PRIx64 "\n", rewind_pctrace[i]);
          exit(1);
        }
      }
#endif

      do {
        run_for(1);
        cur_steps++;

        processor_t* proc = get_core(0);
        state_t* state = proc->get_state();
        addr_t va = state->pc;
        addr_t asid = proc->get_asid();

        if (user_space_addr(va)) {
          // print binary name for the current asid
          auto it = asid_to_bin.find(asid);
          if (it != asid_to_bin.end()) {
          }
        } else if (find_kernel_alloc_bprm(va)) {
#ifdef DEBUG
          pprintf("Found kernel_alloc_bprm, va: 0x%" PRIx64 "\n", va);
#endif
          // map current asid with binary name
          std::string bin = find_launched_binary(proc);
          asid_to_bin.insert({asid, bin});

          run_for(1);
          cur_steps++;
          break;
        } else {
          // do nothing
        }
      } while (true);
    }
  }

  auto rc = stop_sim();
  return rc;
}
