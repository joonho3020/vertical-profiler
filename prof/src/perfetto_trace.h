#ifndef __PERFETTO_TRACE_H__
#define __PERFETTO_TRACE_H__

#include <inttypes.h>
#include <string>
#include <vector>
#include <fstream>


namespace Profiler {
namespace Perfetto {

enum PACKET_TYPE {
  TYPE_SLICE_BEGIN = 0,
  TYPE_SLICE_END   = 1,
  TYPE_INSTANT     = 2
};

class TracePacket { // corresponds to the trace_packet.proto
public:
  TracePacket(std::string name, PACKET_TYPE type_enum, uint64_t timestamp);
  void print(std::ofstream& os);

private:
  std::string name;
  std::string type;
  uint64_t timestamp;
};


class Trace {
public:
  Trace(std::string ofname);
  void add_packet(TracePacket tp);
  void close();

private:
  std::ofstream ofs;
};

} // namespace Perfetto
} // namespace Profiler


#endif // __PERFETTO_TRACE_H__
