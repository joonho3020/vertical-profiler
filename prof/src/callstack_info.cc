#include "callstack_info.h"
#include <string>

namespace Profiler {

CallStackInfo::CallStackInfo(std::string func, std::string binary)
  : func(func), binary(binary)
{
}

std::string CallStackInfo::fn() {
  return func;
}

std::string CallStackInfo::bin() {
  return binary;
}



} // namespace Profiler
