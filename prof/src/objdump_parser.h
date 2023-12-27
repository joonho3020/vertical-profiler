#ifndef __OBJDUMP_PARSER_H__
#define __OBJDUMP_PARSER_H__

#include <fstream>
#include <string>
#include <vector>
#include <map>
#include "types.h"


namespace Profiler {
class ObjdumpParser {

public:
  ObjdumpParser(std::string objdump_path);

  std::string func_args_reg(std::string func, int arg_idx);
  std::vector<std::string>& get_func_body(std::string func);
  addr_t get_func_start_va(std::string func);

private:
  std::string objdump_path;
  std::map<std::string, std::vector<std::string>> func_bodies;
  std::map<std::string, addr_t> func_start_va;
};

} // namespace Profiler

#endif //__OBJDUMP_PARSER_H__
