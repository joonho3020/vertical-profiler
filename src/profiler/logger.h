#ifndef __LOGGER_H_
#define __LOGGER_H_

#include <string>
#include <vector>

#include "../spike-top/processor_lib.h"
#include "thread_pool.h"
#include "perfetto_trace.h"

namespace profiler {

class logger_t {
public:
  logger_t(std::string outdir);
  ~logger_t();

  void submit_trace_to_threadpool(trace_t& trace);

  void submit_packet(perfetto::packet_t* pkt);
  void submit_packet_trace_to_threadpool();
  void flush_packet_trace_to_threadpool();

  void stop();

  std::string spiketrace_filename(uint64_t idx);
  uint64_t get_trace_idx() { return trace_idx_; }
  std::string get_pctracedir() { return pctrace_outdir_; }

private:
  uint64_t trace_idx_ = 0;
  std::string pctrace_outdir_;
  threadpool_t<trace_t, std::string> pctrace_loggers_;

  FILE* prof_event_logfile_;
  std::vector<perfetto::packet_t*> packet_traces_;
  threadpool_t<std::vector<perfetto::packet_t*>, FILE*> packet_loggers_;

  const uint32_t PACKET_TRACE_FLUSH_THRESHOLD = 1000;
};

} // namespace profiler
#endif //__LOGGER_H_
