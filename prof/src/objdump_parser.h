#ifndef __OBJDUMP_PARSER_H__
#define __OBJDUMP_PARSER_H__

#include <fstream>
#include <string>
#include <vector>

class ObjdumpParser {

public:
  ObjdumpParser(std::string objdump_path);

  std::string func_args_reg(std::string func, int arg_idx);
  void get_func_body(std::string func, std::vector<std::string>& body);

private:
  std::string objdump_path;
};

#endif //__OBJDUMP_PARSER_H__
