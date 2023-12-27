
#include <string>
#include <iostream>
#include <fstream>
#include <vector>
#include <algorithm>
#include <stdio.h>
#include "file_splitter.h"
#include "string_parser.h"


using namespace Profiler;

int main(int argc, char** argv) {
  if (argc != 4) {
    printf("./file_splitter <input file path> <field to split> <output file path>\n");
    exit(0);
  }

  std::string ipath = argv[1];
  std::string field = argv[2];
  std::string opath = argv[3];

  FileSplitter *fs = new FileSplitter(ipath);
  fs->split_by_field(opath, field, ' ');

  return 0;
}
