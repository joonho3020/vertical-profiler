



#include <string>
#include <iostream>
#include <vector>
#include "../src/objdump_parser.h"
#include "../src/types.h"

using namespace Profiler;

void test(std::string func, int arg_idx, ObjdumpParser* parser) {
  std::string reg_name = parser->func_args_reg(func, arg_idx);
  std::cout << "func: " << func << " arg[" << arg_idx << "]: " << reg_name << std::endl;
}

void check_func_body(std::string func, ObjdumpParser* parser) {
  std::vector<std::string> body = parser->get_func_body(func);
  for (auto l : body) {
    std::cout << l << std::endl;
  }
}

void check_func_start(std::string func, ObjdumpParser* parser) {
  std::cout << func << " start addr: 0x" << std::hex << parser->get_func_start_va(func) << std::endl;
}

void check_func_end(std::string func, ObjdumpParser* parser) {
  std::cout << func << " end addr: 0x" << std::hex << parser->get_func_end_va(func) << std::endl;
}

/* void check_func_callsites(std::string caller, std::string callee, ObjdumpParser* parser) { */
/* std::vector<addr_t> addrs = parser->get_func_callsites(caller, callee); */
/* std::cout << caller << " calls " << callee << " at" << std::endl; */
/* for (addr : addrs) { */
/* std::cout << std::hex << addr << std::endl; */
/* } */
/* } */

void check_func_exits(std::string func, ObjdumpParser* parser) {
  std::vector<addr_t> exit_points = parser->get_func_exits_va(func);
  std::cout << func << " exit addrs" << std::endl;
  for (auto addr : exit_points) {
    std::cout << std::hex << addr << std::endl;
  }
}

int main() {
  std::string test_file_path = "../test/test.dump";
  ObjdumpParser *parser = new ObjdumpParser(test_file_path);

  check_func_body("alloc_bprm", parser);
  check_func_body("do_execveat_common.isra.0", parser);

  check_func_start("alloc_bprm", parser);
  check_func_end("alloc_bprm", parser);

  check_func_start("do_execveat_common.isra.0", parser);
  check_func_end("do_execveat_common.isra.0", parser);
  check_func_exits("do_execveat_common.isra.0", parser);

  check_func_exits("schedule", parser);
  check_func_exits("pick_next_task_fair", parser);
  check_func_exits("io_schedule_timeout", parser);

/* check_func_callsites("__schedule", "pick_next_task_fair", parser); */

  test("alloc_bprm", 1, parser);
  test("alloc_bprm", 0, parser);
  test("do_execveat_common.isra.0", 0, parser);
  test("do_execveat_common.isra.0", 1, parser);
  test("de_thread", 0, parser);
  return 0;
}
