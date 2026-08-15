/**
 * @file localise.cc
 * @brief String localisation (translation)
 **/

#include "AppHdr.h"
#include "database.h"
#include "localise.h"
#include "localise-util.h"
#include "options.h"
#include "regex-wrapper.h"
#include "stringutil.h"

#include <string>
#include <vector>
//#include <pair>

using namespace std;

//#define debuglog(...) fprintf(stderr, "DEBUG: %s: ", __FUNCTION__); fprintf (stderr, __VA_ARGS__); fprintf(stderr, "\n");

#ifndef debuglog
#define debuglog(...) {}
#endif

static void _init_localisation();
static string _localise_string(const string& context, const string& s);
static string _localise_param_string(const string& context, const string& s);

static vector<pair<string, string>> _patterns;
static bool _initialised = false;

static void _init_localisation()
{
    if (_initialised)
        return;
    _initialised = true;

    // get all translation keys which contain parameters
    vector<string> keys = getTranslatedKeysByRegex("@[^@]+@");

    // convert paramaterised string to regex pattern
    for (const string& key: keys)
    {
        string pattern = key;
        // escape characters that mean something to regex
        pattern = escape_regex_specials(pattern);
        pattern = regexp_replace(pattern, "@num[^@]*@", "([0-9]+)");
        pattern = regexp_replace(pattern, "@[^@]+@", "(.*?)");
        pattern = "^" + pattern + "$";
        _patterns.emplace_back(make_pair(pattern, key));
    }
    debuglog("Localisation initialised");
}

static pair<string, string> _get_matching_pattern(const string& s)
{
    pair<string, string> result;

    for (const pair<string, string>& elem: _patterns)
    {
        if (regexp_match(s, elem.first) && elem.first.length() > result.first.length())
            result = elem;
    }

    return result;
}

// localise parameterised string
static string _localise_param_string(const string& context, const string& s)
{
    // try to find matching pattern
    pair<string, string> match = _get_matching_pattern(s);
    if (match.first == "" || match.second == "")
        return "";

    debuglog("String: %s", s.c_str());
    debuglog("Pattern: %s", match.first.c_str());
    debuglog("Param string: %s", match.second.c_str());

    vector<string> param_keys = extract_params(match.second);
    vector<string> param_vals = regexp_capture(s, match.first);

    if (param_keys.size() != param_vals.size())
    {
        fprintf(stderr, "ERROR: %ld keys, %ld vals\n", param_keys.size(), param_vals.size());
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

    // don't translate integer
    if (regexp_match(s, "^(\\+|-)[0-9]+$"))
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

    result = _localise_param_string(context, s);
    if (!result.empty())
        return result;

    // failed - return the original
    return s;
}

bool localisation_active()
{
    return Options.language != lang_t::EN;
}

string localise(const string &s)
{
    if (!localisation_active())
        return s;

    _init_localisation();

    return _localise_string("", s);
}
