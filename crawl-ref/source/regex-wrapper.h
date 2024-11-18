/**
 * @file  regex-wrapper.h
 * @brief Wrapper for chosen regex library (PCRE or POSIX)
 **/

#pragma once

#include <string>
using std::string;


// return the first substring that matches pattern
string regex_search(const string& s, const string& pattern);

// replace all instance of pattern with the specified replacement string
string regex_replace(const string& s, const string& pattern, const string& subst);
