#include <inttypes.h>
#include <string>
#include <vector>
#include <assert.h>
#include <fstream>
#include <iostream>
#include "perfetto_trace.h"

namespace Profiler {
namespace Perfetto {

TracePacket::TracePacket(std::string name, PACKET_TYPE type_enum, uint64_t timestamp)
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
      type = "TYPE_INSANT";
      break;
    default:
      assert(false);
      break;
  }
}

void TracePacket::print(std::ofstream& os) {
  os << "packet {\n";
  os << "\ttimestamp: " << timestamp << "\n";
  os << "\ttrack_event: {\n";
  os << "\t\ttype: " << type << "\n";
  os << "\t\tname: \"" << name << "\"\n";
  os << "\t}\n";
  os << "\ttrusted_packet_sequence_id: 1\n";
  os << "}\n";
}

Trace::Trace(std::string ofname) {
  ofs.open(ofname);
  if (ofs.fail()) {
    std::cout << "Could not open output trace file" << std::endl;
    exit(1);
  }
}

void Trace::add_packet(TracePacket tp) {
  tp.print(ofs);
}

void Trace::close() {
  ofs.flush();
  ofs.close();
}


} // namespace Perfetto
} // namespace Profiler
