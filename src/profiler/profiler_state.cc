
#include <vector>
#include <optional>
#include "profiler_state.h"
#include "types.h"
#include "callstack_info.h"

namespace profiler {

profiler_state_t::profiler_state_t() {
}

profiler_state_t::~profiler_state_t() {
}

void profiler_state_t::add_prof_func(addr_t va, function_t* f) {
  prof_pc_to_func_[va] = f;
  func_pc_prof_start_.push_back(va);
}

void profiler_state_t::add_prof_exit(addr_t va) {
  func_pc_prof_exit_.push_back(va);
}

std::vector<callstack_entry_t>& profiler_state_t::get_callstack(reg_t pid) {
  if (pid_to_callstack_.find(pid) == pid_to_callstack_.end()) {
    pprintf("Callstack for PID %u not found\n", pid);
    pid_to_callstack_[pid] = std::vector<callstack_entry_t>();
  }
  return pid_to_callstack_[pid];
}

void profiler_state_t::pop_callstack(reg_t pid) {
  pid_to_callstack_[pid].pop_back();
}

void profiler_state_t::push_callstack(reg_t pid, callstack_entry_t entry) {
  pid_to_callstack_[pid].push_back(entry);
}

optreg_t profiler_state_t::found_registered_func_start_addr(reg_t va) {
  for (auto &x : func_pc_prof_start_) {
    if (unlikely(x == va)) return optreg_t(va);
  }
  return {};
}

optreg_t profiler_state_t::found_registered_func_exit_addr(reg_t va) {
  for (auto &x : func_pc_prof_exit_) {
    if (unlikely(x == va)) return optreg_t(va);
  }
  return {};
}

function_t* profiler_state_t::get_profile_func(reg_t va) {
  return prof_pc_to_func_[va];
}

}; // namespace Profiler
