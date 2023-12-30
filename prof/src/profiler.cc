
#include <cstdlib>
#include <string>
#include <sys/types.h>
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
#include <riscv/sim_lib.h>
#include <riscv/mmu.h>

#include "stack_unwinder.h"
#include "types.h"
#include "objdump_parser.h"
#include "thread_pool.h"
#include "string_parser.h"
#include "profiler.h"

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



// TODO : 
// 1. Better parsing of dwarf_paths & objdump_paths.
// Currently assumes that the user space dwarf & objdump has the same filename.
// It receive be sth like "name,dwarf,objdump".
//
// 2. Perf optimizations
//
// 3. Better algorithm for checking if a function is called on top of a parent
// function. 
//
// 4. API for registering things to monitor rather than manually hand coding them

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
  ObjdumpParser *obj = objdumps.find(KERNEL)->second;
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

  // C strings are null terminated. However C++ std::string isn't.
  name.pop_back();

  addr_t va = state->pc;
  addr_t asid = proc->get_asid();
  pprintf("find_launced_binary done %s va: 0x% " PRIx64 " asid: %" PRIu64 "\n",
          name.c_str(), va, asid);
  return name;
}

addr_t Profiler::kernel_function_start_addr(std::string fname) {
  ObjdumpParser* obj = objdumps.find(KERNEL)->second;
  return obj->get_func_start_va(fname);
}

addr_t Profiler::kernel_function_end_addr(std::string fname) {
  ObjdumpParser* obj = objdumps.find(KERNEL)->second;
  return obj->get_func_end_va(fname);
}

