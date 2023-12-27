
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
#include "thread_pool.h"
#include "string_parser.h"
#include "profiler.h"

namespace Profiler {

Profiler::Profiler(
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
      bool checkpoint,
      const char *prof_outdir)
  : sim_lib_t(cfg, halted, mems, plugin_device_factories, args, dm_config,
          log_path, dtb_enabled, dtb_file, socket_enabled, cmd_file, checkpoint),
    prof_outdir(prof_outdir)
{
  for (auto& p: objdump_paths) {
    objdumps.insert({p.first, new ObjdumpParser(p.second)});
  }

  loggers.start();

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

Profiler::~Profiler() {
  for (auto& o : objdumps) {
    delete o.second;
  }
  objdumps.clear();
}

std::string Profiler::find_launched_binary(processor_t* proc) {
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

  addr_t va = state->pc;
  addr_t asid = proc->get_asid();
  pprintf("find_launced_binary done %s va: 0x% " PRIx64 " asid: %" PRIu64 "\n",
          name.c_str(), va, asid);
  return name;
}

bool Profiler::find_kernel_alloc_bprm(addr_t inst_va) {
  ObjdumpParser* obj = objdumps.find("kernel")->second;
  return (obj->get_func_start_va(k_alloc_bprm) == inst_va);
}

// Kernel takes up the upper virtual address
bool Profiler::user_space_addr(addr_t va) {
  addr_t va_hi = va >> 32;
  return ((va_hi & 0xffffffff) == 0);
}

int Profiler::run() {
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
    trace_t pctrace = this->run_trace();
    for (size_t i = 0, cnt = pctrace.size(); i < cnt; i++) {
      if (find_kernel_alloc_bprm(pctrace[i].pc)) {
        rewind = true;
        fwd_steps = i;
        pctrace.clear();
        break;
      }
    }

    if (rewind) {
      trace_t rewind_trace;
      size_t cur_steps = 0;
      load_ckpt(protobuf, false);

      size_t fastfwd_steps = (fwd_steps < INTERLEAVE) ?
                              fwd_steps :
                              fwd_steps - INTERLEAVE;

      run_for(fastfwd_steps);
      rewind_trace = this->run_trace();
      pctrace.insert(pctrace.end(), rewind_trace.begin(), rewind_trace.end());
      cur_steps = fastfwd_steps;

      do {
        run_for(1);
        rewind_trace = this->run_trace();
        pctrace.insert(pctrace.end(), rewind_trace.begin(), rewind_trace.end());
        cur_steps++;

        processor_t* proc = get_core(0);
        state_t* state = proc->get_state();
        addr_t va = state->pc;
        addr_t asid = proc->get_asid();

        if (user_space_addr(va)) {
          printf("0x%" PRIx64 " asid: %" PRIu64 "\n", va, asid);
          // print binary name for the current asid
          auto it = asid_to_bin.find(asid);
          if (it != asid_to_bin.end()) {
          }
        } else if (find_kernel_alloc_bprm(va)) {
          // map current asid with binary name
          std::string bin_path = find_launched_binary(proc);

          std::vector<std::string> words;
          split(words, bin_path, '/');

          std::string bin = words.back();
          asid_to_bin.insert({asid, bin});

          run_for(1);
          rewind_trace = this->run_trace();
          pctrace.insert(pctrace.end(), rewind_trace.begin(), rewind_trace.end());
          cur_steps++;
          break;
        } else {
          // do nothing
        }
      } while (true);
    }
    submit_trace_to_threadpool(pctrace);
  }

  loggers.stop();
  auto rc = stop_sim();
  return rc;
}

void Profiler::submit_trace_to_threadpool(trace_t& trace) {
  std::string sfx;
  if (trace_idx < 10) {
    sfx = "000000" + std::to_string(trace_idx);
  } else if (trace_idx < 100) {
    sfx = "00000" + std::to_string(trace_idx);
  } else if (trace_idx < 1000) {
    sfx = "0000" + std::to_string(trace_idx);
  } else if (trace_idx < 10000) {
    sfx = "000" + std::to_string(trace_idx);
  } else if (trace_idx < 100000) {
    sfx = "00" + std::to_string(trace_idx);
  } else if (trace_idx < 1000000) {
    sfx = "0" + std::to_string(trace_idx);
  } else {
    sfx = std::to_string(trace_idx);
  }
  ++trace_idx;
  std::string name = prof_outdir + "/SPIKETRACE-" + sfx;
  loggers.queueJob(printLogs, trace, name);
}

} // namespace Profiler
