
#include <stdio.h>
#include <iostream>
#include "trace_tracker.h"

size_t split(const std::string &txt, std::vector<std::string> &strs, char ch) {
  size_t pos = txt.find( ch );
  size_t initialPos = 0;
  strs.clear();

  // Decompose statement
  while( pos != std::string::npos ) {
    strs.push_back( txt.substr( initialPos, pos - initialPos ) );
    initialPos = pos + 1;

    pos = txt.find( ch, initialPos );
  }

  // Add the last one
  strs.push_back( txt.substr( initialPos, std::min( pos, txt.size() ) - initialPos + 1 ) );

  return strs.size();
}

int main(int argc, char** argv) {
  if (argc != 3) {
    printf("./main <ABS PATH TO TRACEFILE> <ABS PATH TO DWARFFILE>\n");
    exit(1);
  }
  std::string tracefile = argv[1];
  std::string dwarffile = argv[2];

  std::cout << "tracefile: " << tracefile << "\n"
    << "dwarffile: " << dwarffile << std::endl;

  std::ifstream is(tracefile);
  std::string line;

  TraceTracker *t =
    new TraceTracker(dwarffile, stdout);
  std::vector<std::string> words;
  while (getline(is, line)) {
    split(line, words, ' ');
    if ((int)words.size() != 4) {
      continue;
    }

    std::string addr_str = words[3];
    std::string cycle_str = words[1];
    uint64_t addr_offset = 0xffffffff00000000LL;
    uint64_t addr = (uint64_t)strtoull(addr_str.c_str(), NULL, 16) | addr_offset;
    uint64_t cycle = (uint64_t)strtoull(cycle_str.c_str(), NULL, 16);
/* std::cout << addr_str << " " <<  std::hex << addr << " " << std::dec << cycle << std::endl; */
    t->addInstruction(addr, cycle);
    words.clear();
  }
}
