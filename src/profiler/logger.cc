
#include <vector>
#include <string>

#include "logger.h"
#include "perfetto_trace.h"

namespace profiler {

logger_t::logger_t(std::string outdir)
  : pctrace_outdir_(outdir + "/traces")
{
  pctrace_loggers_.start(4);
  packet_loggers_.start(1);

  proflog_tp_ = fopen((outdir + "/PROF-EVENT-LOGS").c_str(), "w");
  if (proflog_tp_ == NULL) {
    fprintf(stderr, "Unable to open log file PROF-LOGS-THREADPOOL\n");
    exit(-1);
  }
}

logger_t::~logger_t() {
}

void logger_t::stop() {
  pctrace_loggers_.stop();
  packet_loggers_.stop();
}

std::string logger_t::spiketrace_filename(uint64_t idx) {
  std::string sfx;
  if (idx < 10) {
    sfx = "000000000" + std::to_string(idx);
  } else if (idx < 100) {
    sfx = "00000000" + std::to_string(idx);
  } else if (idx < 1000) {
    sfx = "0000000" + std::to_string(idx);
  } else if (idx < 10000) {
    sfx = "000000" + std::to_string(idx);
  } else if (idx < 100000) {
    sfx = "00000" + std::to_string(idx);
  } else if (idx < 1000000) {
    sfx = "0000" + std::to_string(idx);
  } else if (idx < 10000000) {
    sfx = "000" + std::to_string(idx);
  } else if (idx < 100000000) {
    sfx = "00" + std::to_string(idx);
  } else if (idx < 1000000000) {
    sfx = "0" + std::to_string(idx);
  } else {
    sfx = std::to_string(idx);
  }
  return ("SPIKETRACE-" + sfx);
}

void logger_t::submit_trace_to_threadpool(trace_t& trace) {
  std::string name = pctrace_outdir_ + "/" + spiketrace_filename(trace_idx_);
  ++trace_idx_;
  pctrace_loggers_.queue_job(print_insn_logs, trace, name);
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

} // namespace profiler
