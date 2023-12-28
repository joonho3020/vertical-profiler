#ifndef __CALLSTACK_INFO_H__
#define __CALLSTACK_INFO_H__

#include <string>

namespace Profiler {

struct CallStackInfo {
public:
  CallStackInfo(std::string func, std::string binary);
  std::string fn();
  std::string bin();


private:
  std::string func;
  std::string binary;
};


} // namespace Profiler


#endif //__CALLSTACK_INFO_H__
