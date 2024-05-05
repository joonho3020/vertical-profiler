#ifndef __TRACE_READER_H__
#define __TRACE_READER_H__

#include "../spike-top/processor_lib.h"
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <inttypes.h>

namespace profiler {

class trace_buffer_t {
public:
  trace_buffer_t(size_t max_entries, size_t max_file_bytes);
  ~trace_buffer_t();

  bool empty();
  bool full();
  bool can_consume();
  void done_consume();
  uint8_t* get_buffer();
  rtl_step_t& pop_front();
  rtl_step_t& push_back();
  void generate_trace(int bytes_read);

private:
  size_t max_entries;
  size_t head;
  size_t tail;
  std::mutex consumable_mutex;
  bool consumable;
  std::vector<rtl_step_t*> trace;
  uint8_t* buffer;
};

class trace_reader_t {
public:
  trace_reader_t(int hartid, int nthreads, size_t per_buff_entries, size_t max_file_bytes, std::string trace_dir);
  ~trace_reader_t();

  trace_buffer_t* cur_buffer();
  void pop_buffer();
  void start();

private:
  void threadloop();

  std::string trace_dir;
  int hartid;
  int consumer_id;
  int producer_id;
  uint64_t trace_id;
  std::vector<trace_buffer_t*> buffers;
  size_t max_file_bytes;

  bool init;
  int nthreads;
  std::mutex buffer_mutex;
  std::vector<std::thread> threads;
};

} // namespace profiler


// while (trace_reader->empty()) {
//    ;
// }
// rtl_step_t& step = trace_reader->get_next_step();
// // profiler consumes step

#endif // __TRACE_READER_H__
