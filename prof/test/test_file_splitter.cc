


#include "../src/file_splitter.h"

using namespace Profiler;

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

  fs->split_by_field("test/test", "index:", ' ');
  return 0;
}
