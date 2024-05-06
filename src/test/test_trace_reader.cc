

#include <chrono>
#include <iostream>
#include <assert.h>
#include "../lib/trace_reader.h"

using namespace std::chrono;

int main(int argc, char** argv) {
  if (argc < 3) {
    printf("usage: ./test_trace_reader <path to tracedir> <num files>\n");
    exit(1);
  }

  std::string tracedir = std::string(argv[1]);
  int num_files = atoi(argv[2]);

  trace_reader_t* reader = new trace_reader_t(
      0, 12, 200 * 1000, 5 * 1024 * 1024, tracedir);
  reader->start();

  auto start = high_resolution_clock::now();
  uint64_t cur_step = 0;
  for (int i = 0; i < num_files; i++) {
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
  auto end = high_resolution_clock::now();
  auto duration = duration_cast<seconds>(end - start);
  std::cout << "Test passed: " << duration.count() << " seconds\n";
  return 0;
}
