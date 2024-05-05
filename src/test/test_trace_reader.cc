

#include "../profiler/trace_reader.h"

using namespace profiler;

int main(int argc, char** argv) {
  std::string tracedir = "/scratch/joonho.whangbo/coding/FIRESIM_RUNS_DIR/boom-linux-multithread/sim_slot_0/COSPIKE-TRACES";
/* std::string tracedir = std::string(argv[1]); */
  trace_reader_t* reader = new trace_reader_t(
      0, 48, 200 * 1000, 5 * 1024 * 1024, tracedir);
  reader->start();

  uint64_t cur_step = 0;
  for (int i = 0; i < 425818; i++) {
    trace_buffer_t* buf = reader->cur_buffer();
    while (!buf->can_consume()) {
      ;
    }
    while (!buf->empty()) {
      rtl_step_t& step = buf->pop_front();
      if (cur_step > step.time) {
        step.print();
        printf("cur_step: %" PRIu64 " i : %d\n", cur_step, i);
        assert(false);
      }
      cur_step = step.time;
    }
    buf->done_consume();
    reader->pop_buffer();
  }
  printf("Test passed\n");

  return 0;
}
