/**
 * @file  xlate.h
 * @brief Low-level translation routines.
 **/

#pragma once

#include <string>
using std::string;

// translate with context
//
// context = the context in which the text is being used (optional, default=none)
//  (for disambiguating same English text used in different contexts which may require different translations)
//  if no translation is found in the specified context, will look for translation at global (no) context
// text_en = English text to be translated
// fallback_en = fall back to English if no translation is found?
string cxlate(const string &context, const string &text_en, bool fallback_en = true);

// translate with context and number
// returns the plural form corresponding to number
//
// context = the context in which the text is being used (optional, default=none)
//  (for disambiguating same English text used in different contexts which may require different translations)
//  if no translation is found in the specified context, will look for translation at global (no) context
// singular_en = English singular text
// plural_en = English plural text
// n = the count of whatever it is
// fallback_en = fall back to English if no translation is found?
string cnxlate(const string &context,
               const string &singular_en, const string &plural_en,
               unsigned long n, bool fallback_en = true);

// translate with no context
static inline string xlate(const string &text_en, bool fallback_en = true)
{
    return cxlate("", text_en, fallback_en);
}

// translate with number (no context)
static inline string nxlate(const string &singular_en, const string &plural_en,
                            unsigned long n, bool fallback_en = true)
{
    return cnxlate("", singular_en, plural_en, n, fallback_en);
}
