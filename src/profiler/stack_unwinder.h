#ifndef __STACK_UNWINDER_H__
#define __STACK_UNWINDER_H__

#include <string>
#include <map>
#include <vector>

#include "trace_tracker.h"
#include "tracerv_processing.h"

namespace Profiler {

class StackUnwinder {
public:
  StackUnwinder(
      std::vector<std::pair<std::string, std::string>> objdump_paths,
      FILE* stackfile);

  void addInstruction(uint64_t inst_addr, uint64_t cycle, std::string binary);

private:
  std::map<std::string, ObjdumpedBinary*> bin_dumps;
  std::vector<LabelMeta*> label_stack;
  Instr *last_instr;
  FILE* stackfile;
};

} // namespace Profiler

#endif //__STACK_UNWINDER_H__
