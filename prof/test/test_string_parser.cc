



#include <vector>
#include <string>
#include <iostream>
#include "../lib/string_parser.h"


void test(std::vector<std::string>& words, std::string line) {
  words.clear();
  split(words, line);
  for (auto w : words) {
    std::cout << w << std::endl;;
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

  return 0;
}
