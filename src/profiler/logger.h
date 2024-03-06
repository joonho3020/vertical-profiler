#ifndef __LOGGER_H_
#define __LOGGER_H_

#include <string>
#include <vector>

#include "thread_pool.h"
#include "perfetto_trace.h"

namespace profiler {

void print_event_logs(std::vector<perfetto::packet_t> trace, FILE* ofile);

class logger_t {
public:
  logger_t(FILE* outfile);
  ~logger_t();

  void submit_packet(perfetto::packet_t pkt);
  void submit_packet_trace_to_threadpool();
  void flush_packet_trace_to_threadpool();
  void stop();

private:
  std::vector<perfetto::packet_t> packet_traces_;
  threadpool_t<std::vector<perfetto::packet_t>, FILE*> packet_loggers_;
  FILE* proflog_tp_;
  const uint32_t PACKET_TRACE_FLUSH_THRESHOLD = 1000;
};

} // namespace profiler
#endif //__LOGGER_H_
