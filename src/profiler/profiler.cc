
#include <cstdint>
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
#include <riscv/mmu.h>

#include "profiler.h"
#include "callstack_info.h"
#include "stack_unwinder.h"
#include "types.h"
#include "objdump_parser.h"
#include "perfetto_trace.h"
#include "profiler_state.h"
#include "../lib/string_parser.h"
#include "../spike-top/sim_lib.h"
#include "../spike-top/processor_lib.h"

// TODO : 
// 1. Logging the outputs of the profiler is becoming slow (extra 30 second to boot linux).
//    Need to optimize this.
//
// 3. Better algorithm for checking if a function is called on top of a parent
// function. 
//  - TODO : return CallStackInfo* in function_t
//
// 5. Check robustness of func_args_reg & func_ret_reg of ObjdumpParser
//
// 6. Auto generate the consts section regarding function arguments & offsetof
//
// 7. Separate out the profiling part & the post-processing part
//    -> remove the StackUnwinder from profiler_t & make this process into a
//    separate main??
//    -> Or do we have to???? The post-processing requires all the metadata
//    from the profiler anyways.
//    -> maybe the correct way to do things is to output some logs during the
//    profiling run, and when it is finished, read those logs while using the
//    profiling metadata directly (which is how the stack-unwinder is implemented)


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

profiler_t::profiler_t(
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
      const char* rtl_tracefile_name,
      std::string prof_tracedir,
      FILE *stackfile,
      FILE *prof_logfile)
  : sim_lib_t(cfg, halted, mems, plugin_device_factories, args, dm_config,
          log_path, dtb_enabled, dtb_file, socket_enabled, cmd_file, checkpoint,
          rtl_tracefile_name,
          false /* don't serialize_mem */),
    prof_tracedir(prof_tracedir),
    prof_logfile(prof_logfile)
{
  for (auto& p: objdump_paths) {
    objdumps.insert({p.first, new objdump_parser_t(p.second)});
  }

  this->pstate = new profiler_state_t();
  this->stack_unwinder = new stack_unwinder_t(dwarf_paths, stackfile);

  loggers.start(4);
  packet_loggers.start(1);

  proflog_tp = fopen("./out/PROF-LOGS-THREADPOOL", "w");
  if (proflog_tp == NULL) {
    fprintf(stderr, "Unable to open log file PROF-LOGS-THREADPOOL\n");
    exit(-1);
  }

  function_t* f1 = new kf_do_execveat_common(k_do_execveat_common);
  this->add_kernel_func_to_profile(f1, false);

  function_t* f2 = new kf_set_mm_asid(k_set_mm_asid);
  this->add_kernel_func_to_profile(f2, false);

  function_t* f3 = new kf_kernel_clone(k_kernel_clone);
  this->add_kernel_func_to_profile(f3, true);

  function_t* f4 = new kf_pick_next_task_fair(k_pick_next_task_fair);
  this->add_kernel_func_to_profile(f4, true);

  function_t* f5 = new kf_finish_task_switch(k_finish_task_switch);
  this->add_kernel_func_to_profile(f5, false);
}

profiler_t::~profiler_t() {
  for (auto& o : objdumps) {
    delete o.second;
  }
  objdumps.clear();
  delete this->stack_unwinder;
  delete this->pstate;
}

void profiler_t::add_kernel_func_to_profile(function_t* f, bool rewind_at_exit) {
  std::string fname = f->name();

  auto it = objdumps.find(KERNEL);
  if (it == objdumps.end()) {
    pexit("Could not find kernel objdump\n");
  }

  objdump_parser_t* kdump = it->second;
  addr_t              sva  = kdump->get_func_start_va(fname);
  std::vector<addr_t> evas = kdump->get_func_exits_va(fname);

  if (rewind_at_exit) {
    for (auto e : evas) {
      // Don't need to pop the stack because we are 
      // intercepting the function at its exit.
      pstate->add_prof_func(e, f);
    }
  } else {
    for (auto e : evas) {
      pstate->add_prof_exit(e);
    }
    pstate->add_prof_func(sva, f);
  }
}

objdump_parser_t* profiler_t::get_objdump_parser(std::string oname) {
#ifdef PROFILER_DEBUG
  if (objdumps.find(oname) == objdumps.end()) {
    pexit("Objdump mismatch: %s\n", oname.c_str());
  }
#endif
  return objdumps.find(oname)->second;
}

// Kernel takes up the upper virtual address
bool profiler_t::user_space_addr(addr_t va) {
  addr_t va_hi = va >> 32;
  return ((va_hi & 0xffffffff) == 0);
}

void profiler_t::step_until_insn(std::string type) {
  do {
    run_for(1);
    auto ctrace = this->run_trace();
    insn_t insn = ctrace.back().insn;
    if (disasm.is_type(type, insn))
      break;
  } while (true);
}

