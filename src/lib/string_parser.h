#ifndef __STRING_PARSER_H__
#define __STRING_PARSER_H__

#include <vector>
#include <string>
#include <string_view>
#include <inttypes.h>


void split_by_idx(std::vector<std::string>& words, std::string& line, std::vector<int>& indices);
void split(std::vector<std::string>& words, std::string& line, char delim);
void split(std::vector<std::string>& words, std::string& line, std::vector<char> delims);
void split(std::vector<std::string>& words, std::string& line, std::vector<char> delims = {' ', 9});

std::vector<std::string> fast_split(const std::string_view s, const char delim, const size_t maxFields);
std::vector<std::string> fast_split(const std::string_view s, const char delim, const size_t maxFields = 0);

bool strtobool_fast(const char* cstr);
uint64_t strtoull_fast_dec(const char *s);
uint64_t strtoull_fast_hex(const char *s);

#endif //__STRING_PARSER_H__
