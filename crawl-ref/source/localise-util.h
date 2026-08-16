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

string add_context(const string &context, const string &s);
string strip_context(const string &s);

vector<string> extract_params(const string& s);

// escape any characters that have a special meaning in regex
string escape_regex_specials(const string& s);

// length of string excluding any @foo@ parameters
size_t length_excl_params(const string &s);

// is this a string representation of an integer?
// (1 or more digits with an optional sign in front)
bool is_integer_string(const string &s);

// is this a string representation of a floating point number?
bool is_float_string(const string &s);
