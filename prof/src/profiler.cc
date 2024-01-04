
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

#include "callstack_info.h"
#include "stack_unwinder.h"
#include "types.h"
#include "objdump_parser.h"
#include "thread_pool.h"
#include "string_parser.h"
#include "profiler.h"

// TODO : 
// 3. Better algorithm for checking if a function is called on top of a parent
// function. 
//  - TODO : return CallStackInfo* in Function
//
// 5. Check robustness of func_args_reg & func_ret_reg of ObjdumpParser
// 6. Auto generate the consts section regarding function arguments & offsetof

/* #define PROFILER_ASSERT */

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

  Function* f1 = new KF_do_execveat_common(k_do_execveat_common);
  this->add_kernel_func_to_profile(f1, false);

  Function* f2 = new KF_set_mm_asid(k_set_mm_asid);
  this->add_kernel_func_to_profile(f2, false);

  Function* f3 = new KF_pick_next_task_fair(k_pick_next_task_fair);
  this->add_kernel_func_to_profile(f3, true);

  Function* f4 = new KF_kernel_clone(k_kernel_clone);
  this->add_kernel_func_to_profile(f4, true);
}

Profiler::~Profiler() {
  for (auto& o : objdumps) {
    delete o.second;
  }
  objdumps.clear();
}

void Profiler::add_kernel_func_to_profile(Function* f, bool rewind_at_exit) {
  std::string fname = f->name();

  auto it = objdumps.find(KERNEL);
  if (it == objdumps.end()) {
    pprintf("Could not find kernel objdump\n");
    exit(1);
  }

  ObjdumpParser* kdump = it->second;
  addr_t              sva  = kdump->get_func_start_va(fname);
  std::vector<addr_t> evas = kdump->get_func_exits_va(fname);

  if (rewind_at_exit) {
    for (auto e : evas) {
      // Don't need to pop the stack because we are 
      // intercepting the function at its exit.
      func_pc_prof_start.push_back(e);
      prof_pc_to_func[e] = f;
    }
  } else {
    for (auto e : evas) {
      func_pc_prof_exit .push_back(e);
    }
    prof_pc_to_func[sva] = f;
    func_pc_prof_start.push_back(sva);
  }
}

ObjdumpParser* Profiler::get_objdump_parser(std::string oname) {
#ifdef PROFILER_ASSERT
  if (objdumps.find(oname) == objdumps.end()) {
    fprintf(stderr, "Objdump mismatch: %s\n", oname.c_str());
    exit(1);
  }
#endif
  return objdumps.find(oname)->second;
}


std::vector<CallStackInfo>& Profiler::get_callstack() {
  return fstack;
}

std::map<reg_t, std::string>& Profiler::get_asid2bin_map() {
  return asid_to_bin;
}

std::map<pid_t, std::string>& Profiler::get_pid2bin_map() {
  return pid_to_bin;
}

addr_t Profiler::kernel_function_start_addr(std::string fname) {
  ObjdumpParser* obj = objdumps.find(KERNEL)->second;
  return obj->get_func_start_va(fname);
}

addr_t Profiler::kernel_function_end_addr(std::string fname) {
  ObjdumpParser* obj = objdumps.find(KERNEL)->second;
  return obj->get_func_end_va(fname);
}

bool Profiler::found_registered_func_start_addr(addr_t va) {
  for (auto &x : func_pc_prof_start) {
    if (unlikely(x == va)) return true;
  }
  return false;
}

bool Profiler::found_registered_func_end_addr(addr_t va) {
  for (auto &x : func_pc_prof_exit) {
    if (unlikely(x == va)) return true;
  }
  return false;
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
      if (found_registered_func_start_addr(pc)) {
        rewind = true;
        fwd_steps = i;
        pctrace.clear();
        break;
      } else if (found_registered_func_end_addr(pc)) {
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

#ifdef PROFILER_ASSERT
      if (unlikely(fwd_steps < rewind_trace.size())) {
        printf("fwd_steps: %" PRIu64 " < fastfwd_steps: % " PRIu64 "\n",
            fwd_steps, rewind_trace.size());
        exit(1);
      }
#endif PROFILER_ASSERT

      bool found_function = false;
      do {
        processor_t* proc = get_core(0);
        state_t* state = proc->get_state();
        addr_t va = state->pc;
        addr_t asid = proc->get_asid();

        for (auto& sa : func_pc_prof_start) {
          if (unlikely(sa == va)) {
            auto f = prof_pc_to_func[va];
            OptCallStackInfo entry = f->update_profiler(this, pctrace);
            if (entry.has_value())
              fstack.push_back(entry.value());

            found_function = true;
            break;
          }
        }

        run_for(1);
        rewind_trace = this->run_trace();
        pctrace.insert(pctrace.end(), rewind_trace.begin(), rewind_trace.end());
        INCREMENT_CNTR(single_step_cnt);

      } while (!found_function);

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
