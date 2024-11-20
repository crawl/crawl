#include "AppHdr.h"

#include "pattern.h"
#include "stringutil.h"
#include "regex-wrapper.h"

string pattern_match::annotate_string(const string &color) const
{
    string ret(text);

    if (*this && start < end)
    {
        ret.insert(end, make_stringf("</%s>", color.c_str()));
        ret.insert(start, make_stringf("<%s>", color.c_str()));
    }

    return ret;
}

text_pattern::~text_pattern()
{
}

const text_pattern &text_pattern::operator= (const text_pattern &tp)
{
    if (this == &tp)
        return tp;

    pattern = tp.pattern;
    validated    = tp.validated;
    isvalid      = tp.isvalid;
    ignore_case  = tp.ignore_case;
    return *this;
}

const text_pattern &text_pattern::operator= (const string &spattern)
{
    if (pattern == spattern)
        return *this;

    pattern = spattern;
    validated = false;
    isvalid = true;
    // We don't change ignore_case
    return *this;
}

bool text_pattern::operator== (const text_pattern &tp) const
{
    if (this == &tp)
        return true;

    return pattern == tp.pattern && ignore_case == tp.ignore_case;
}

bool text_pattern::valid() const
{
    if (!validated)
    {
        isvalid = regexp_valid(pattern);
        validated = true;
    }
    return isvalid;
}

bool text_pattern::matches(const string& s) const
{
    return regexp_match(s, pattern, ignore_case);
}

pattern_match text_pattern::match_location(const string& s) const
{
    size_t mpos, mlength;

    if (regexp_match(s, pattern, ignore_case, &mpos, &mlength))
        return pattern_match::succeeded(s, mpos, mpos + mlength);
    else
        return pattern_match::failed(string(s));
}

const plaintext_pattern &plaintext_pattern::operator= (const string &spattern)
{
    if (pattern == spattern)
        return *this;

    pattern = spattern;
    // We don't change ignore_case

    return *this;
}

bool plaintext_pattern::operator== (const plaintext_pattern &tp) const
{
    if (this == &tp)
        return true;

    return pattern == tp.pattern && ignore_case == tp.ignore_case;
}

bool plaintext_pattern::matches(const string &s) const
{
    string needle = ignore_case ? lowercase_string(pattern) : pattern;
    string haystack = ignore_case ? lowercase_string(s) : s;
    return haystack.find(needle) != string::npos;
}

pattern_match plaintext_pattern::match_location(const string &s) const
{
    string needle = ignore_case ? lowercase_string(pattern) : pattern;
    string haystack = ignore_case ? lowercase_string(s) : s;
    size_t pos;
    if ((pos = haystack.find(needle)) != string::npos)
        return pattern_match::succeeded(s, pos, pos + pattern.length());
    else
        return pattern_match::failed(s);
}
