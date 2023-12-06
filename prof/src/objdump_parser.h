#ifndef __OBJDUMP_PARSER_H__
#define __OBJDUMP_PARSER_H__

#include <fstream>
#include <string>
#include <vector>

class ObjdumpParser {

public:
  ObjdumpParser(std::string objdump_path);

  void get_func_body(std::string func, std::vector<std::string>& body);

  std::string func_args_reg(std::string func, int arg_idx);



private:
  std::string objdump_path;
  std::ifstream objdump_file;
};

#endif //__OBJDUMP_PARSER_H__
