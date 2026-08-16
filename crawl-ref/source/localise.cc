/**
 * @file localise.cc
 * @brief String localisation (translation)
 **/

#include "AppHdr.h"
#include "database.h"
#include "localise.h"
#include "localise-util.h"
#include "pattern.h"
#include "options.h"
#include "stringutil.h"

#include <algorithm>
#include <string>
#include <vector>
//#include <pair>

using namespace std;

//#define debuglog(...) {fprintf(stderr, "DEBUG: %s: ", __FUNCTION__); fprintf (stderr, __VA_ARGS__); fprintf(stderr, "\n");}

#ifndef debuglog
#define debuglog(...) {}
#endif

static string _localise_string(const string& context, const string& s);
static string _localise_param_string(const string& context, const string& s);

static vector<pair<text_pattern, string>> _patterns;
static bool _initialised = false;

void init_localisation()
{
    if (_initialised || Options.language == lang_t::EN)
        return;
    _initialised = true;

    text_pattern patt("^.$");
    if (!patt.matches("の"))
        fprintf(stderr, "WARNING: Regex is not UTF-8 aware\n");

    // get all translation keys which contain parameters
    vector<string> keys = getTranslatedKeysByRegex("@[^@]+@");
    debuglog("%ld parameterised translation strings\n", (long)keys.size());

    // sort with longest strings first because we want to match the longest string possible
    std::sort(keys.begin(), keys.end(), [](const string& a, const string& b) {
        return length_excl_params(a) > length_excl_params(b);
    });

    // convert paramaterised string to regex pattern
    text_pattern num_arg_patt("@num[^@]*@");
    text_pattern str_arg_patt("@[^@]+@");
    for (const string& key: keys)
    {
        string pattern = key;
        // escape characters that mean something to regex
        pattern = escape_regex_specials(pattern);
        pattern = num_arg_patt.replace(pattern, "([\\+|\\-]?[0-9]+)");
        pattern = str_arg_patt.replace(pattern, "(.*?)");
        pattern = "^" + pattern + "$";
        _patterns.emplace_back(make_pair(text_pattern(pattern), key));
    }
    debuglog("Localisation initialised");
}

static int _get_matching_pattern_index(const string& s)
{
    if (s.empty())
        return -1;

    for (size_t i = 0; i < _patterns.size(); i++)
    {
        // do some quick tests first to reduce the amount of regex
        // (POSIX regex is slow)
        const string& param_str = _patterns[i].second;
        if (param_str.empty())
            continue;
        if (isaalpha(s[0]) && param_str[0] != s[0])
            continue;
        if (length_excl_params(s) < length_excl_params(param_str))
            continue;

        if (_patterns[i].first.matches(s))
            return (int)i;
    }

    return -1;
}

// localise parameterised string
static string _localise_param_string(const string& context, const string& s)
{
    // try to find matching pattern
    int match_idx = _get_matching_pattern_index(s);
    if (match_idx < 0)
        return "";

    const pair<text_pattern, string>& match = _patterns[match_idx];

    debuglog("String: %s", s.c_str());
    debuglog("Pattern: %s", match.first.c_str());
    debuglog("Param string: %s", match.second.c_str());

    vector<string> param_keys = extract_params(match.second);
    vector<string> param_vals = match.first.capture(s);

    if (param_keys.size() != param_vals.size())
    {
        debuglog("ERROR: %ld keys, %ld vals\n", (long)param_keys.size(), (long)param_vals.size());
        return "";
    }

    // TODO: Handle embedded contexts

    string result = cxlate(context, match.second);
    for (size_t i = 0; i <param_keys.size(); i++)
    {
        string param_val = _localise_string("", param_vals[i]);
        result = replace_all(result, param_keys[i], param_val);
    }

    return result;
}

static string _localise_string(const string& context, const string& s)
{
    if (s.empty())
        return s;

    if (is_integer_string(s))
        return s;

    // try simple translation first
    string result = cxlate(context, s);
    if (!result.empty())
        return result;

    // check for leading/trailing whitespace
    string trimmed = trimmed_string(s);
    if (trimmed.length() != s.length())
    {
        if (trimmed.empty())
        {
            // all whitespace
            return s;
        }
        result = _localise_string(context, trimmed);
        return replace_all(s, trimmed, result);
    }

    if (isaalpha(s[0]) || isadigit(s[0]))
    {
        if (s.length() >= 4 && s.substr(1, 3) == " - ")
        {
            // has a menu letter prefix
            string prefix = s.substr(0, 4);
            string rest = s.substr(4);
            rest = _localise_string(context, rest);
            return prefix + strip_context(rest);
        }
        else if (s.length() >= 3 && s.substr(1, 2) == ") ")
        {
            // also a menu letter prefix
            string prefix = s.substr(0, 3);
            string rest = s.substr(3);
            rest = _localise_string(context, rest);
            return prefix + strip_context(rest);
        }
    }

    result = _localise_param_string(context, s);
    if (!result.empty())
        return result;

    // failed - return the original
    return s;
}

bool localisation_active()
{
    return _initialised;
}

string localise(const string &s)
{
    if (!localisation_active())
        return s;

    string result = _localise_string("", s);

    return strip_context(result);
}
