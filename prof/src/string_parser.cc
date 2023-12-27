


#include <vector>
#include <string>
#include <iostream>
#include <algorithm>

#include "string_parser.h"


namespace Profiler {

// TODO : Perf opts???

void split_by_idx(std::vector<std::string>& words, std::string& line, std::vector<int>& indices) {
  if (indices.size() == 0) {
    words.emplace_back(line);
  } else {
    int prev_idx = -1;
    int len = (int)line.size();
    for (auto& idx : indices) {
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

void split(std::vector<std::string>& words, std::string& line, char delim) {
  std::vector<int> indices;
  int idx = 0;
  for (auto& c : line) {
    if (c == delim) {
      indices.push_back(idx);
    }
    ++idx;
  }
  split_by_idx(words, line, indices);
}

void split(std::vector<std::string>& words, std::string& line, std::vector<char> delims) {
  std::vector<int> indices;
  for (auto& d: delims) {
    int idx = 0;
    for (auto& c : line) {
      if (c == d) {
        indices.push_back(idx);
      }
      ++idx;
    }
  }
  std::sort(indices.begin(), indices.end());
  split_by_idx(words, line, indices);
}

} // namespace Profiler
