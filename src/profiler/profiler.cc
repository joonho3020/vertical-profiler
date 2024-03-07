
#include <cstdint>
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
      const char* rtl_tracefile_name,
      std::string prof_tracedir,
      FILE *stackfile)
  : sim_lib_t(cfg, halted, mems, plugin_device_factories, args, dm_config,
          log_path, dtb_enabled, dtb_file, socket_enabled, cmd_file,
          rtl_tracefile_name,
          false /* don't serialize_mem */)
{
  for (auto p: objdump_paths) {
    objdumps_.insert({p.first, new objdump_parser_t(p.second)});
  }

  this->logger_ = new logger_t(prof_tracedir);
  this->pstate_ = new profiler_state_t();
  this->stack_unwinder = new stack_unwinder_t(dwarf_paths, stackfile);

  auto it = objdumps_.find(profiler::KERNEL);
  if (it == objdumps_.end()) {
    pprintf("%s\n", it->first.c_str());
    passert("Could not find kernel objdump\n");
  }

  objdump_parser_t* kdump = it->second;

  function_t* f1 = new kf_do_execveat_common(k_do_execveat_common);
  this->profile_kernel_func_at_pc(f1, 
      kdump->get_func_start_va(f1->name()),
      kdump->get_func_exits_va(f1->name()));

  function_t* f2 = new kf_set_mm_asid(k_set_mm_asid);
  this->profile_kernel_func_at_pc(f2,
      kdump->get_func_csrw_va(f2->name(), profiler::PROF_CSR_SATP),
      kdump->get_func_exits_va(f2->name()));

  function_t* f3 = new kf_kernel_clone(k_kernel_clone);
  this->profile_kernel_func_at_exit(f3,
      kdump->get_func_exits_va(f3->name()));

  function_t* f4 = new kf_pick_next_task_fair(k_pick_next_task_fair);
  this->profile_kernel_func_at_exit(f4,
      kdump->get_func_exits_va(f4->name()));

  function_t* f5 = new kf_finish_task_switch(k_finish_task_switch);
  this->profile_kernel_func_at_pc(f5,
      kdump->get_func_start_va(f5->name()),
      kdump->get_func_exits_va(f5->name()));
}

profiler_t::~profiler_t() {
  for (auto& o : objdumps_) {
    delete o.second;
  }
  objdumps_.clear();

  delete this->stack_unwinder;
  delete this->pstate_;
  delete this->logger_;
}

void profiler_t::profile_kernel_func_at_exit(
    function_t* f, std::vector<addr_t> evas) {
  for (auto e : evas) {
    // Don't need to pop the stack because we are 
    // intercepting the function at its exit.
    pstate_->add_prof_func(e, f);
  }
}

void profiler_t::profile_kernel_func_at_pc(
    function_t* f, addr_t pc, std::vector<addr_t> evas) {
  for (auto e : evas) {
    pstate_->add_prof_exit(e);
  }
  pstate_->add_prof_func(pc, f);
}

objdump_parser_t* profiler_t::get_objdump_parser(std::string oname) {
#ifdef PROFILER_DEBUG
  if (objdumps_.find(oname) == objdumps_.end()) {
    passert("Objdump mismatch: %s\n", oname.c_str());
  }
#endif
  return objdumps_.find(oname)->second;
}

reg_t profiler_t::get_pc(int hartid) {
  processor_t* proc = get_core(hartid);
  state_t* state = proc->get_state();
  return state->pc;
}

// Kernel takes up the upper virtual address
bool profiler_t::user_space_addr(addr_t va) {
  addr_t va_hi = va >> 32;
  return ((va_hi & 0xffffffff) == 0);
}

profiler_state_t* profiler_t::pstate() {
  return pstate_;
}

