#ifndef __FILE_SPLITTER_H__
#define __FILE_SPLITTER_H__


#include <iostream>
#include <string>

namespace Profiler {

class FileSplitter {
public:
  FileSplitter(std::string ipath);

  /*
   * Splits each line by "delim", searches for "field" and gets the value of "field".
   * Splits the file whenever the value changes.
   */
  void split_by_field(std::string opath, std::string field, char delim);

  void write_line(std::string line);

private:
  std::string ipath;
};

} // namespace Profiler


#endif // __FILE_SPLITTER_H__
