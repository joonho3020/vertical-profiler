
#include <string>
#include <vector>
#include <cassert>
#include "objdump_parser.h"
#include "string_parser.h"

ObjdumpParser::ObjdumpParser(std::string objdump_path)
  : objdump_path(objdump_path)
{
  objdump_file = std::ifstream(objdump_path, std::ios::binary);
}

void ObjdumpParser::get_func_body(std::string func, std::vector<std::string>& body) {
  std::string line;
  std::vector<std::string> words;

  bool in_func = false;

  while (std::getline(objdump_file, line)) {
    split(words, line, " ");

    if (in_func && (int)words.size() == 0) {
      return;
    } else if (in_func) {
      body.emplace_back(line);
    } else if ((int)words.size() == 2 && words[1].find(func) != std::string::npos) {
      in_func = true;
    }
    words.clear();
  }
}


std::string ObjdumpParser::func_args_reg(std::string func, int arg_idx) {
  assert((void("RISC-V can pass up to 8 arguments via regs"), arg_idx <= 7));

  std::vector<std::string> body;
  get_func_body(func, body);

  std::string r = "a" + std::to_string(arg_idx);
  std::vector<std::string> words;
  for (auto l : body) {
    split(words, l, " ");

    // found first instance of "r" used
    if ((int)words.size() >= 4 && words[3].find(r) != std::string::npos) {
      if (words[2].compare("mv") == 0) {
        std::vector<std::string> ops;
        split(ops, words[3], ",");
        assert((int)ops.size() == 2);
        return ops[0];
      } else {
        return r;
      }
      break;
    }
    words.clear();
  }

  return r;
}
