/**
 * @file localise.h
 * @brief String localisation (translation)
 **/

#pragma once

#include <string>

using std::string;

void init_localisation();

// Is localisation active?
bool localisation_active();

string localise(const string &s);
