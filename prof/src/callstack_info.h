#ifndef __CALLSTACK_INFO_H__
#define __CALLSTACK_INFO_H__

#include <string>
#include <optional>
#include <vector>
#include <riscv/processor.h>

#include "types.h"

namespace Profiler {

  class Profiler;

struct CallStackInfo {
public:
  CallStackInfo(std::string func, std::string binary);
  std::string fn();
  std::string bin();


private:
  std::string func;
  std::string binary;
};

typedef std::optional<CallStackInfo> OptCallStackInfo;


class Function {
public:
  Function(std::string name);

  std::string name() { return n; }
  virtual OptCallStackInfo update_profiler(Profiler *p, trace_t& t) = 0;

private:
  const std::string n;
};


class KernelFunction : public Function {
public:
  KernelFunction(std::string name);
  virtual OptCallStackInfo update_profiler(Profiler *p, trace_t& t) = 0;

protected:
  addr_t get_current_ptr(processor_t* proc);
  pid_t  get_current_pid(processor_t* proc);
};


class KF_do_execveat_common : public KernelFunction {
public:
  KF_do_execveat_common(std::string name);
  virtual OptCallStackInfo update_profiler(Profiler *p, trace_t& t) override;

private:
  std::string find_exec_syscall_filepath(Profiler *p, processor_t *proc);
  const addr_t MAX_FILENAME_SIZE = 200;
};


class KF_set_mm_asid : public KernelFunction {
public:
  KF_set_mm_asid(std::string name);
  virtual OptCallStackInfo update_profiler(Profiler *p, trace_t& t) override;

private:
  bool called_by_do_execveat_common(std::vector<CallStackInfo>& cs);
};

class KF_pick_next_task_fair : public KernelFunction {
public:
  KF_pick_next_task_fair(std::string name);
  virtual OptCallStackInfo update_profiler(Profiler *p, trace_t& t) override;

private:
  std::optional<pid_t> get_pid_next_task(Profiler *p, processor_t* proc);
};

class KF_kernel_clone : public KernelFunction {
public:
  KF_kernel_clone(std::string name);
  virtual OptCallStackInfo update_profiler(Profiler *p, trace_t& t) override;

private:
  pid_t get_forked_task_pid(Profiler* p, processor_t* proc);
};

} // namespace Profiler


#endif //__CALLSTACK_INFO_H__
