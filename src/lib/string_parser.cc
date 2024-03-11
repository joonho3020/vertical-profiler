


#include <vector>
#include <string>
#include <iostream>
#include <algorithm>
#include <string_view>

#include "string_parser.h"


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

std::vector<std::string> fast_split(const std::string_view s, const char delim, const size_t maxFields) {
  std::vector<std::string> elems;
  size_t start{};
  size_t end{};
  size_t numFieldsParsed{};

  do {
    end = s.find_first_of(delim, start);
    elems.emplace_back(s.substr(start, end - start));
    start = end + 1;
  } while (end != std::string::npos && (maxFields == 0 || ++numFieldsParsed < maxFields));
  return elems;
}

bool strtobool_fast(const char* cstr) {
  return (*cstr == '1');
}

uint64_t strtoull_fast_dec(const char *s) {
  uint64_t sum = 0;
  while (*s) {
    sum = (sum * 10) + (*s++ - '0');
  }
  return sum;
}

uint64_t strtoull_fast_hex(const char *s) {
  uint64_t sum = 0;
  while (*s) {
    uint64_t d;
    if (*s >= 97)
      d = (*s - 87);
    else if (*s >= 65)
      d = (*s - 55);
    else
      d = (*s - '0');

    sum = (sum * 16) + d;
    s++;
  }
  return sum;
}
