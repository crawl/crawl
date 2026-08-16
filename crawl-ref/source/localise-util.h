/**
 * @file localise-util.h
 * @brief String localisation (translation) utility functions
 **/

#pragma once

#include <string>
#include <vector>

using std::string;
using std::vector;


// low-level translate function
string xlate(const string& s);

// low-level translate function with context
string cxlate(const string& context, const string &s);

string add_context_to_string(const string &context, const string &s);
string strip_context_from_string(const string &s);

vector<string> extract_params(const string& s);

// escape any characters that have a special meaning in regex
string escape_regex_specials(const string& s);

// length of string excluding any @foo@ parameters
size_t length_excl_params(const string &s);
