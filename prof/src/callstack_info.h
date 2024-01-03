#ifndef __CALLSTACK_INFO_H__
#define __CALLSTACK_INFO_H__

#include <string>
#include <optional>
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


class Function {
public:
  Function(std::string name, addr_t va_s, addr_t va_e);

  std::string name() { return n; }
  addr_t va_start()  { return va_s; }
  addr_t va_end()    { return va_e; }
  virtual CallStackInfo update_profiler(Profiler *p, trace_t& t) = 0;

private:
  const std::string n;
  const addr_t va_s;
  const addr_t va_e;
};


class Function_k_do_execveat_common : public Function {
public:
  Function_k_do_execveat_common(std::string name, addr_t va_s, addr_t va_e);
  virtual CallStackInfo update_profiler(Profiler *p, trace_t& t) override;

private:
  std::string find_exec_syscall_filepath(Profiler *p, processor_t *proc);
  const addr_t MAX_FILENAME_SIZE = 200;
};


class Function_k_set_mm_asid : public Function {
public:
  Function_k_set_mm_asid(std::string name, addr_t va_s, addr_t va_e);
  virtual CallStackInfo update_profiler(Profiler *p, trace_t& t) override;

private:
  bool called_by_do_execveat_common(std::vector<CallStackInfo>& cs);
};

class Function_k_pick_next_task_fair : public Function {
public:
  Function_k_pick_next_task_fair(std::string name, addr_t va_s, addr_t va_e);
  virtual CallStackInfo update_profiler(Profiler *p, trace_t& t) override;

private:
  std::optional<pid_t> get_pid_next_task(Profiler *p, processor_t* proc);
};

} // namespace Profiler


#endif //__CALLSTACK_INFO_H__
