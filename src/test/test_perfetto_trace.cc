#include <string>
#include "../profiler/perfetto_trace.h"


void add_packet(profiler::perfetto::event_trace_t* trace,
                std::string s,
                profiler::perfetto::PACKET_TYPE t,
                uint64_t time) {
  profiler::perfetto::packet_t tp(s, t, time);
  trace->add_packet(tp);
}

int main() {
  profiler::perfetto::event_trace_t* trace =
    new profiler::perfetto::event_trace_t("test-perfetto.txt");

  add_packet(trace, "ONE",    profiler::perfetto::TYPE_SLICE_BEGIN, 100);
  add_packet(trace, "ONE",    profiler::perfetto::TYPE_SLICE_END  , 200);

  add_packet(trace, "TWO",   profiler::perfetto::TYPE_SLICE_BEGIN, 200);
  add_packet(trace, "THREE", profiler::perfetto::TYPE_SLICE_BEGIN, 300);
  add_packet(trace, "THREE", profiler::perfetto::TYPE_SLICE_END,   400);
  add_packet(trace, "TWO",   profiler::perfetto::TYPE_SLICE_END,   400);

  add_packet(trace, "ONE",   profiler::perfetto::TYPE_SLICE_BEGIN, 500);
  add_packet(trace, "ONE",   profiler::perfetto::TYPE_SLICE_BEGIN, 600);

  trace->close();

  return 0;
}
