#include <inttypes.h>
#include <string>
#include <vector>
#include <assert.h>
#include <fstream>
#include <iostream>
#include "perfetto_trace.h"

namespace profiler {
namespace perfetto {

packet_t::packet_t(std::string name, PACKET_TYPE type_enum, uint64_t timestamp)
  : name(name), timestamp(timestamp)
{
  switch (type_enum) {
    case TYPE_SLICE_BEGIN:
      type = "TYPE_SLICE_BEGIN";
      break;
    case TYPE_SLICE_END:
      type = "TYPE_SLICE_END";
      break;
    case TYPE_INSTANT:
      type = "TYPE_INSTANT";
      break;
    default:
      assert(false);
      break;
  }
}

void packet_t::print(FILE* of) {
  fprintf(of, "packet {\n");
  fprintf(of, "\ttimestamp: %" PRIu64 "\n", timestamp);
  fprintf(of, "\ttrack_event: {\n");
  fprintf(of, "\t\ttype: %s\n", type.c_str());
  fprintf(of, "\t\tname: \"%s\"\n", name.c_str());
  fprintf(of, "\t}\n");
  fprintf(of, "\ttrusted_packet_sequence_id: 1\n");
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

void event_trace_t::add_packet(packet_t tp) {
  tp.print(of);
}

void event_trace_t::close() {
  fclose(of);
}


} // namespace perfetto
} // namespace profiler
