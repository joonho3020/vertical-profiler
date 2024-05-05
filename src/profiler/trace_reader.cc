#include "trace_reader.h"
#include <sys/stat.h>
#include <string>
#include <zlib.h>
#include <filesystem>

namespace profiler {

trace_buffer_t::trace_buffer_t(size_t max_entries, size_t max_file_bytes) {
  for (size_t i = 0; i < max_entries + 1; i++) this->trace.push_back(new rtl_step_t());
  this->buffer = (uint8_t*)malloc(sizeof(uint8_t) * max_file_bytes);
  this->max_entries = max_entries + 1;
  this->head = 0;
  this->tail = 0;
  this->consumable = false;
}

trace_buffer_t::~trace_buffer_t() {
  delete this->buffer;
}

bool trace_buffer_t::empty() {
  return (head == tail);
}

bool trace_buffer_t::full() {
  return (head == (tail + 1) % max_entries);
}

bool trace_buffer_t::can_consume() {
  bool ret;
  {
    std::unique_lock<std::mutex> lock(consumable_mutex);
    ret = consumable;
  }
  return ret;
}

void trace_buffer_t::done_consume() {
  {
    std::unique_lock<std::mutex> lock(consumable_mutex);
    consumable = false;
  }
}

uint8_t* trace_buffer_t::get_buffer() {
  return buffer;
}

rtl_step_t& trace_buffer_t::pop_front() {
  size_t cur_head = head;
  head = (head + 1) % max_entries;
  return *trace[head];
}

rtl_step_t& trace_buffer_t::push_back() {
  assert(!this->full());

  size_t cur_tail = tail;
  tail = (tail + 1) % max_entries;
  return *trace[cur_tail];
}

// TODO : Bottleneck??
void trace_buffer_t::generate_trace(int bytes_read) {
  int i = 0;
  int digits[] = {0, 10, 16, 10, 10, 10, 10, 10, 16};
  uint64_t trace_members[] = {0, 0, 0, 0, 0, 0, 0, 0, 0};

  do {
    int index = 0;
    rtl_step_t& step = this->push_back();
    char cur_char = (char)buffer[i];
    while (cur_char != '\n') {
      uint64_t x = 0;
      while (cur_char != ' ' && cur_char != '\n') {
/* printf("cur_char: %c\n", cur_char); */
        int v = cur_char >= 97 ? cur_char - 87 : cur_char - '0';
        x = (x * digits[index]) + v;
        cur_char = (char)buffer[++i];
      }
/* printf("inner loop done index: %d x: %d\n", index, x); */
      trace_members[index++] = x;
      if (cur_char == ' ')
        cur_char = (char)buffer[++i];
    }
    i++;
    step.time    = trace_members[1];
    step.pc      = trace_members[2];
    step.val     = trace_members[3];
    step.except  = trace_members[4];
    step.intrpt  = trace_members[5];
    step.has_w   = trace_members[6];
    step.cause   = trace_members[7];
    step.wdata   = trace_members[8];
  } while (i < bytes_read);
  {
    std::unique_lock<std::mutex> lock(consumable_mutex);
    consumable = true;
  }
}

///////////////////////////////////////////////////////////////////////////////

trace_reader_t::trace_reader_t(int hartid, int nthreads, size_t per_buff_entries, size_t max_file_bytes, std::string trace_dir) {
  this->trace_dir = trace_dir;
  this->hartid = hartid;
  this->consumer_id = 0;
  this->producer_id = 0;
  this->trace_id = 0;
  this->max_file_bytes = max_file_bytes;
  this->nthreads = nthreads;
  this->init = true;
  for (int i = 0; i < nthreads; i++) {
    this->buffers.push_back(new trace_buffer_t(per_buff_entries, max_file_bytes));
  }
}

trace_reader_t::~trace_reader_t() {
  for (auto& b: this->buffers)
    delete b;

  for (auto& t : threads)
    t.join();
}

trace_buffer_t* trace_reader_t::cur_buffer() {
  return buffers[consumer_id];
}

void trace_reader_t::pop_buffer() {
  {
    std::unique_lock<std::mutex> lock(buffer_mutex);
    printf("consume done id: %d\n", consumer_id);
    consumer_id = (consumer_id + 1) % buffers.size();
  }
}

void trace_reader_t::start() {
  const uint32_t num_threads = std::max(
      std::thread::hardware_concurrency() / 16,
      std::min(std::thread::hardware_concurrency(), (unsigned int)this->nthreads));
  for (uint32_t ii = 0; ii < num_threads; ++ii) {
    threads.emplace_back(std::thread(&trace_reader_t::threadloop, this));
  }
}

void trace_reader_t::threadloop() {
  while (true) {
    int pid = -1;
    std::string file;
    {
      std::unique_lock<std::mutex> lock(buffer_mutex);
      file = "COSPIKE-TRACE-" +
             std::to_string(hartid) + "-" +
             std::to_string(trace_id) + ".gz";
/* std::string path = trace_dir + "/" + file; */
/* struct stat sbuf; */
/* bool has_file = (stat (path.c_str(), &sbuf) == 0); */
      const std::filesystem::path path{trace_dir + "/" + file};
      bool has_file = std::filesystem::exists(path);
      if (has_file) {
        trace_buffer_t* cbuf = buffers[consumer_id];
        if ((producer_id != consumer_id || init)) {
          printf("start decompressing trace: %d producer id: %d\n", trace_id, producer_id);
          this->init = false;
          trace_id++;
          pid = producer_id;
          producer_id = (producer_id + 1) % buffers.size();
        }
      }
    }
    if (pid >= 0) {
      std::string path = trace_dir + "/" + file;
      trace_buffer_t* pbuf = buffers[pid];
      gzFile fp = gzopen(path.c_str(), "r");
      int bytes_read = gzread(fp, pbuf->get_buffer(), this->max_file_bytes);
      pbuf->generate_trace(bytes_read);
    }
  }
}

} // namespace profiler
