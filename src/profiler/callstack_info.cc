#include <string>
#include <optional>
#include <map>
#include <vector>

#include <riscv/processor.h>
#include <riscv/mmu.h>

#include "callstack_info.h"
#include "objdump_parser.h"
#include "profiler.h"
#include "types.h"
#include "perfetto_trace.h"
#include "../spike-top/processor_lib.h"

namespace profiler {

callstack_entry_t::callstack_entry_t(std::string func, std::string binary)
  : func(func), binary(binary)
{
}

std::string callstack_entry_t::fn() {
  return func;
}

std::string callstack_entry_t::bin() {
  return binary;
}


function_t::function_t(std::string name)
  : n(name)
{
}


kernel_function_t::kernel_function_t(std::string name)
  : function_t(name)
{
}

addr_t kernel_function_t::get_current_ptr(processor_lib_t* proc) {
  state_t* s = proc->get_state();
  unsigned int tidx = riscv_abi_ireg["tp"];
  return s->XPR[tidx];
}

pid_t kernel_function_t::get_current_pid(processor_lib_t* proc) {
  mmu_t* mmu = proc->get_mmu();

  addr_t curr_ptr = get_current_ptr(proc);
  addr_t pid_addr = curr_ptr + offsetof_task_struct_pid;
  pid_t  pid = mmu->load<pid_t>(pid_addr);
  return pid;
}

kf_do_execveat_common::kf_do_execveat_common(std::string name) 
  : kernel_function_t(name)
{
}

opt_cs_entry_t kf_do_execveat_common::update_profiler(profiler_t* p) {
  // TODO : multicore support
  processor_lib_t* proc = p->get_core(0);

  // map current asid with binary name
  std::string filepath = find_exec_syscall_filepath(p, proc);
  update_pid2bin(p, proc, filepath);
  return callstack_entry_t(k_do_execveat_common, filepath);
}

std::string kf_do_execveat_common::find_exec_syscall_filepath(
    profiler_t* p,
    processor_lib_t* proc)
{
  objdump_parser_t *obj = p->get_objdump_parser(KERNEL);
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
  return name;
}

void kf_do_execveat_common::update_pid2bin(
    profiler_t* p,
    processor_lib_t* proc,
    std::string filepath)
{
  state_t* state = proc->get_state();
  addr_t va = state->pc;
  addr_t asid = proc->get_asid();

  // update the pid mapping
  reg2str_t& pid2bin = p->pstate->pid2bin();
  reg_t pid = get_current_pid(proc);

  auto it = pid2bin.find(pid);
  if (it != pid2bin.end()) {
    pprintf("Updating pid2bin[%u]: %s -> %s\n",
      pid, it->second.c_str(), filepath.c_str());

    p->submit_packet(perfetto::packet_t(
          std::string(k_do_execveat_common),
          perfetto::TYPE_INSTANT,
          p->pstate->get_insn_retired()));
  }
  pid2bin[pid] = filepath;
}


kf_set_mm_asid::kf_set_mm_asid(std::string name)
  : kernel_function_t(name)
{
}

opt_cs_entry_t kf_set_mm_asid::update_profiler(profiler_t* p) {
  processor_lib_t* proc = p->get_core(0);
  reg_t pid = get_current_pid(proc);
  std::vector<callstack_entry_t>& cs = p->pstate->get_callstack(pid);

  if (called_by_do_execveat_common(cs)) {
    p->step_until_insn(CSRW);

    callstack_entry_t top = cs.back();
    std::string bin = top.bin();
    reg_t asid = proc->get_asid();

    p->pstate->asid2bin().insert({asid, bin});

    pprintf("Found mapping ASID: %" PRIu64 " PID: %u bin: %s\n",
        asid, pid, bin.c_str());

    p->submit_packet(perfetto::packet_t(
          std::string(k_set_mm_asid),
          perfetto::TYPE_INSTANT,
          p->pstate->get_insn_retired()));

    if (pid != p->pstate->get_curpid()) {
      pexit("%d Prof internal pid: %u, spike pid: %u\n",
          __LINE__, p->pstate->get_curpid(), pid);
    }
  }
  return callstack_entry_t(k_set_mm_asid, "");
}

bool kf_set_mm_asid::called_by_do_execveat_common(std::vector<callstack_entry_t>& cs) {
  if (cs.size() == 0) {
    return false;
  } else {
    auto back = cs.back();
    return (back.fn() == k_do_execveat_common);
  }
}

kf_kernel_clone::kf_kernel_clone(std::string name)
  : kernel_function_t(name)
{
}

opt_cs_entry_t kf_kernel_clone::update_profiler(profiler_t* p) {
  processor_lib_t* proc = p->get_core(0);
  pid_t newpid = get_forked_task_pid(p, proc);
  pid_t parpid = get_current_pid(proc);


  reg2str_t& pid2bin = p->pstate->pid2bin();
  auto it = pid2bin.find(parpid);
  if (it != pid2bin.end()) {
    std::string bin = it->second;
    pid2bin[newpid] = bin;
  } else {
    pid2bin[newpid] = "X";
  }

  pprintf("Forked p: %u c: %u bin: %s\n", parpid, newpid, pid2bin[newpid].c_str());

  p->submit_packet(perfetto::packet_t(
        std::string(k_kernel_clone),
        perfetto::TYPE_INSTANT,
        p->pstate->get_insn_retired()));

  return {};
}

pid_t kf_kernel_clone::get_forked_task_pid(profiler_t* p, processor_lib_t* proc) {
  objdump_parser_t *obj = p->get_objdump_parser(KERNEL);
  std::string ret_reg = obj->func_ret_reg(k_kernel_clone);
  unsigned int reg_idx = riscv_abi_ireg[ret_reg];

  mmu_t* mmu = proc->get_mmu();
  state_t* state = proc->get_state();
  pid_t next_pid = state->XPR[reg_idx];
  return next_pid;
}

kf_pick_next_task_fair::kf_pick_next_task_fair(std::string name)
  : kernel_function_t(name)
{
}

opt_cs_entry_t kf_pick_next_task_fair::update_profiler(profiler_t* p) {
  processor_lib_t* proc = p->get_core(0);
  optreg_t pid = get_pid_next_task(p, proc);
  return {};
}

optreg_t kf_pick_next_task_fair::get_pid_next_task(profiler_t *p, processor_lib_t* proc) {
  objdump_parser_t *obj = p->get_objdump_parser(KERNEL);
  std::string ret_reg = obj->func_ret_reg(k_pick_next_task_fair);
  unsigned int reg_idx = riscv_abi_ireg[ret_reg];

  mmu_t* mmu = proc->get_mmu();
  state_t* state = proc->get_state();
  addr_t next_task_ptr = state->XPR[reg_idx];
  if (next_task_ptr == 0) {
    pprintf("CFS doesn't have a task to schedule, ret_reg: %s\n", ret_reg.c_str());
    return {};
  } else {
    addr_t next_task_pid_addr = next_task_ptr + offsetof_task_struct_pid;
    pid_t pid = mmu->load<pid_t>(next_task_pid_addr);

    auto pid2bin = p->pstate->pid2bin();
/* pprintf("CFS next_task_ptr: 0x%" PRIx64 "\n", next_task_ptr); */
/* pprintf("CFS next task pid: %" PRIu32 " %s\n", pid, pid2bin[pid].c_str()); */
    return optreg_t(pid);
  }
}

kf_finish_task_switch::kf_finish_task_switch(std::string name)
  : kernel_function_t(name)
{
}

opt_cs_entry_t kf_finish_task_switch::update_profiler(profiler_t* p) {
  processor_lib_t* proc = p->get_core(0);
  pid_t cur_pid  = get_current_pid(proc);
  pid_t prev_pid = get_prev_pid(p, proc);
  p->pstate->set_curpid(cur_pid);

/* pprintf("ContextSwitch Finished %u -> %u\n", prev_pid, cur_pid); */

  auto pid2bin = p->pstate->pid2bin();

  std::string prev_bin = pid2bin[prev_pid];
  p->submit_packet(perfetto::packet_t(
        prev_bin,
        perfetto::TYPE_SLICE_END,
        p->pstate->get_insn_retired()));

  std::string cur_bin = pid2bin[cur_pid];
  p->submit_packet(perfetto::packet_t(
        cur_bin,
        perfetto::TYPE_SLICE_BEGIN,
        p->pstate->get_insn_retired()));

  return callstack_entry_t(k_finish_task_switch, "");
}

pid_t kf_finish_task_switch::get_prev_pid(profiler_t *p, processor_lib_t* proc) {
  objdump_parser_t *obj = p->get_objdump_parser(KERNEL);
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

} // namespace profiler_t
