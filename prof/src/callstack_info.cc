#include <string>
#include <optional>
#include <map>
#include <vector>

#include <riscv/processor.h>
#include <riscv/mmu.h>

#include "callstack_info.h"
#include "profiler.h"
#include "types.h"
#include "perfetto_trace.h"

namespace Profiler {

#define PROFILER_DEBUG

CallStackInfo::CallStackInfo(std::string func, std::string binary)
  : func(func), binary(binary)
{
}

std::string CallStackInfo::fn() {
  return func;
}

std::string CallStackInfo::bin() {
  return binary;
}


Function::Function(std::string name)
  : n(name)
{
}


KernelFunction::KernelFunction(std::string name)
  : Function(name)
{
}

addr_t KernelFunction::get_current_ptr(processor_t* proc) {
  state_t* s = proc->get_state();
  unsigned int tidx = riscv_abi_ireg["tp"];
  return s->XPR[tidx];
}

pid_t KernelFunction::get_current_pid(processor_t* proc) {
  mmu_t* mmu = proc->get_mmu();

  addr_t curr_ptr = get_current_ptr(proc);
  addr_t pid_addr = curr_ptr + offsetof_task_struct_pid;
  pid_t  pid = mmu->load<pid_t>(pid_addr);
  return pid;
}

KF_do_execveat_common::KF_do_execveat_common(std::string name) 
  : KernelFunction(name)
{
}

OptCallStackInfo KF_do_execveat_common::update_profiler(Profiler *p, trace_t& t) {
  // TODO : multicore support
  processor_t* proc = p->get_core(0);

  // map current asid with binary name
  std::string filepath = find_exec_syscall_filepath(p, proc);
  return CallStackInfo(k_do_execveat_common, filepath);
}

std::string KF_do_execveat_common::find_exec_syscall_filepath(
    Profiler* p,
    processor_t* proc)
{
  ObjdumpParser *obj = p->get_objdump_parser(KERNEL);
  std::string farg_abi_reg = obj->func_args_reg(k_do_execveat_common,
                                                k_do_execveat_common_filename_arg);
  unsigned int reg_idx = riscv_abi_ireg[farg_abi_reg];

  mmu_t*   mmu   = proc->get_mmu();
  state_t* state = proc->get_state();
  addr_t filename_ptr    = state->XPR[reg_idx];
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

  // update the pid mapping
  std::map<pid_t, std::string>& pid2bin = p->get_pid2bin_map();
  pid_t pid = get_current_pid(proc);
  auto it = pid2bin.find(pid);

  if (it != pid2bin.end()) {
    pprintf("Updating pid2bin[%u]: %s -> %s\n",
      pid, it->second.c_str(), name.c_str());

    p->submit_packet(Perfetto::TracePacket(
          std::string(k_do_execveat_common),
          Perfetto::TYPE_INSTANT,
          p->get_insn_retired()));
  }
  pid2bin[pid] = name;

#ifdef PROFILER_DEBUG
  if (pid != p->get_curpid()) {
    pexit("%d Prof internal pid: %u, spike pid: %u\n", __LINE__, p->get_curpid(), pid);
  }
#endif

  return name;
}

KF_set_mm_asid::KF_set_mm_asid(std::string name)
  : KernelFunction(name)
{
}

OptCallStackInfo KF_set_mm_asid::update_profiler(Profiler *p, trace_t& t) {
  processor_t* proc = p->get_core(0);
  pid_t pid = get_current_pid(proc);
  std::vector<CallStackInfo>& cs = p->get_callstack(pid);

  if (called_by_do_execveat_common(cs)) {
    p->step_until_insn(CSRW, t);

    CallStackInfo top = cs.back();
    std::string bin = top.bin();
    reg_t asid = proc->get_asid();

    p->get_asid2bin_map().insert({asid, bin});

    pprintf("Found mapping ASID: %" PRIu64 " PID: %u bin: %s\n",
        asid, pid, bin.c_str());

    p->submit_packet(Perfetto::TracePacket(
          std::string(k_set_mm_asid),
          Perfetto::TYPE_INSTANT,
          p->get_insn_retired()));

    if (pid != p->get_curpid()) {
      pexit("%d Prof internal pid: %u, spike pid: %u\n", __LINE__, p->get_curpid(), pid);
    }
  }
  return CallStackInfo(k_set_mm_asid, "");
}

bool KF_set_mm_asid::called_by_do_execveat_common(std::vector<CallStackInfo>& cs) {
  if (cs.size() == 0) {
    return false;
  } else {
    auto back = cs.back();
    return (back.fn() == k_do_execveat_common);
  }
}

KF_kernel_clone::KF_kernel_clone(std::string name)
  : KernelFunction(name)
{
}

OptCallStackInfo KF_kernel_clone::update_profiler(Profiler *p, trace_t& t) {
  processor_t* proc = p->get_core(0);
  pid_t newpid = get_forked_task_pid(p, proc);
  pid_t parpid = get_current_pid(proc);


  std::map<pid_t, std::string>& pid2bin = p->get_pid2bin_map();
  auto it = pid2bin.find(parpid);
  if (it != pid2bin.end()) {
    std::string bin = it->second;
    pid2bin[newpid] = bin;
  } else {
    pid2bin[newpid] = "X";
  }

  pprintf("Forked p: %u c: %u bin: %s\n", parpid, newpid, pid2bin[newpid].c_str());

  p->submit_packet(Perfetto::TracePacket(
        std::string(k_kernel_clone),
        Perfetto::TYPE_INSTANT,
        p->get_insn_retired()));

  return {};
}

pid_t KF_kernel_clone::get_forked_task_pid(Profiler* p, processor_t* proc) {
  ObjdumpParser *obj = p->get_objdump_parser(KERNEL);
  std::string ret_reg = obj->func_ret_reg(k_kernel_clone);
  unsigned int reg_idx = riscv_abi_ireg[ret_reg];

  mmu_t* mmu = proc->get_mmu();
  state_t* state = proc->get_state();
  pid_t next_pid = state->XPR[reg_idx];
  return next_pid;
}

KF_pick_next_task_fair::KF_pick_next_task_fair(std::string name)
  : KernelFunction(name)
{
}

OptCallStackInfo KF_pick_next_task_fair::update_profiler(Profiler *p, trace_t& t) {
  processor_t* proc = p->get_core(0);
  std::optional<pid_t> pid = get_pid_next_task(p, proc);
  return {};
}

std::optional<pid_t> KF_pick_next_task_fair::get_pid_next_task(Profiler *p, processor_t* proc) {
  ObjdumpParser *obj = p->get_objdump_parser(KERNEL);
  std::string ret_reg = obj->func_ret_reg(k_pick_next_task_fair);
  unsigned int reg_idx = riscv_abi_ireg[ret_reg];

  mmu_t* mmu = proc->get_mmu();
  state_t* state = proc->get_state();
  addr_t next_task_ptr = state->XPR[reg_idx];
  if (next_task_ptr == 0) {
    pprintf("CFS doesn't have a task to schedule\n");
    return {};
  } else {
    addr_t next_task_pid_addr = next_task_ptr + offsetof_task_struct_pid;
    pid_t pid = mmu->load<pid_t>(next_task_pid_addr);

    auto pid2bin = p->get_pid2bin_map();
/* pprintf("CFS next task pid: %" PRIu32 " %s\n", pid, pid2bin[pid].c_str()); */
    return std::optional<pid_t>(pid);
  }
}

KF_finish_task_switch::KF_finish_task_switch(std::string name)
  : KernelFunction(name)
{
}

OptCallStackInfo KF_finish_task_switch::update_profiler(Profiler *p, trace_t& t) {
  processor_t* proc = p->get_core(0);
  pid_t cur_pid  = get_current_pid(proc);
  pid_t prev_pid = get_prev_pid(p, proc);
  p->set_curpid(cur_pid);

/* pprintf("ContextSwitch Finished %u -> %u\n", prev_pid, cur_pid); */

  auto pid2bin = p->get_pid2bin_map();

  std::string prev_bin = pid2bin[prev_pid];
  p->submit_packet(Perfetto::TracePacket(
        prev_bin,
        Perfetto::TYPE_SLICE_END,
        p->get_insn_retired()));

  std::string cur_bin = pid2bin[cur_pid];
  p->submit_packet(Perfetto::TracePacket(
        cur_bin,
        Perfetto::TYPE_SLICE_BEGIN,
        p->get_insn_retired()));

  return CallStackInfo(k_finish_task_switch, "");
}

pid_t KF_finish_task_switch::get_prev_pid(Profiler *p, processor_t* proc) {
  ObjdumpParser *obj = p->get_objdump_parser(KERNEL);
  std::string reg_name = obj->func_args_reg(k_finish_task_switch,
                                            k_finish_task_switch_prev_arg);
  unsigned int reg_idx = riscv_abi_ireg[reg_name];

  mmu_t* mmu = proc->get_mmu();
  state_t* state = proc->get_state();
  addr_t prev_task_ptr = state->XPR[reg_idx];
  if (prev_task_ptr == 0) {
    pexit("prev is null in %s\n", k_finish_task_switch);
  } else {
    addr_t prev_task_pid_addr = prev_task_ptr + offsetof_task_struct_pid;

    pid_t prev_pid = mmu->load<pid_t>(prev_task_pid_addr);
    return prev_pid;
  }
}

} // namespace Profiler
