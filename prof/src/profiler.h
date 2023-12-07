#ifndef __PROFILER_H__
#define __PROFILER_H__


#include <string>
#include <vector>
#include <map>

#include "types.h"
#include "objdump_parser.h"

class profiler_t {
public:
  profiler_t::profiler_t(std::vector<std::string> objdump_paths);
  profiler_t::~profiler_t();

  std::string find_user_space_binary(addr_t inst_va);

private:
  std::map<std::string, ObjdumpParser*> objdumps;
  std::map<reg_t, std::string> asid_to_bin;
};

#endif //__PROFILER_H__
