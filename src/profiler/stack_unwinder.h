#ifndef __STACK_UNWINDER_H__
#define __STACK_UNWINDER_H__

#include <string>
#include <map>
#include <vector>

#include "../tracerv/trace_tracker.h"
#include "../tracerv/tracerv_processing.h"
#include "thread_pool.h"
#include "types.h"

namespace profiler {

struct named_insn_t {
  reg_t pc;
  reg_t cycle;
  std::string bin;
};

typedef std::vector<named_insn_t> named_trace_t;

class stack_unwinder_t {
public:
  stack_unwinder_t(
      std::vector<std::pair<std::string, std::string>> objdump_paths,
      FILE* stackfile);

  void stop();
  void submit_insn(reg_t pc, reg_t cycle, std::string bin);
  void flush_insns_to_threadpool();
  void submit_insns_to_threadpool();
  void process_instruction(uint64_t pc, uint64_t cycle, std::string bin);

private:
  threadpool_t<named_trace_t, stack_unwinder_t*> workers_;
  named_trace_t named_traces_;
  const uint32_t INSN_TRACE_FLUSH_THRESHOLD = 1000000;

  std::map<std::string, ObjdumpedBinary*> bin_dumps_;
  std::vector<LabelMeta*> label_stack_;
  Instr *last_instr_;
  FILE* stackfile_;
};

void process_traces(named_trace_t trace, stack_unwinder_t* su);

} // namespace Profiler

#endif //__STACK_UNWINDER_H__