// Kernel takes up the upper virtual address
bool Profiler::user_space_addr(addr_t va) {
  addr_t va_hi = va >> 32;
  return ((va_hi & 0xffffffff) == 0);
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

  double run_us = 0.0;

  uint64_t ckpt_cnt = 0;
  double   ckpt_us  = 0.0;

  uint64_t spike_cnt = 0;
  double   spike_us  = 0.0;

  uint64_t trace_check_cnt = 0;
  double   trace_check_us  = 0.0;

  uint64_t rewind_cnt = 0;
  double   rewind_us  = 0.0;

  uint64_t ld_ckpt_cnt = 0;
  double   ld_ckpt_us  = 0.0;

  uint64_t single_step_cnt = 0;

  addr_t do_execveat_common_start = kernel_function_start_addr(k_do_execveat_common);
  addr_t do_execveat_common_end   = kernel_function_end_addr(k_do_execveat_common);

  addr_t set_mm_asid_start = kernel_function_start_addr(k_set_mm_asid);
  addr_t set_mm_asid_end   = kernel_function_end_addr(k_set_mm_asid);

  auto run_s = GET_TIME();
  while (target_running()) {
    std::string protobuf;

    auto ckpt_s = GET_TIME();
    take_ckpt(protobuf);
    auto ckpt_e = GET_TIME();
    MEASURE_AVG_TIME(ckpt_s, ckpt_e, ckpt_us, ckpt_cnt);

    auto spike_s = GET_TIME();
    run_for(INSN_PER_CKPT);
    auto spike_e = GET_TIME();
    MEASURE_AVG_TIME(spike_s, spike_e, spike_us, spike_cnt);

    bool rewind = false;
    size_t fwd_steps = 0;
    trace_t pctrace = this->run_trace();
    int popcnt = 0;

    auto trace_check_s = GET_TIME();
    for (size_t i = 0, cnt = pctrace.size(); i < cnt; i++) {
      reg_t pc = pctrace[i].pc;
      if (unlikely(
          (pc == do_execveat_common_start) ||
          (pc == set_mm_asid_start))) {
        rewind = true;
        fwd_steps = i;
        pctrace.clear();
        break;
      } else if (unlikely(
          (pc == do_execveat_common_end) ||
          (pc == set_mm_asid_end))) {
        popcnt++;
      }
    }
    auto trace_check_e = GET_TIME();
    MEASURE_AVG_TIME(trace_check_s, trace_check_e, trace_check_us, trace_check_cnt);

    while (popcnt--) {
      fstack.pop_back();
    }

    if (rewind) {
      auto rewind_s = GET_TIME();

      trace_t rewind_trace;
      auto ld_ckpt_s = GET_TIME();
      load_ckpt(protobuf, false);
      auto ld_ckpt_e = GET_TIME();
      MEASURE_AVG_TIME(ld_ckpt_s, ld_ckpt_e, ld_ckpt_us, ld_ckpt_cnt);

      size_t fastfwd_steps = (fwd_steps < INTERLEAVE) ?
                              0 :
                              fwd_steps - INTERLEAVE;
      run_for(fastfwd_steps);
      rewind_trace = this->run_trace();
      pctrace.insert(pctrace.end(), rewind_trace.begin(), rewind_trace.end());
      if (unlikely(fwd_steps < rewind_trace.size())) {
        printf("fwd_steps: %" PRIu64 " < fastfwd_steps: % " PRIu64 "\n",
            fwd_steps, rewind_trace.size());
        exit(1);
      }

      do {
        processor_t* proc = get_core(0);
        state_t* state = proc->get_state();
        addr_t va = state->pc;
        addr_t asid = proc->get_asid();

        if (user_space_addr(va)) {
          // print binary name for the current asid
          auto it = asid_to_bin.find(asid);
          if (it != asid_to_bin.end()) {
          }
        } else if (unlikely(va == do_execveat_common_start)) {
          // map current asid with binary name
          std::string bin_path = find_launched_binary(proc);
          fstack.push_back(CallStackInfo(k_do_execveat_common, bin_path));
          run_for(1);
          rewind_trace = this->run_trace();
          pctrace.insert(pctrace.end(), rewind_trace.begin(), rewind_trace.end());

          run_for(1);
          rewind_trace = this->run_trace();
          pctrace.insert(pctrace.end(), rewind_trace.begin(), rewind_trace.end());
          INCREMENT_CNTR(single_step_cnt);

          break;
        } else if (unlikely(va == set_mm_asid_start)) {
          if (called_by_do_execveat_common()) {
            step_until_insn(CSRW, pctrace);
            asid = proc->get_asid();
            std::string bin = callstack_top().bin();
            asid_to_bin.insert({asid, bin});
            pprintf("Found mapping ASID: %" PRIu64 " bin: %s\n", asid, bin.c_str());
          }

          fstack.push_back(CallStackInfo(k_set_mm_asid, ""));

          run_for(1);
          rewind_trace = this->run_trace();
          pctrace.insert(pctrace.end(), rewind_trace.begin(), rewind_trace.end());
          INCREMENT_CNTR(single_step_cnt);

          break;
        } else {
          // do nothing
        }
        run_for(1);
        rewind_trace = this->run_trace();
        pctrace.insert(pctrace.end(), rewind_trace.begin(), rewind_trace.end());
        INCREMENT_CNTR(single_step_cnt);

      } while (true);

      auto rewind_e = GET_TIME();
      MEASURE_AVG_TIME(rewind_s, rewind_e, rewind_us, rewind_cnt);
    }
    submit_trace_to_threadpool(pctrace);
  }
  auto run_e = GET_TIME();
  MEASURE_TIME(run_s, run_e, run_us);


  loggers.stop();
  auto rc = stop_sim();

  std::ofstream os("ASID-MAPPING", std::ofstream::out);
  for (auto x : asid_to_bin) {
    os << x.first << " " << x.second << "\n";
  }
  os.close();

  PRINT_TIME_STAT("RUN TOOK", run_us);
  PRINT_AVG_TIME_STAT("CKPT", ckpt_us, ckpt_cnt);
  PRINT_AVG_TIME_STAT("SPIKE", spike_us, spike_cnt);
  PRINT_AVG_TIME_STAT("TRACE_CHECK", trace_check_us, trace_check_cnt);
  PRINT_AVG_TIME_STAT("REWIND", rewind_us, rewind_cnt);
  PRINT_AVG_TIME_STAT("LDCKPT", ld_ckpt_us, ld_ckpt_cnt);
  PRINT_CNTR_STAT("SINGLE_STEP", single_step_cnt);

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
    std::string::size_type sz = 0;
    std::vector<std::string> words;
    while (std::getline(spike_trace, line)) {
      split(words, line);
      uint64_t addr = std::stoull(words[0], &sz, 16);
      uint64_t asid = std::stoull(words[1], &sz, 10);
      uint64_t prv  = std::stoull(words[2], &sz, 10);
/* std::string prev_prv = words[3]; // TODO don't need? */
      words.clear();

      auto it = asid_to_bin.find(asid);
      if (user_space_addr(addr) && it != asid_to_bin.end()) {
        std::string binpath = it->second;
        std::vector<std::string> subpath;
        split(subpath, binpath, '/');
        stack_unwinder->addInstruction(addr, cycle, subpath.back());
      } else {
        stack_unwinder->addInstruction(addr, cycle, KERNEL);
      }
      cycle++;
    }
  }
}

} // namespace Profiler