int profiler_t::run() {
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
    serialize_proto(protobuf);
    auto ckpt_e = GET_TIME();
    MEASURE_AVG_TIME(ckpt_s, ckpt_e, ckpt_us, ckpt_cnt);

    auto spike_s = GET_TIME();
    this->clear_run_trace();
    this->run_for(INSN_PER_CKPT);
    auto spike_e = GET_TIME();
    MEASURE_AVG_TIME(spike_s, spike_e, spike_us, spike_cnt);

    bool rewind = false;
    size_t fwd_steps = 0;
    trace_t pctrace = this->run_trace();
    int popcnt = 0;

    auto trace_check_s = GET_TIME();
    for (size_t i = 0, cnt = pctrace.size(); i < cnt; i++) {
      reg_t pc = pctrace[i].pc;
      if (pstate->found_registered_func_start_addr(pc).has_value()) {
        rewind = true;
        fwd_steps = i;
        break;
      } else if (pstate->found_registered_func_exit_addr(pc).has_value()) {
#ifdef PROFILER_DEBUG
/* pdebug("Exit PC: 0x%" PRIx64 "\n", pc); */
#endif
        popcnt++;
      }
    }
    auto trace_check_e = GET_TIME();
    MEASURE_AVG_TIME(trace_check_s, trace_check_e, trace_check_us, trace_check_cnt);

    while (popcnt--) {
#ifdef PROFILER_DEBUG
      if (fstacks.find(cur_pid) == fstacks.end()) {
        pexit("Could not find callstack for PID %u\n", cur_pid);
      }
      if (fstacks[cur_pid].size() == 0) {
        pexit("Callstack for PID %u empty, popcnt: %d\n", cur_pid, popcnt);
      }
#endif
      // TODO : What happens when we need to pop on when there is a context switch?
      // Can we guarantee that we can use the cur_pid?
      pstate->pop_callstack(pstate->get_curpid());
    }

    if (rewind) {
      auto rewind_s = GET_TIME();
      auto ld_ckpt_s = GET_TIME();
      deserialize_proto(protobuf);
      auto ld_ckpt_e = GET_TIME();
      MEASURE_AVG_TIME(ld_ckpt_s, ld_ckpt_e, ld_ckpt_us, ld_ckpt_cnt);

      size_t fastfwd_steps = (fwd_steps < INTERLEAVE) ?
                              0 :
                              fwd_steps - INTERLEAVE;
      this->clear_run_trace();
      this->run_for(fastfwd_steps);
      bool found_function = false;
      do {
        processor_t* proc = get_core(0);
        state_t* state = proc->get_state();
        addr_t va = state->pc;

        optreg_t opt_sa = pstate->found_registered_func_start_addr(va);
        if (unlikely(opt_sa.has_value())) {
          auto f = pstate->get_profile_func(opt_sa.value());
          opt_cs_entry_t entry = f->update_profiler(this);
          if (entry.has_value()) {
            pstate->push_callstack(pstate->get_curpid(), entry.value());
          }
          found_function = true;
        }
        run_for(1);
        INCREMENT_CNTR(single_step_cnt);
      } while (!found_function);

      pctrace = this->run_trace();

      auto rewind_e = GET_TIME();
      MEASURE_AVG_TIME(rewind_s, rewind_e, rewind_us, rewind_cnt);
    }
    pstate->incr_retired_insns((reg_t)pctrace.size());
    submit_trace_to_threadpool(pctrace);
    if ((uint32_t) packet_traces.size() >= PACKET_TRACE_FLUSH_THRESHOLD) {
      submit_packet_trace_to_threadpool();
      packet_traces.clear();
    }
  }
  auto run_e = GET_TIME();
  MEASURE_TIME(run_s, run_e, run_us);

  submit_packet_trace_to_threadpool();
  loggers.stop();
  packet_loggers.stop();
  auto rc = stop_sim();

  std::ofstream os("ASID-MAPPING", std::ofstream::out);
  auto asid_to_bin = pstate->asid2bin();
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

std::string profiler_t::spiketrace_filename(uint64_t idx) {
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

void profiler_t::submit_trace_to_threadpool(trace_t& trace) {
  std::string name = prof_tracedir + "/" + spiketrace_filename(trace_idx);
  ++trace_idx;
  loggers.queue_job(print_insn_logs, trace, name);
}

void profiler_t::submit_packet(perfetto::packet_t pkt) {
  packet_traces.push_back(pkt);
}

void profiler_t::submit_packet_trace_to_threadpool() {
  packet_loggers.queue_job(print_event_logs, packet_traces, proflog_tp);
}

void profiler_t::process_callstack() {
  pprintf("Start stack unwinding\n");

  // FIXME : just increment cycle every insn
  uint64_t cycle = 0;

  for (uint64_t i = 0; i < trace_idx; i++) {
    std::string path = prof_tracedir + "/" + spiketrace_filename(i);
    std::ifstream spike_trace = std::ifstream(path, std::ios::binary);
    if (!spike_trace) {
      pexit("%s does not exist\n", path.c_str());
    }

    std::string line;
    std::string::size_type sz = 0;
    std::vector<std::string> words;
    while (std::getline(spike_trace, line)) {
      split(words, line);
      uint64_t addr = std::stoull(words[0], &sz, 16);
      uint64_t asid = std::stoull(words[1], &sz, 10);
/* uint64_t prv  = std::stoull(words[2], &sz, 10); */
/* std::string prev_prv = words[3]; // TODO don't need? */
      words.clear();

      auto asid_to_bin = pstate->asid2bin();
      auto it = asid_to_bin.find(asid);
      if (user_space_addr(addr) && it != asid_to_bin.end()) {
        std::string binpath = it->second;
        std::vector<std::string> subpath;
        split(subpath, binpath, '/');
        stack_unwinder->add_instruction(addr, cycle, subpath.back());
      } else {
        stack_unwinder->add_instruction(addr, cycle, KERNEL);
      }
      cycle++;
    }
  }
}

} // namespace profiler_t
