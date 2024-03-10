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

class packet_t {
public:
  packet_t(std::string name);
  virtual void print(FILE* of) { }

protected:
  std::string name_;
};

// corresponds to the trace_packet.proto
class trackevent_packet_t : public packet_t {
public:
  trackevent_packet_t(std::string name, PACKET_TYPE type_enum,
                      int trackid, uint64_t timestamp);
  virtual void print(FILE* of) override;

private:
  std::string type_;
  int trackid_;
  uint64_t timestamp_;
};

class trackdescriptor_packet_t : public packet_t {
public:
  trackdescriptor_packet_t(std::string name, int trackid);
  virtual void print(FILE* of) override;

private:
  int trackid_;
};

class event_trace_t {
public:
  event_trace_t(std::string ofname);
  void add_packet(packet_t* tp);
  void close();

private:
  FILE* of;
};

} // namespace perfetto
} // namespace profiler

#endif // __PERFETTO_TRACE_H__
