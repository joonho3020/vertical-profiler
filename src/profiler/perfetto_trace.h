#ifndef __PERFETTO_TRACE_H__
#define __PERFETTO_TRACE_H__

#include <inttypes.h>
#include <string>
#include <vector>
#include <fstream>


namespace profiler {
namespace perfetto {

enum PACKET_TYPE {
  TYPE_SLICE_BEGIN = 0,
  TYPE_SLICE_END   = 1,
  TYPE_INSTANT     = 2
};

class packet_t { // corresponds to the trace_packet.proto
public:
  packet_t(std::string name, PACKET_TYPE type_enum, uint64_t timestamp);
  void print(FILE* of);

private:
  std::string name;
  std::string type;
  uint64_t timestamp;
};

class event_trace_t {
public:
  event_trace_t(std::string ofname);
  void add_packet(packet_t tp);
  void close();

private:
  FILE* of;
};

} // namespace perfetto
} // namespace profiler

#endif // __PERFETTO_TRACE_H__
