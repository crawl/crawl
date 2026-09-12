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

string localise(const string &s);

formatted_string localise(const formatted_string& fs);
