#include <string>
#include <iostream>
#include <fstream>
#include <vector>
#include <algorithm>
#include <stdio.h>
#include "file_splitter.h"
#include "string_parser.h"


FileSplitter::FileSplitter(std::string ipath) : ipath(ipath)
{
}

void FileSplitter::split_by_field(std::string opath, std::string field, std::string delim) {
  std::ifstream ifile(ipath, std::ios::binary);

  std::string line;
  std::vector<std::string> words;
  std::string prev_value = "";
  bool first = true;
  int of_idx = -1;
  std::ofstream cur_ofile;

  while (std::getline(ifile, line)) {
    split(words, line, delim);
    auto it = std::find(words.begin(), words.end(), field);
    auto nxt = std::next(it);

    // found a value
    if (it != words.end() && nxt != words.end()) {
      std::string value = *nxt;
      if (prev_value != value) {
        // close the current file
        if (cur_ofile) {
          cur_ofile.close();
          of_idx++;
        }
        // open up a new one
        cur_ofile = std::ofstream(opath + "-" + std::to_string(of_idx) + ".out", std::ios::binary);
        cur_ofile << line << "\n";
      } else {
        cur_ofile << line << "\n";
      }
      prev_value = value;
    }
    words.clear();
  }

  ifile.close();
}

void FileSplitter::write_line(std::string line) {
  std::ofstream ofile;
  ofile.open(ipath, std::ios::out | std::ios::app);
  ofile << line << "\n";
  ofile.close();
}

#ifdef TEST

int main() {
  FileSplitter *fs = new FileSplitter("test/test.txt");
  fs->write_line("name index");
  fs->write_line("name: 0 index: 0");
  fs->write_line("name: 1 index: 0");
  fs->write_line("name: 2 index: 0");
  fs->write_line("name: 3 index: 1");
  fs->write_line("name: 4 index: 1");
  fs->write_line("name: 5 index: 1");
  fs->write_line("name: 6 index: 0");
  fs->write_line("name: 7 index: 0");
  fs->write_line("name: 8 index: 0");

  fs->split_by_field("test/test", "index:", " ");
  return 0;
}

#endif

#ifdef STANDALONE
int main(int argc, char** argv) {
  if (argc != 4) {
    printf("./file_splitter <input file path> <field to split> <output file path>\n");
    exit(0);
  }

  std::string ipath = argv[1];
  std::string field = argv[2];
  std::string opath = argv[3];

  FileSplitter *fs = new FileSplitter(ipath);
  fs->split_by_field(opath, field, " ");

  return 0;
}
#endif
