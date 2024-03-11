



#include <vector>
#include <string>
#include <inttypes.h>
#include <iostream>
#include <assert.h>
#include "../lib/string_parser.h"


void test(std::vector<std::string>& words, std::string line) {
  words.clear();
  split(words, line);
  for (auto w : words) {
    std::cout << w << std::endl;;
  }
}

void test_stroull_fast_hex(std::string input, uint64_t value) {
  uint64_t out = strtoull_fast_hex(input.c_str());
  if (out != value) {
    printf("Failed out: %" PRIu64 " != expect: %" PRIu64 "\n",
        out, value);
    assert(false);
  }
}

int main() {
  std::vector<std::string> words;

  test(words, " hello world");
  test(words, "  hello world");
  test(words, " hello   world");
  test(words, "  hello   world ");
  test(words, "  hello   world  ");
  test(words, "hello   world  ");
  test(words, "hello   world");
  test(words, "helloworld");

  test(words, " hello world bye ");
  test(words, "hello world bye");

  test(words, "ffffffff8019db02:       b7d5                    j       ffffffff8019dae6 <do_execveat_common.isra.0+0x17e>");

  words.clear();

  std::string test_str = "ffffffff8019db02:  b7d5                    j       ffffffff8019dae6 <do_execveat_common.isra.0+0x17e>";
  split(words, test_str);

  test_stroull_fast_hex("800000000014112d", 0x800000000014112d);
  test_stroull_fast_hex("301022f3",         0x301022f3);

  return 0;
}
