#ifndef __STRING_PARSER_H__
#define __STRING_PARSER_H__

#include <vector>
#include <string>


void split_by_idx(std::vector<std::string>& words, std::string& line, std::vector<int>& indices);
void split(std::vector<std::string>& words, std::string& line, char delim);
void split(std::vector<std::string>& words, std::string& line, std::vector<char> delims);
void split(std::vector<std::string>& words, std::string& line, std::vector<char> delims = {' ', 9});

#endif //__STRING_PARSER_H__
