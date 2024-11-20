/**
 * @file  regex-wrapper.h
 * @brief Wrapper for chosen regex library (PCRE or POSIX)
 **/

#pragma once

#include <string>
using std::string;

/*
** Return whether the string matches the pattern.
** If mpos and mlength are not null, the position and length of the first match will be returned there.
** Unlike some regex match functions, the whole string does not have to match the pattern, only a substring.
** To match the whole string, start the pattern with ^ and end it with $.
*/
bool regexp_match(const string& s, const string& pattern,
                  size_t* mpos = nullptr, size_t* mlength = nullptr);

// return the first substring that matches pattern
string regexp_search(const string& s, const string& pattern);

// replace all instance of pattern with the specified replacement string
string regexp_replace(const string& s, const string& pattern, const string& subst);
