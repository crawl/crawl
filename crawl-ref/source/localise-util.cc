/**
 * @file localise-util.cc
 * @brief String localisation (translation) utility functions
 **/

#include "AppHdr.h"
#include "database.h"
#include "localise-util.h"
#include "stringutil.h"

string add_context_to_string(const string &context, const string &s)
{
    return "{" + context + "}" + s;
}

// low-level translate function
string xlate(const string &s)
{
    return getTranslatedString(s);
}

// low-level translate function with context
string cxlate(const string &context, const string &s)
{
    string result;

    // first try with context
    result = getTranslatedString(add_context_to_string(context, s));
    if (!result.empty())
        return result;

    // now try without
    return getTranslatedString(s);
}
