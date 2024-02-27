
#include <string>
#include <iostream>
#include <fstream>
#include <vector>
#include <map>
#include <stdio.h>

#include "stack_unwinder.h"
#include "../lib/string_parser.h"
#include "types.h"


std::string spiketrace_filename(uint64_t idx) {
  std::string sfx;
  if (idx < 10) {
    sfx = "000000" + std::to_string(idx);
  } else if (idx < 100) {
    sfx = "00000" + std::to_string(idx);
  } else if (idx < 1000) {
    sfx = "0000" + std::to_string(idx);
  } else if (idx < 10000) {
    sfx = "000" + std::to_string(idx);
  } else if (idx < 100000) {
    sfx = "00" + std::to_string(idx);
  } else if (idx < 1000000) {
    sfx = "0" + std::to_string(idx);
  } else {
    sfx = std::to_string(idx);
  }
  return ("SPIKETRACE-" + sfx);
}

bool user_space_addr(uint64_t va) {
  uint64_t va_hi = va >> 32;
  return ((va_hi & 0xffffffff) == 0);
}

int main() {

  printf("Start stack unwinding\n");

  FILE *callstack = NULL;
  callstack = fopen("SPIKE-CALLSTACK", "w");
  if (callstack == NULL) {
    fprintf(stderr, "Unable to open callstack file SPIKE-CALLSTACK\n");
    exit(-1);
  }

  std::vector<std::pair<std::string, std::string>> dwarf_paths;
  dwarf_paths.push_back({KERNEL,            "../../test-io/test-binaries/linux-workloads/linux-workloads-bin-dwarf"});
  dwarf_paths.push_back({"hello.linux.riscv", "../../test-io/test-binaries/linux-workloads/overlay/root/hello.linux.riscv"});

  std::map<uint64_t, std::string> asid_to_bin;

  std::ifstream a2b_file = std::ifstream("ASID-MAPPING", std::ios::binary);
  if (!a2b_file) {
    std::cerr << "ASID-MAPPING" << " does not exist" << std::endl;
    exit(1);
  }

  std::string line;
  std::vector<std::string> words;
  std::string::size_type sz = 0;

  while (std::getline(a2b_file, line)) {
    words.clear();
    split(words, line);
    uint64_t k = std::stoull(words[0], &sz, 10);

    auto x = words[1];
    words.clear();
    split(words, x, '/');

    asid_to_bin[k] = words.back();
  }

  const uint64_t trace_idx = 20000;

  profiler::stack_unwinder_t *stack_unwinder = new profiler::stack_unwinder_t(
      dwarf_paths,
      callstack);

  uint64_t cycle = 0;

  for (uint64_t i = 0; i < trace_idx; i++) {
    printf("SPIKETRACE-%" PRIu64 "\n", i);
    std::string path = "./out/" + spiketrace_filename(i);
    std::ifstream spike_trace = std::ifstream(path, std::ios::binary);
    if (!spike_trace) {
      std::cerr << path << " does not exist" << std::endl;
      exit(1);
    }


    while (std::getline(spike_trace, line)) {
/* std::cout << line << std::endl; */
      words.clear();
      split(words, line);
      uint64_t addr = std::stoull(words[0], &sz, 16);
      uint64_t asid = std::stoull(words[1], &sz, 10);
/* uint64_t prv  = std::stoull(words[2], &sz, 10); */
/* std::string prev_prv = words[3]; // TODO don't need? */

      if (user_space_addr(addr)) {
        std::string binpath = asid_to_bin[asid];
        std::vector<std::string> subpath;
        split(subpath, binpath, '/');
        stack_unwinder->add_instruction(addr, cycle, subpath.back());
      } else {
        stack_unwinder->add_instruction(addr, cycle, KERNEL);
      }
      cycle++;
    }
  }
}
