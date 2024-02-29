#ifndef __OBJDUMP_PARSER_H__
#define __OBJDUMP_PARSER_H__

#include <fstream>
#include <string>
#include <vector>
#include <map>
#include "types.h"


namespace profiler {


class instruction_t {
public:
  instruction_t(addr_t addr, std::string fn) : addr(addr), fn(fn) {}

  addr_t addr;
  std::string fn;
};

class objdump_parser_t {
public:
  objdump_parser_t(std::string objdump_path);

  std::string func_args_reg(std::string func, int arg_idx);
  std::string func_ret_reg(std::string func);
  std::vector<std::string>& get_func_body(std::string func);

  addr_t get_func_start_va(std::string func);
  addr_t get_func_end_va(std::string func);
  std::vector<addr_t> get_func_exits_va(std::string func);

  addr_t get_func_csrw_va(std::string func, std::string csr);

private:
  std::string objdump_path;
  std::map<std::string, std::vector<std::string>> func_bodies;
  std::map<std::string, addr_t> func_start_va;
  std::map<std::string, addr_t> func_end_va;
  std::vector<instruction_t*> text;
};

} // namespace profiler

#endif //__OBJDUMP_PARSER_H__
