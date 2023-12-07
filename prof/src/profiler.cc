
#include <string>
#include <vector>
#include <map>

#include "types.h"
#include "objdump_parser.h"
#include "profiler.h"


profiler_t::profiler_t(std::vector<std::string> objdump_paths) {
  for (auto& p: objdump_paths) {
    objdumps.insert({p, new ObjdumpParser(p)});
  }
}

profiler_t::~profiler_t() {
  for (auto& o : objdumps) {
    delete o.second;
  }
  objdumps.clear();
}

std::string profiler_t::find_user_space_binary(addr_t inst_va) {
}
