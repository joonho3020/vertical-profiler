
#include <vector>
#include <string>

#include "logger.h"
#include "perfetto_trace.h"

namespace profiler {

void print_event_logs(std::vector<perfetto::packet_t> trace, FILE* ofile) {
  for (auto &pkt : trace) {
    pkt.print(ofile);
  }
}

logger_t::logger_t(FILE* outfile) {
  packet_loggers_.start(1);
  proflog_tp_ = outfile;
}

logger_t::~logger_t() {
}

void logger_t::submit_packet(perfetto::packet_t pkt) {
  packet_traces_.push_back(pkt);
}

void logger_t::submit_packet_trace_to_threadpool() {
  if ((uint32_t)packet_traces_.size() >= PACKET_TRACE_FLUSH_THRESHOLD) {
    flush_packet_trace_to_threadpool();
  }
}

void logger_t::flush_packet_trace_to_threadpool() {
  packet_loggers_.queue_job(print_event_logs, packet_traces_, proflog_tp_);
  packet_traces_.clear();
}

void logger_t::stop() {
  flush_packet_trace_to_threadpool();
  packet_loggers_.stop();
}

} // namespace profiler
