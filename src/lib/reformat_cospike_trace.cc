#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <fstream>
#include "string_parser.h"

int main(int argc, char** argv) {
  std::ios_base::sync_with_stdio(false);
  std::cin.tie(NULL);

  if (argc < 3) {
    printf("Usage ./reformat_cospike_trace <path to trace> <path to outfile>\n");
    exit(1);
  }

  FILE* out = fopen(argv[2], "w");
  if (out == NULL) {
    printf("failed to open %s\n", argv[2]);
    exit(1);
  }

  printf("processing file %s\n", argv[1]);
  std::ifstream trace_file = std::ifstream(std::string(argv[1]), std::ios::binary);
  std::string line;
  while (std::getline(trace_file, line)) {
    std::vector<std::string> words = fast_split(line, ' ', 10);

    bool val = strtobool_fast(words[1].c_str());
    uint64_t time   = strtoull_fast_dec(words[0].c_str());
    uint64_t pc     = strtoull_fast_hex(words[2].substr(2).c_str());
    bool     except = strtobool_fast(words[4].c_str());
    bool     intrpt = strtobool_fast(words[5].c_str());
    int      cause  = strtoull_fast_dec(words[6].c_str());
    bool     has_w  = strtobool_fast(words[7].c_str());
    uint64_t wdata  = strtoull_fast_hex(words[8].substr(2).c_str());
/* uint64_t insn   = strtoull_fast_hex(words[3].substr(2).c_str()); */
/* int      priv   = strtoull_fast_dec(words[9].c_str()); */

    fprintf(out, "%d %d %d %d %d %lu %lx %lx\n",
                  val, except, intrpt, has_w, cause, time, pc, wdata);
  }
  fflush(out);
  fclose(out);

  return 0;
}
