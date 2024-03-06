#ifndef __STACK_UNWINDER_H__
#define __STACK_UNWINDER_H__

#include <string>
#include <map>
#include <vector>

#include "../tracerv/trace_tracker.h"
#include "../tracerv/tracerv_processing.h"

namespace profiler {

class stack_unwinder_t {
public:
  stack_unwinder_t(
      std::vector<std::pair<std::string, std::string>> objdump_paths,
      FILE* stackfile);

  void add_instruction(uint64_t inst_addr, uint64_t cycle, std::string binary);

private:
  std::map<std::string, ObjdumpedBinary*> bin_dumps;
  std::vector<LabelMeta*> label_stack;
  Instr *last_instr;
  FILE* stackfile;
};

} // namespace Profiler

#endif //__STACK_UNWINDER_H__
