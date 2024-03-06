#include <thread>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <utility>
#include <iostream>
#include <fstream>

#include <riscv/cfg.h>
#include <riscv/processor.h>

#include "perfetto_trace.h"
#include "types.h"
#include "thread_pool.h"

namespace profiler {

void print_insn_logs(trace_t trace, std::string oname) {
  std::ofstream os(oname, std::ofstream::out);
  for (auto& t : trace) {
    os << std::hex << t.pc << " " << std::dec << t.asid << " " << t.prv << " " << t.prev_prv << "\n";
  }
  os.close();
}

void print_event_logs(std::vector<perfetto::packet_t> trace, FILE* ofile) {
  for (auto &pkt : trace) {
    pkt.print(ofile);
  }
}


} // namespace profiler
