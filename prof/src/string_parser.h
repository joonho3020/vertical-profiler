#ifndef __STRING_PARSER_H__
#define __STRING_PARSER_H__

#include <vector>
#include <string>


void split(std::vector<std::string>& words, std::string& line, std::string delim);
void split(std::vector<std::string>& words, std::string& line, std::string delim = " ");

#endif //__STRING_PARSER_H__
