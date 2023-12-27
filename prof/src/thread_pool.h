#ifndef __THREAD_POOL_H__
#define __THREAD_POOL_H__

/* https://stackoverflow.com/questions/15752659/thread-pooling-in-c11 */

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

namespace Profiler {

typedef std::function<void(trace_t, std::string)> job_t;

class ThreadPool {
public:
  void start();
  void queueJob(const job_t& job, const trace_t& trace, std::string& oname);
  void stop();
  bool busy();

private:
  void threadLoop();

  bool should_terminate = false;           // Tells threads to stop looking for jobs
  std::mutex queue_mutex;                  // Prevents data races to the job queue
  std::condition_variable mutex_condition; // Allows threads to wait on new jobs or termination 
  std::vector<std::thread> threads;
  std::queue<job_t> jobs;
  std::queue<trace_t> traces;
  std::queue<std::string> ofnames;
};

void printLogs(trace_t trace, std::string ofname);

} // namespace Profiler

#endif //__THREAD_POOL_H__
