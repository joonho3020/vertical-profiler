#include <thread>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <utility>
#include <iostream>
#include <fstream>

#include <riscv/cfg.h>
#include <riscv/processor.h>

#include "types.h"
#include "thread_pool.h"

namespace Profiler {

void ThreadPool::start() {
  const uint32_t num_threads = std::thread::hardware_concurrency() / 2;
  for (uint32_t ii = 0; ii < num_threads; ++ii) {
    threads.emplace_back(std::thread(&ThreadPool::threadLoop, this));
  }
}

void ThreadPool::threadLoop() {
  while (true) {
    job_t job;
    trace_t trace;
    std::string oname;
    {
      std::unique_lock<std::mutex> lock(queue_mutex);
      mutex_condition.wait(lock, [this] {
          return !jobs.empty() || should_terminate;
          });
      if (should_terminate) {
        return;
      }
      job = jobs.front();
      jobs.pop();

      trace = traces.front();
      traces.pop();

      oname = ofnames.front();
      ofnames.pop();
    }
    job(trace, oname);
  }
}

void ThreadPool::queueJob(const job_t& job, const trace_t& trace, std::string& oname) {
    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        jobs.push(job);
        traces.push(trace);
        ofnames.push(oname);
    }
    mutex_condition.notify_one();
}

bool ThreadPool::busy() {
    bool poolbusy;
    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        poolbusy = !jobs.empty();
    }
    return poolbusy;
}

void ThreadPool::stop() {
    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        should_terminate = true;
    }
    mutex_condition.notify_all();
    for (std::thread& active_thread : threads) {
        active_thread.join();
    }
    threads.clear();
}

void printLogs(trace_t trace, std::string oname) {
  std::ofstream os(oname, std::ofstream::out);
  for (auto& t : trace) {
    os << std::hex << t.pc << " " << std::dec << t.asid << " " << t.prv << " " << t.prev_prv << "\n";
  }
  os.close();
}


} // namespace Profiler
