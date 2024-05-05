

#include "../profiler/trace_reader.h"

using namespace profiler;

int main() {
  std::string tracedir = "/scratch/joonho.whangbo/coding/FIRESIM_RUNS_DIR/boom-linux-multithread/sim_slot_0/COSPIKE-TRACES";
  trace_reader_t* reader = new trace_reader_t(
      0, 3, 200 * 1000, 5 * 1024 * 1024, tracedir);
  reader->start();

  for (int i = 0; i < 100; i++) {
    trace_buffer_t* buf = reader->cur_buffer();
    while (!buf->can_consume()) {
      ;
    }
    while (!buf->empty()) {
      rtl_step_t& step = buf->pop_front();
      step.print();
    }
    buf->done_consume();
    reader->pop_buffer();
  }

  return 0;
}
