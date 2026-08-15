/**
 * @file localise-util.cc
 * @brief String localisation (translation) utility functions
 **/

#include "AppHdr.h"
#include "database.h"
#include "localise-util.h"
#include "stringutil.h"

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
    if (!context.empty()) {
        result = getTranslatedString(add_context_to_string(context, s));
        if (!result.empty())
            return result;
    }

    // fall back to the default translation
    return getTranslatedString(s);
}

string add_context_to_string(const string &context, const string &s)
{
    return "{" + context + "}" + s;
}

string strip_context_from_string(const string &s)
{
    if (s.empty() || s[0] != '{')
        return s;

    size_t end = s.find('}');
    if (end == string::npos)
        return s;

    return s.substr(end + 1);
}

// extract @foo@ parameters from string
vector<string> extract_params(const string& s)
{
    vector<string> results;

    size_t start = s.find('@', 0);
    while (start != string::npos) {
        size_t end = s.find('@', start + 1);
        if (end == string::npos)
            break;
        results.push_back(s.substr(start, end - start + 1));
        start = s.find('@', end + 1);
    }

    return results;
}

string escape_regex_specials(const string& s)
{
    const string specials = "\\^$.|?*+()[]{}";
    string result;
    for (char c: s)
    {
        if (specials.find(c) != string::npos)
            result += '\\';
        result += c;
    }
    return result;
}

size_t length_excl_params(const string &s)
{
    size_t len = 0;
    bool in_param = false;
    for (const char c: s)
    {
        if (c == '@')
        {
            in_param = !in_param;
            continue;
        }
        if (!in_param)
            len++;
    }
    return len;
}