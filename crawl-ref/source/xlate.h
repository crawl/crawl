/**
 * @file  xlate.h
 * @brief Low-level translation routines.
 **/

#pragma once

#include <stddef.h>
#include <string>
using std::string;

// translate with context
//
// context = the context in which the text is being used (optional, default=none)
//  (for disambiguating same English text used in different contexts which may require different translations)
//  if no translation is found in the specified context, will look for translation at global (no) context
// msgid = English text to be translated
// fallback_en = fall back to English if no translation is found?
string cxlate(const string &context, const string &msgid, bool fallback_en = true);

// translate with context and number
// returns the plural form corresponding to number
//
// context = the context in which the text is being used (optional, default=none)
//  (for disambiguating same English text used in different contexts which may require different translations)
//  if no translation is found in the specified context, will look for translation at global (no) context
// msgid1 = English singular text
// msgid2 = English plural text
// n = the count of whatever it is
string cnxlate(const string &context,
        const string &msgid1, const string &msgid2, unsigned long n);

// translate with no context
static inline string xlate(const string &msgid, bool fallback_en = true)
{
    return cxlate("", msgid, fallback_en);
}

// translate with number (no context)
static inline string nxlate(const string &msgid1, const string &msgid2, unsigned long n)
{
    return cnxlate("", msgid1, msgid2, n);
}
