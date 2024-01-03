
#include <string>
#include <vector>
#include <cassert>
#include <iostream>
#include <stdlib.h>
#include "objdump_parser.h"
#include "string_parser.h"

namespace Profiler {

ObjdumpParser::ObjdumpParser(std::string objdump_path)
  : objdump_path(objdump_path)
{
  std::string line;
  std::vector<std::string> words;

  bool in_func = false;
  std::ifstream objdump_file = std::ifstream(objdump_path, std::ios::binary);
  std::string name;
  size_t len;
  char* end = nullptr;

  if (!objdump_file) {
    std::cerr << "objdump file does not exist: " << objdump_path << std::endl;
    exit(1);
  }

  while (std::getline(objdump_file, line)) {
    split(words, line);

    if (in_func && (int)words.size() == 1) {
      in_func = false;

      auto &cur_body = func_bodies.find(name)->second;
      auto prev_line = cur_body.back();

      words.clear();
      split(words, prev_line);
      func_end_va.insert({name, std::strtoul(words[0].c_str(), &end, 16)});
    } else if (in_func) {
      auto& cur_body = func_bodies.find(name)->second;
      cur_body.emplace_back(line);
    } else if ((int)words.size() == 2 && words[1].find(">:") != std::string::npos) {
      in_func = true;
      len = words[1].size() - 3;
      name = words[1].substr(1, len);

      func_bodies.insert({name, {line}});
      func_start_va.insert({name, std::strtoul(words[0].c_str(), &end, 16)});
    }
    words.clear();
  }
  objdump_file.close();
}

// TODO : Don't want to repeat opening up a file, read it, and parse it.
std::vector<std::string>& ObjdumpParser::get_func_body(std::string func) {
  auto it = func_bodies.find(func);
  if (it == func_bodies.end()) {
    assert((void("Could not find function body in the objdump"), false));
  }
  return it->second;
}


std::string ObjdumpParser::func_args_reg(std::string func, int arg_idx) {
  assert((void("RISC-V can pass up to 8 arguments via regs"), arg_idx <= 7));

  std::vector<std::string> body = get_func_body(func);
  std::string r = "a" + std::to_string(arg_idx);
  std::vector<std::string> words;

  for (auto l : body) {
    split(words, l);

    // found first instance of "r" used
    if ((int)words.size() >= 4 && words[3].find(r) != std::string::npos) {
      if (words[2].compare("mv") == 0) {
        std::vector<std::string> ops;
        split(ops, words[3], ',');
        assert((int)ops.size() == 2);
        if (ops[0].compare(r) == 0)
          return ops[0];
        else
          return ops[1];
      } else {
        return r;
      }
      break;
    }
    words.clear();
  }

  return r;
}


addr_t ObjdumpParser::get_func_start_va(std::string func) {
  auto it = func_start_va.find(func);
  if (it == func_start_va.end()) {
    assert((void("Could not find function starting name in the objdump"), false));
  }
  return it->second;
}

addr_t ObjdumpParser::get_func_end_va(std::string func) {
  auto it = func_end_va.find(func);
  if (it == func_end_va.end()) {
    assert((void("Could not find function ending name in the objdump"), false));
  }
  return it->second;
}


std::vector<addr_t> ObjdumpParser::get_func_exits(std::string func) {
  std::vector<std::string>& body = get_func_body(func);
  std::vector<std::string> words;

  std::vector<addr_t> exit_points;
  char* end = nullptr;

  for (auto& l : body) {
    split(words, l);
    if ((int)words.size() < 3)
      continue;

    if (words[2].compare("ret") == 0)
      exit_points.push_back(std::strtoul(words[0].c_str(), &end, 16));

    words.clear();
  }
  return exit_points;
}

} // namespace Profiler
