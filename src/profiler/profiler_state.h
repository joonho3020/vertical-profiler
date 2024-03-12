#ifndef __PROFILER_STATE_H__
#define __PROFILER_STATE_H__

#include <vector>
#include <string>
#include "callstack_info.h"
#include "types.h"

namespace profiler {

class function_t;
class callstack_entry_t;

class profiler_state_t {
public:
  profiler_state_t();
  ~profiler_state_t();

  void add_prof_func (addr_t va, function_t* f);
  void add_prof_start(addr_t va);
  void add_prof_exit (addr_t va);

  reg2str_t& asid2bin();
  void update_asid2bin(reg_t asid, std::string bin);

  reg2str_t& pid2bin();
  std::optional<std::string> pid2bin_lookup(reg_t pid);
  void update_pid2bin(reg_t pid, std::string bin);

  void  set_curpid(reg_t pid);
  reg_t get_curpid();

  reg_t get_timestamp();
  void  update_timestamp(reg_t val);

  std::vector<callstack_entry_t>& get_callstack(reg_t pid);
  void pop_callstack (reg_t pid);
  void push_callstack(reg_t pid, callstack_entry_t entry);

  optreg_t found_registered_func_start_addr(reg_t va);
  optreg_t found_registered_func_exit_addr(reg_t va);

  function_t* get_profile_func(reg_t va);

  void dump_asid2bin_mapping(std::string outdir);

  std::vector<addr_t> start_pcs_to_profile() { return func_pc_prof_start_; }
  std::vector<addr_t> exit_pcs_to_profile()  { return func_pc_prof_exit_;  }

private:
  std::map<addr_t, function_t*> prof_pc_to_func_;
  std::vector<addr_t> func_pc_prof_start_;
  std::vector<addr_t> func_pc_prof_exit_;

  std::map<reg_t, std::vector<callstack_entry_t>> pid_to_callstack_;
  reg2str_t asid_to_bin_;

  // fork : add a new pid to bin mapping. the binary should be from the parent pid
  // exec : update the existing pid to bin mapping
  reg2str_t pid_to_bin_;


  // NOTE : There can be times when cur_pid and the pid from Spike does not
  // match. This is because we are updating cur_pid whenever CFS makes a
  // scheduling decision. However, the kernel has to perform the
  // "context_switch" function after the scheduling decision is made to
  // update the PID. Hence in this case, the value of cur_pid is one step
  // ahead of the functional sim's PID.
  reg_t cur_pid_ = 0;


  // TODO : We are using this as a replacement for "cycle" in the functional only mode.
  // Replace this with the actual timing information in the RTL driven mode.
  reg_t timestamp_ = 0;
};

}; // namespace profiler

#endif // __PROFILER_STATE_H__
