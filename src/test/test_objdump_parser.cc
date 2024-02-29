



#include <string>
#include <iostream>
#include <vector>
#include "../profiler/objdump_parser.h"
#include "../profiler/types.h"

using namespace profiler;

void test(std::string func, int arg_idx, objdump_parser_t* parser) {
  std::string reg_name = parser->func_args_reg(func, arg_idx);
  std::cout << "func: " << func << " arg[" << arg_idx << "]: " << reg_name << std::endl;
}

void check_func_body(std::string func, objdump_parser_t* parser) {
  std::vector<std::string> body = parser->get_func_body(func);
  for (auto l : body) {
    std::cout << l << std::endl;
  }
}

void check_func_start(std::string func, objdump_parser_t* parser) {
  std::cout << func << " start addr: 0x" << std::hex << parser->get_func_start_va(func) << std::endl;
}

void check_func_end(std::string func, objdump_parser_t* parser) {
  std::cout << func << " end addr: 0x" << std::hex << parser->get_func_end_va(func) << std::endl;
}

void check_func_exits(std::string func, objdump_parser_t* parser) {
  std::vector<addr_t> exit_points = parser->get_func_exits_va(func);
  std::cout << func << " exit addrs" << std::endl;
  for (auto addr : exit_points) {
    std::cout << std::hex << addr << std::endl;
  }
}

void check_func_csrw(std::string func, objdump_parser_t* parser) {
  addr_t csrw_pc = parser->get_func_csrw_va(func, profiler::PROF_CSR_SATP);
  std::cout << func << " csrw addr " << std::hex << csrw_pc << std::endl;
}

int main() {
  std::string test_file_path = "../test/test.dump";
  objdump_parser_t *parser = new objdump_parser_t(test_file_path);

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

  check_func_csrw("set_mm_asid", parser);


  test("alloc_bprm", 1, parser);
  test("alloc_bprm", 0, parser);
  test("do_execveat_common.isra.0", 0, parser);
  test("do_execveat_common.isra.0", 1, parser);
  test("de_thread", 0, parser);
  return 0;
}
