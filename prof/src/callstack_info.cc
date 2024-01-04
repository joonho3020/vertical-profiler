#include <string>
#include <optional>

#include <riscv/processor.h>
#include <riscv/mmu.h>

#include "callstack_info.h"
#include "profiler.h"
#include "types.h"

namespace Profiler {

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

Function_k_do_execveat_common::Function_k_do_execveat_common(std::string name) 
  : Function(name)
{
}

std::optional<CallStackInfo> Function_k_do_execveat_common::update_profiler(Profiler *p, trace_t& t) {
/* pprintf("Called kernel_do_execveat_common_start update_profiler\n"); */

  // TODO : multicore support
  processor_t* proc = p->get_core(0);

  // map current asid with binary name
  std::string filepath = find_exec_syscall_filepath(p, proc);
  return CallStackInfo(k_do_execveat_common, filepath);
}

std::string Function_k_do_execveat_common::find_exec_syscall_filepath(
    Profiler* p,
    processor_t* proc)
{
  ObjdumpParser *obj = p->get_objdump_parser(KERNEL);
  std::string farg_abi_reg = obj->func_args_reg(k_do_execveat_common,
                                                k_do_execveat_common_filename_arg);
  unsigned int reg_idx = p->get_riscv_abi_ireg(farg_abi_reg);

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
  pprintf("find_launced_binary done %s va: 0x% " PRIx64 " asid: %" PRIu64 "\n",
          name.c_str(), va, asid);
  return name;
}


Function_k_set_mm_asid::Function_k_set_mm_asid(std::string name)
  : Function(name)
{
}

std::optional<CallStackInfo> Function_k_set_mm_asid::update_profiler(Profiler *p, trace_t& t) {
/* pprintf("Called kernel_set_mm_asid update_profiler\n"); */

  std::vector<CallStackInfo>& cs = p->get_callstack();
  processor_t* proc = p->get_core(0);

  if (called_by_do_execveat_common(cs)) {
    p->step_until_insn(CSRW, t);

    CallStackInfo top = cs.back();
    std::string bin = top.bin();
    reg_t asid = proc->get_asid();

    p->get_asid2bin_map().insert({asid, bin});
    pprintf("Found mapping ASID: %" PRIu64 " bin: %s\n", asid, bin.c_str());
  }
  return CallStackInfo(k_set_mm_asid, "");
}

bool Function_k_set_mm_asid::called_by_do_execveat_common(std::vector<CallStackInfo>& cs) {
  if (cs.size() == 0) {
    return false;
  } else {
    auto back = cs.back();
    return (back.fn() == k_do_execveat_common);
  }
}

Function_k_pick_next_task_fair::Function_k_pick_next_task_fair(std::string name)
  : Function(name)
{
}

std::optional<CallStackInfo> Function_k_pick_next_task_fair::update_profiler(Profiler *p, trace_t& t) {
  processor_t* proc = p->get_core(0);
  std::optional<pid_t> pid = get_pid_next_task(p, proc);
  return {};
}

std::optional<pid_t> Function_k_pick_next_task_fair::get_pid_next_task(Profiler *p, processor_t* proc) {
  ObjdumpParser *obj = p->get_objdump_parser(KERNEL);
  std::string ret_reg = obj->func_ret_reg(k_pick_next_task_fair);
  unsigned int reg_idx = p->get_riscv_abi_ireg(ret_reg);

  mmu_t* mmu = proc->get_mmu();
  state_t* state = proc->get_state();
  addr_t next_task_ptr = state->XPR[reg_idx];
  if (next_task_ptr == 0) {
    pprintf("CFS doesn't have a task to schedule\n");
    return -1;
  } else {
    addr_t next_task_pid_addr = next_task_ptr + offsetof_task_struct_pid;
    uint32_t pid = mmu->load<uint32_t>(next_task_pid_addr);
    pprintf("CFS next task pid: %" PRIu32 "\n", pid);
    return std::optional<pid_t>(pid);
  }
}

/* Function_k_kernel_clone::Function_k_kernel_clone(std::string name) */
/* : Function(name) */
/* { */
/* } */

/* std::optional<CallStackInfo> update */

} // namespace Profiler
