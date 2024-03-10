#include <string>
#include "../profiler/perfetto_trace.h"

using namespace profiler::perfetto;

void add_packet(event_trace_t* trace,
                std::string s,
                PACKET_TYPE t,
                uint64_t time) {
  trackevent_packet_t* tp = new trackevent_packet_t(s, t, 0, time);
  trace->add_packet(tp);
}

int main() {
  event_trace_t* trace = new event_trace_t("test-perfetto.txt");

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