logger_t* profiler_t::logger() {
  return logger_;
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
      if (pstate_->found_registered_func_start_addr(pc).has_value()) {
#ifdef PROFILER_DEBUG
        pprintf("Rewind PC: 0x%" PRIx64 "\n", pc);
#endif
        rewind = true;
        fwd_steps = i;
        break;
      } else if (pstate_->found_registered_func_exit_addr(pc).has_value()) {
        popcnt++;
      }
    }
    auto trace_check_e = GET_TIME();
    MEASURE_AVG_TIME(trace_check_s, trace_check_e, trace_check_us, trace_check_cnt);

    while (popcnt--) {
      // TODO : What happens when we need to pop on when there is a context switch?
      // Can we guarantee that we can use the cur_pid?
      pstate_->pop_callstack(pstate_->get_curpid());
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
        addr_t va = this->get_pc(0);
        optreg_t opt_sa = pstate_->found_registered_func_start_addr(va);

        if (unlikely(opt_sa.has_value())) {
          auto f = pstate_->get_profile_func(opt_sa.value());

          // TODO : This logic of returning a stack entry for certain
          // functions is not that pretty.
          opt_cs_entry_t entry = f->update_profiler(this);
          if (entry.has_value()) {
            pstate_->push_callstack(pstate_->get_curpid(), entry.value());
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
    pstate_->update_timestamp(pstate_->get_timestamp() + (reg_t)pctrace.size());
    logger_->submit_trace_to_threadpool(pctrace);
    logger_->submit_packet_trace_to_threadpool();
  }
  auto run_e = GET_TIME();
  MEASURE_TIME(run_s, run_e, run_us);

  logger_->flush_packet_trace_to_threadpool();
  logger_->stop();
  auto rc = stop_sim();

  std::ofstream os("ASID-MAPPING", std::ofstream::out);
  auto asid_to_bin = pstate_->asid2bin();
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

int profiler_t::run_from_trace() {
  init();

  assert(rtl_tracefile_name);
  std::string line;
  std::string tracefile_string = std::string(rtl_tracefile_name);
  std::ifstream rtl_trace = std::ifstream(tracefile_string, std::ios::binary);

  // TODO : multicore support
  int hartid = 0;
  this->configure_log(true, true);
  this->get_core(hartid)->get_state()->pc = ROCKETCHIP_RESET_VECTOR;

  uint64_t loop_cntr = 0;
  while (std::getline(rtl_trace, line)) {
    uint64_t tohost_req = check_tohost_req();
    if (tohost_req)
      handle_tohost_req(tohost_req);

    rtl_step_t step = parse_line_into_rtltrace(line);
    bool success = ganged_step(step, hartid);
    if (!success) {
      passert("ganged simulation failed!\n");
    }

    pstate_->update_timestamp(step.time);

    addr_t pc = this->get_pc(hartid);
    optreg_t opt_sa = pstate_->found_registered_func_start_addr(pc);
    if (opt_sa.has_value()) {
      auto f = pstate_->get_profile_func(opt_sa.value());
      opt_cs_entry_t entry = f->update_profiler(this);
      if (entry.has_value()) {
        pstate_->push_callstack(pstate_->get_curpid(), entry.value());
      }
    } else if (pstate_->found_registered_func_exit_addr(pc).has_value()) {
      pstate_->pop_callstack(pstate_->get_curpid());
    }

    loop_cntr++;
    if (loop_cntr % SPIKE_LOG_FLUSH_PERIOD == 0) {
      trace_t pctrace = this->run_trace();
      this->clear_run_trace();

      logger_->submit_trace_to_threadpool(pctrace);
      logger_->submit_packet_trace_to_threadpool();
    }
  }
  logger_->flush_packet_trace_to_threadpool();
  logger_->stop();
  auto rc = stop_sim();
  return rc;
}

void profiler_t::process_callstack() {
  pprintf("Start stack unwinding\n");

  // FIXME : just increment cycle every insn
  uint64_t cycle = 0;
  uint64_t trace_cnt = logger_->get_trace_idx();

  for (uint64_t i = 0; i < trace_cnt; i++) {
    std::string path = logger_->get_pctracedir() + "/" + logger_->spiketrace_filename(i);
    std::ifstream spike_trace = std::ifstream(path, std::ios::binary);
    if (!spike_trace) {
      passert("%s does not exist\n", path.c_str());
    }

    std::string line;
    std::string::size_type sz = 0;
    std::vector<std::string> words;
    while (std::getline(spike_trace, line)) {
      split(words, line);
      uint64_t addr = std::stoull(words[0], &sz, 16);
      uint64_t asid = std::stoull(words[1], &sz, 10);
      words.clear();

      auto asid_to_bin = pstate_->asid2bin();
      auto it = asid_to_bin.find(asid);
      if (user_space_addr(addr) && it != asid_to_bin.end()) {
        std::string binpath = it->second;
        std::vector<std::string> subpath;
        split(subpath, binpath, '/');
        stack_unwinder->add_instruction(addr, cycle, subpath.back());
      } else {
        stack_unwinder->add_instruction(addr, cycle, profiler::KERNEL);
      }
      cycle++;
    }
  }
}

} // namespace profiler_t
