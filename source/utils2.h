#pragma once
#include "common.h"
#include <sstream>
#include <iomanip>
#include <fstream>
#include <cstdlib>
#include <dirent.h>

extern u64 hex_to_u64( std::string value);
extern std::string u32_to_hex_string(u32 i);
extern std::string ToHex(const std::string& s);
extern std::string get_file_contents(const char *filename);
extern std::istream& GetLine(std::istream& is, std::string& t);
extern int mkpath(std::string s,mode_t mode);
extern char parse_hex(char c);
extern char* parse_string(const std::string & s);
extern void removeForbiddenChar(std::string* s, bool onlyCR);
