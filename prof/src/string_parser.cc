


#include <boost/algorithm/string.hpp>
#include <boost/range/iterator_range_core.hpp>
#include <vector>
#include <string>

#include "string_parser.h"

#include <iostream>

void split(std::vector<std::string>& words, std::string& line, std::string delim) {
  std::vector<boost::iterator_range<std::string::const_iterator>> matches;
  boost::find_all(matches, line, delim);

  if (matches.size() == 0) {
    words.emplace_back(line);
  } else {
    int prev_idx = -1;
    int len = (int)line.size();
    for (auto& m : matches) {
      int idx = m.begin() - line.begin();

      if (!(idx == prev_idx || idx == prev_idx + 1)) {
        words.emplace_back(line.substr(prev_idx + 1, idx - prev_idx - 1));
      }
      prev_idx = idx;
    }
    if (prev_idx != len - 1) {
      words.emplace_back(line.substr(prev_idx+1));
    }
  }
}
