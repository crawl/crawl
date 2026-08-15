/**
 * @file localise-util.h
 * @brief String localisation (translation) utility functions
 **/

#pragma once

#include <string>

using std::string;


// low-level translate function
string xlate(const string& s);

// low-level translate function with context
string cxlate(const string& context, const string &s);

string add_context_to_string(const string &context, const string &s);
