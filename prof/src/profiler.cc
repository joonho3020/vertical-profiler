
#include <cstdlib>
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

#include "stack_unwinder.h"
#include "types.h"
#include "objdump_parser.h"
#include "thread_pool.h"
#include "string_parser.h"
#include "profiler.h"


// TODO : 
// 1. Better parsing of dwarf_paths & objdump_paths.
// Currently assumes that the user space dwarf & objdump has the same filename.
// It receive be sth like "name,dwarf,objdump".
//
// 2. Check stack unwinding correctness
//
// 3. Better algorithm for checking if a function is called on top of a parent
// function.

namespace Profiler {

Profiler::Profiler(
      std::vector<std::pair<std::string, std::string>> objdump_paths,
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
      const char *prof_outdir,
      FILE *stackfile)
  : sim_lib_t(cfg, halted, mems, plugin_device_factories, args, dm_config,
          log_path, dtb_enabled, dtb_file, socket_enabled, cmd_file, checkpoint),
    prof_outdir(prof_outdir)
{
  for (auto& p: objdump_paths) {
    objdumps.insert({p.first, new ObjdumpParser(p.second)});
  }

  stack_unwinder = new StackUnwinder(dwarf_paths, stackfile);

  loggers.start();

  std::string iregs[XPR_CNT] = {
    "x0", "ra", "sp", "gp",  "tp",  "t0", "t1", "t2",
    "s0", "s1", "a0",  "a1",  "a2", "a3", "a4", "a5",
    "a6", "a7", "s2",  "s3",  "s4", "s5", "s6", "s7",
    "s8", "s9", "s10", "s11", "t3", "t4", "t5", "t6"
  };

  for (int i = 0; i < XPR_CNT; i++) {
    riscv_abi.insert({iregs[i], i});
  }
}

Profiler::~Profiler() {
  for (auto& o : objdumps) {
    delete o.second;
  }
  objdumps.clear();
}

std::string Profiler::find_launched_binary(processor_t* proc) {
  ObjdumpParser *obj = objdumps.find("kernel")->second;
  std::string farg_abi_reg = obj->func_args_reg(k_do_execveat_common,
                                                k_do_execveat_common_filename_arg);

  if (riscv_abi.find(farg_abi_reg) == riscv_abi.end()) {
    fprintf(stderr, "ABI mismatch: %s, PC: 0x%" PRIx64 "\n",
        farg_abi_reg, proc->get_state()->pc);
    exit(1);
  }

  unsigned int reg_idx = riscv_abi[farg_abi_reg];
  state_t* state = proc->get_state();
  mmu_t* mmu = proc->get_mmu();

  addr_t filename_ptr = state->XPR[reg_idx];
  addr_t filename_struct = mmu->load<uint64_t>(filename_ptr);

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

bool Profiler::find_kernel_function(addr_t inst_va, std::string fname) {
  ObjdumpParser* obj = objdumps.find("kernel")->second;
  return (obj->get_func_start_va(fname) == inst_va);
}

bool Profiler::find_kernel_do_execveat_common(addr_t inst_va) {
  return find_kernel_function(inst_va, k_do_execveat_common);
}

bool Profiler::find_kernel_set_mm_asid(addr_t inst_va) {
  return find_kernel_function(inst_va, k_set_mm_asid);
}

bool Profiler::find_kernel_function_end(addr_t inst_va, std::string fname) {
  ObjdumpParser* obj = objdumps.find("kernel")->second;
  return (obj->get_func_end_va(fname) == inst_va);
}

// Kernel takes up the upper virtual address
bool Profiler::user_space_addr(addr_t va) {
  addr_t va_hi = va >> 32;
  return ((va_hi & 0xffffffff) == 0);
}

void Profiler::step_until_kernel_function(std::string fname, trace_t& trace) {
  do {
    run_for(1);
    auto ctrace = this->run_trace();
    trace.insert(trace.end(), ctrace.begin(), ctrace.end());

    addr_t pc = get_va_pc(0);
    if (find_kernel_function(pc, fname))
      break;
  } while (true);
}

void Profiler::step_until_insn(std::string type, trace_t& trace) {
  do {
    run_for(1);
    auto ctrace = this->run_trace();
    trace.insert(trace.end(), ctrace.begin(), ctrace.end());

    insn_t insn = ctrace.back().insn;
    if (disasm.is_type(type, insn))
      break;
  } while (true);
}

state_t* Profiler::get_state(int hartid) {
  processor_t* p = get_core(hartid);
  return p->get_state();
}

addr_t Profiler::get_va_pc(int hartid) {
  state_t* state = get_state(hartid);
  return state->pc;
}

bool Profiler::called_by_do_execveat_common() {
  if (fstack.size() == 0) return false;
  auto back = fstack.back();
  return (back.fn() == k_do_execveat_common);
}

CallStackInfo Profiler::callstack_top() {
  return fstack.back();
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
    int popcnt = 0;
    for (size_t i = 0, cnt = pctrace.size(); i < cnt; i++) {
      if (find_kernel_do_execveat_common(pctrace[i].pc) ||
          find_kernel_set_mm_asid(pctrace[i].pc)) {
        rewind = true;
        fwd_steps = i;
        pctrace.clear();
        break;
      } else {
        if (find_kernel_function_end(pctrace[i].pc, k_do_execveat_common) ||
            find_kernel_function_end(pctrace[i].pc, k_set_mm_asid)) {
          popcnt++;
        }
      }
    }

    while (popcnt--) {
      fstack.pop_back();
    }

    if (rewind) {
      trace_t rewind_trace;
      load_ckpt(protobuf, false);

      size_t fastfwd_steps = (fwd_steps < INTERLEAVE) ?
                              fwd_steps :
                              fwd_steps - INTERLEAVE;
      run_for(fastfwd_steps);
      rewind_trace = this->run_trace();
      pctrace.insert(pctrace.end(), rewind_trace.begin(), rewind_trace.end());

      do {
        run_for(1);
        rewind_trace = this->run_trace();
        pctrace.insert(pctrace.end(), rewind_trace.begin(), rewind_trace.end());

        processor_t* proc = get_core(0);
        state_t* state = proc->get_state();
        addr_t va = state->pc;
        addr_t asid = proc->get_asid();

        if (user_space_addr(va)) {
          // print binary name for the current asid
          auto it = asid_to_bin.find(asid);
          if (it != asid_to_bin.end()) {
          }
        } else if (find_kernel_do_execveat_common(va)) {
          // map current asid with binary name
          std::string bin_path = find_launched_binary(proc);
          fstack.push_back(CallStackInfo(k_do_execveat_common, bin_path));
          run_for(1);
          rewind_trace = this->run_trace();
          pctrace.insert(pctrace.end(), rewind_trace.begin(), rewind_trace.end());
          break;
        } else if (find_kernel_set_mm_asid(va)) {
          if (called_by_do_execveat_common()) {
            step_until_insn(CSRW, pctrace);
            asid = proc->get_asid();
            std::string bin = callstack_top().bin();
            asid_to_bin.insert({asid, bin});
            pprintf("Found mapping ASID: %" PRIu64 " bin: %s\n", asid, bin.c_str());
          }

          fstack.push_back(CallStackInfo(k_set_mm_asid, ""));
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

std::string Profiler::spiketrace_filename(uint64_t idx) {
  std::string sfx;
  if (idx < 10) {
    sfx = "000000" + std::to_string(idx);
  } else if (idx < 100) {
    sfx = "00000" + std::to_string(idx);
  } else if (idx < 1000) {
    sfx = "0000" + std::to_string(idx);
  } else if (idx < 10000) {
    sfx = "000" + std::to_string(idx);
  } else if (idx < 100000) {
    sfx = "00" + std::to_string(idx);
  } else if (idx < 1000000) {
    sfx = "0" + std::to_string(idx);
  } else {
    sfx = std::to_string(idx);
  }
  return ("SPIKETRACE-" + sfx);
}

void Profiler::submit_trace_to_threadpool(trace_t& trace) {
  std::string name = prof_outdir + "/" + spiketrace_filename(trace_idx);
  ++trace_idx;
  loggers.queueJob(printLogs, trace, name);
}


void Profiler::process_callstack() {
  pprintf("Start stack unwinding\n");

  // FIXME : just increment cycle every insn
  uint64_t cycle = 0;

  for (uint64_t i = 0; i < trace_idx; i++) {
    std::string path = prof_outdir + "/" + spiketrace_filename(i);
    std::ifstream spike_trace = std::ifstream(path, std::ios::binary);
    if (!spike_trace) {
      std::cerr << path << " does not exist" << std::endl;
      exit(1);
    }

    std::string line;
    std::vector<std::string> words;
    while (std::getline(spike_trace, line)) {
      split(words, line);
      uint64_t addr = std::stoull(words[0], 0, 0);
      uint64_t asid = std::stoull(words[1], 0, 0);
      uint64_t prv  = std::stoull(words[2], 0, 0);
      words.clear();
/* std::string prev_prv = words[3]; // TODO don't need? */

      if (user_space_addr(addr)) {
        std::string binpath = asid_to_bin[asid];
        std::vector<std::string> subpath;
        split(subpath, binpath, '/');
        stack_unwinder->addInstruction(addr, cycle, subpath.back());
      } else {
        stack_unwinder->addInstruction(addr, cycle, "kernel");
      }
      cycle++;
    }
  }
}

} // namespace Profiler
