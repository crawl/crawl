/**
 * @file localise.h
 * @brief String localisation (translation)
 **/

#pragma once

#include <string>

#include "format.h"

using std::string;

void init_localisation();

// not mandatory to call this, except in regression tests, where we want to switch languages
void shutdown_localisation();

// Is localisation active?
bool localisation_active();

// localise string
string localise(const string &s);

// convenience function, equivalent to localise(make_stringf(format, ...))
string localisef(const char* format, ...);

// localise formatted string
formatted_string localise(const formatted_string& fs);
