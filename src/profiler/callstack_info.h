#ifndef __CALLSTACK_INFO_H__
#define __CALLSTACK_INFO_H__

#include <string>
#include <optional>
#include <vector>
#include <riscv/processor.h>

#include "types.h"
#include "profiler_state.h"
#include "../spike-top/processor_lib.h"

namespace profiler {

class profiler_t;

struct callstack_entry_t {
public:
  callstack_entry_t(std::string func, std::string binary);
  std::string fn();
  std::string bin();


private:
  std::string func;
  std::string binary;
};

typedef std::optional<callstack_entry_t> opt_cs_entry_t;


class function_t {
public:
  function_t(std::string name);

  std::string name() { return n; }

  // when called, updates the profiler state under the hood
  virtual opt_cs_entry_t update_profiler(profiler_t* p) = 0;

private:
  const std::string n;
};

class kernel_function_t : public function_t {
public:
  kernel_function_t(std::string name);
  virtual opt_cs_entry_t update_profiler(profiler_t* p) = 0;

protected:
  addr_t get_current_ptr(processor_lib_t* proc);
  pid_t  get_current_pid(processor_lib_t* proc);
};


class kf_do_execveat_common : public kernel_function_t {
public:
  kf_do_execveat_common(std::string name);
  virtual opt_cs_entry_t update_profiler(profiler_t* p) override;

private:
  std::string find_exec_syscall_filepath(profiler_t *p, processor_lib_t *proc);
  void update_pid2bin(profiler_t* p, processor_lib_t* proc, std::string filepath);
  const addr_t MAX_FILENAME_SIZE = 200;
};


class kf_set_mm_asid : public kernel_function_t {
public:
  kf_set_mm_asid(std::string name);
  virtual opt_cs_entry_t update_profiler(profiler_t* p) override;

private:
  bool called_by_do_execveat_common(std::vector<callstack_entry_t>& cs);
};

class kf_kernel_clone : public kernel_function_t {
public:
  kf_kernel_clone(std::string name);
  virtual opt_cs_entry_t update_profiler(profiler_t* p) override;

private:
  pid_t get_forked_task_pid(profiler_t* p, processor_lib_t* proc);
};

class kf_pick_next_task_fair : public kernel_function_t {
public:
  kf_pick_next_task_fair(std::string name);
  virtual opt_cs_entry_t update_profiler(profiler_t* p) override;

private:
  void get_pid_next_task(profiler_t *p, processor_lib_t* proc);
};

class kf_finish_task_switch : public kernel_function_t {
public:
  kf_finish_task_switch(std::string name);
  virtual opt_cs_entry_t update_profiler(profiler_t* p) override;

private:
  pid_t get_prev_pid(profiler_t *p, processor_lib_t* proc);
};

} // namespace profiler_t


#endif //__CALLSTACK_INFO_H__
