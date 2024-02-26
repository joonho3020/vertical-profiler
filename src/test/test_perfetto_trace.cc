#include <string>
#include "../src/perfetto_trace.h"


void add_packet(
    Profiler::Perfetto::Trace* trace,
    std::string s, Profiler::Perfetto::PACKET_TYPE t, uint64_t time) {
  Profiler::Perfetto::TracePacket tp(s, t, time);
  trace->add_packet(tp);
}

int main() {
  Profiler::Perfetto::Trace* trace = new Profiler::Perfetto::Trace("test-perfetto.txt");

  add_packet(trace, "ONE",    Profiler::Perfetto::TYPE_SLICE_BEGIN, 100);
  add_packet(trace, "ONE",    Profiler::Perfetto::TYPE_SLICE_END  , 200);

  add_packet(trace, "TWO",   Profiler::Perfetto::TYPE_SLICE_BEGIN, 200);
  add_packet(trace, "THREE", Profiler::Perfetto::TYPE_SLICE_BEGIN, 300);
  add_packet(trace, "THREE", Profiler::Perfetto::TYPE_SLICE_END,   400);
  add_packet(trace, "TWO",   Profiler::Perfetto::TYPE_SLICE_END,   400);

  add_packet(trace, "ONE",   Profiler::Perfetto::TYPE_SLICE_BEGIN, 500);
  add_packet(trace, "ONE",   Profiler::Perfetto::TYPE_SLICE_BEGIN, 600);

  trace->close();

  return 0;
}
