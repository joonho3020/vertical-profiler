#include <inttypes.h>
#include <string>
#include <vector>
#include <assert.h>
#include <fstream>
#include <iostream>
#include "perfetto_trace.h"

namespace profiler {
namespace perfetto {

packet_t::packet_t(std::string name) : name_(name) {
}

trackevent_packet_t::trackevent_packet_t(std::string name, PACKET_TYPE type_enum,
                                         int trackid, uint64_t timestamp)
  : packet_t(name), trackid_(trackid), timestamp_(timestamp)
{
  switch (type_enum) {
    case TYPE_SLICE_BEGIN:
      type_ = "TYPE_SLICE_BEGIN";
      break;
    case TYPE_SLICE_END:
      type_ = "TYPE_SLICE_END";
      break;
    case TYPE_INSTANT:
      type_ = "TYPE_INSTANT";
      break;
    default:
      assert(false);
      break;
  }
}

void trackevent_packet_t::print(FILE* of) {
  fprintf(of, "packet {\n");
  fprintf(of, "  timestamp: %" PRIu64 "\n", timestamp_);
  fprintf(of, "  track_event: {\n");
  fprintf(of, "    type: %s\n", type_.c_str());
  fprintf(of, "    name: \"%s\"\n", name_.c_str());
  fprintf(of, "    track_uuid: %d\n", trackid_);
  fprintf(of, "  }\n");
  fprintf(of, "  trusted_packet_sequence_id: 1\n");
  fprintf(of, "}\n");
  fflush(of);
}

trackdescriptor_packet_t::trackdescriptor_packet_t(std::string name, int trackid)
  : packet_t(name), trackid_(trackid)
{
}

void trackdescriptor_packet_t::print(FILE* of) {
  fprintf(of, "packet {\n");
  fprintf(of, "  track_descriptor {\n");
  fprintf(of, "    name: \"%s\"\n", name_.c_str());
  fprintf(of, "    uuid: %d\n", trackid_);
  fprintf(of, "  }\n");
  fprintf(of, "}\n");
  fflush(of);
}

event_trace_t::event_trace_t(std::string ofname) {
  of = fopen(ofname.c_str(), "w");
  if (of == NULL) {
    fprintf(stderr, "Unable to open log file %s\n", ofname.c_str());
    exit(-1);
  }
}

void event_trace_t::add_packet(packet_t* tp) {
  tp->print(of);
}

void event_trace_t::close() {
  fclose(of);
}

} // namespace perfetto
} // namespace profiler
