#ifndef __THREAD_POOL_H__
#define __THREAD_POOL_H__

/* https://stackoverflow.com/questions/15752659/thread-pooling-in-c11 */

#include <cstdint>
#include <thread>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <utility>
#include <iostream>
#include <fstream>
#include <string>

#include <riscv/cfg.h>
#include <riscv/processor.h>
#include "types.h"
#include "perfetto_trace.h"

namespace Profiler {

template <class T, class S>
class ThreadPool {
typedef std::function<void(T, S)> job_t;

public:

  void start(uint32_t max_concurrency) {
    const uint32_t num_threads = std::max(
        std::thread::hardware_concurrency()/16,
        max_concurrency);
    for (uint32_t ii = 0; ii < num_threads; ++ii) {
      threads.emplace_back(std::thread(&ThreadPool::threadLoop, this));
    }
  }

  void queueJob(const job_t& job, const T& trace, S& oname) {
    {
      std::unique_lock<std::mutex> lock(queue_mutex);
      jobs.push(job);
      traces.push(trace);
      ofnames.push(oname);
    }
    mutex_condition.notify_one();
  }

  void stop() {
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

  bool busy() {
    bool poolbusy;
    {
      std::unique_lock<std::mutex> lock(queue_mutex);
      poolbusy = !jobs.empty();
    }
    return poolbusy;
  }

private:
  void threadLoop() {
    while (true) {
      job_t job;
      T trace;
      S oname;
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

  bool should_terminate = false;           // Tells threads to stop looking for jobs
  std::mutex queue_mutex;                  // Prevents data races to the job queue
  std::condition_variable mutex_condition; // Allows threads to wait on new jobs or termination 
  std::vector<std::thread> threads;
  std::queue<job_t> jobs;
  std::queue<T> traces;
  std::queue<S> ofnames;
};

void printLogs(trace_t trace, std::string ofname);
void printPacketLogs(std::vector<Perfetto::TracePacket> trace, FILE* ofile);

} // namespace Profiler

#endif //__THREAD_POOL_H__
