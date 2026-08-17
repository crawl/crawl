/**
 * @file localise.cc
 * @brief String localisation (translation)
 **/

#include "AppHdr.h"
#include "database.h"
#include "english.h"
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

static string _localise_string(const string& s, bool fallback_en = true);
static string _localise_param_string(const string& s);

static vector<pair<text_pattern, string>> _patterns;
static bool _initialised = false;
static string _context;

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
    debuglog("%ld parameterised translation strings", (long)keys.size());

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
        pattern = str_arg_patt.replace(pattern, "(.*)");
        pattern = "^" + pattern + "$";
        if (contains(key, "shivers"))
            debuglog("\"%s\" -> \"%s\"", key.c_str(), pattern.c_str());
        _patterns.emplace_back(make_pair(text_pattern(pattern), key));
    }
    debuglog("Localisation initialised");
}

// if string starts with a context, remove it and set current context to that
static string _shift_context(const string& str)
{
    string result = str;
    while (starts_with(result, "{"))
    {
        size_t pos = result.find('}');
        if (pos == string::npos)
            break;

        _context = result.substr(1, pos - 1);
        result = result.substr(pos + 1);
    }

    return result;
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
        if (isaalpha(param_str[0]) && param_str[0] != s[0])
            continue;
        if (length_excl_params(s) < length_excl_params(param_str))
            continue;

        if (_patterns[i].first.matches(s))
            return (int)i;
    }

    return -1;
}

static string _localise_param(map<string, string>& params, const string& key)
{
    string low_key = lowercase_first(key);
    string old_key;

    if (params.count(low_key))
        old_key = low_key;
    else if (params.count("the_" + low_key))
        old_key = "the_" + low_key;
    else if (params.count("a_" + low_key))
        old_key = "a_" + low_key;
    else if (starts_with(low_key, "the_") && params.count(low_key.substr(4)))
        old_key = low_key.substr(4);
    else if (starts_with(low_key, "a_") && params.count(low_key.substr(2)))
        old_key = low_key.substr(2);

    if (old_key.empty())
        return "";

    string old_val = params[old_key];
    if (starts_with(low_key, "the_") && !starts_with(old_key, "the_"))
    {
        text_pattern patt("^(the|a|an) ");
        old_val = patt.replace(old_val, "");
        old_val = "the " + old_val;
    }
    else if (starts_with(low_key, "a_") && !starts_with(old_key, "a_"))
    {
        text_pattern patt("^(the|a|an) ");
        old_val = patt.replace(old_val, "");
        old_val = article_a(old_val);
    }
    else if (!starts_with(low_key, "the_") && !starts_with(low_key, "a_"))
    {
        if (starts_with(old_key, "the_") || starts_with(old_key, "a_"))
        {
            text_pattern patt("^(the|a|an) ");
            old_val = patt.replace(old_val, "");
        }
    }

    string new_val = _localise_string(old_val, false);

    // param val might be capitalised due to being at start of sentence
    if (new_val.empty() && old_val.length() > 1 && isaupper(old_val[0]))
        new_val = _localise_string(lowercase_first(old_val), false);

    if (new_val.empty())
        return old_val;

    return isaupper(key[0]) ? uppercase_first(new_val) : new_val;
}

// localise parameterised string
static string _localise_param_string(const string& s)
{
    // try to find matching pattern
    int match_idx = _get_matching_pattern_index(s);
    if (match_idx < 0)
        return "";

    const pair<text_pattern, string>& match = _patterns[match_idx];
    debuglog("Param string: %s", match.second.c_str());

    vector<string> param_keys = extract_params(match.second);
    vector<string> param_vals = match.first.capture(s);

    if (param_keys.size() != param_vals.size())
    {
        debuglog("ERROR: %ld keys, %ld vals\n", (long)param_keys.size(), (long)param_vals.size());
        return "";
    }

    map<string, string> params;
    for (size_t i = 0; i < param_keys.size(); i++)
    {
        // store all params with lowercase keys
        params[lowercase_first(param_keys[i])] = param_vals[i];
    }

    string format = cxlate(_context, match.second);
    if (format.empty())
    {
        debuglog("No translation for: \"%s\"", match.second.c_str());
        return "";
    }
    debuglog("Param string translation: \"%s\"", format.c_str());

    string result;

    size_t curr = 0;
    size_t next = format.find_first_of("{@");

    while (curr < format.length())
    {
        if (next == string::npos)
        {
            result += format.substr(curr);
            break;
        }
        else if (next > curr)
        {
            result += format.substr(curr, next - curr);
            curr = next;
        }

        if (format[next] == '{')
        {
            // set new context
            size_t end = format.find('}', next + 1);
            if (end == string::npos)
            {
                result += format[next];
                curr = next + 1;
            }
            else
            {
                _context = format.substr(next + 1, end - next - 1);
                curr = end + 1;
            }
        }
        else if (format[next] == '@')
        {
            size_t end = format.find('@', next + 1);
            if (end == string::npos)
            {
                result += format[next];
                curr = next + 1;
            }
            else
            {
                string key = format.substr(next + 1, end - next - 1);
                string saved_context = _context;
                string val = _localise_param(params, key);
                if (isaupper(key[0]))
                    val = uppercase_first(val);
                if (!saved_context.empty())
                    _context = saved_context;
                result += val;
                curr = end + 1;
            }
        }
        next = format.find_first_of("{@", curr);
    }

    return result;
}

static string _localise_string(const string& s, bool fallback_en)
{
    if (s.empty())
        return s;

    // check if all whitespace
    string trimmed = trimmed_string(s);
    if (trimmed.empty())
        return s;

    if (is_integer_string(trimmed))
        return s;

    // try simple translation first
    string result = cxlate(_context, s);
    if (!result.empty())
    {
        result = _shift_context(result);
        return result;
    }

    if (trimmed.length() != s.length())
    {
        result = _localise_string(trimmed);
        return replace_all(s, trimmed, result);
    }

    if (isaalpha(s[0]) || isadigit(s[0]))
    {
        if (s.length() >= 4 && s.substr(1, 3) == " - ")
        {
            // has a menu letter prefix
            string prefix = s.substr(0, 4);
            string rest = s.substr(4);
            rest = _localise_string(rest);
            return prefix + rest;
        }
        else if (s.length() >= 3 && s.substr(1, 2) == ") ")
        {
            // also a menu letter prefix
            string prefix = s.substr(0, 3);
            string rest = s.substr(3);
            rest = _localise_string(rest);
            return prefix + rest;
        }
    }

    result = _localise_param_string(s);
    if (!result.empty())
    {
        result = _shift_context(result);
        return result;
    }

    debuglog("No translation found for \"%s\"", s.c_str());
    return fallback_en ? s : "";
}

bool localisation_active()
{
    return Options.language != lang_t::EN && _initialised;
}

string localise(const string &s)
{
    if (!localisation_active())
        return s;

    if (s.empty())
        return s;

    string result;

    // localise lines individually
    auto lines = split_string("\n", s, false, true);
    for (size_t i = 0; i < lines.size(); i++)
    {
        if (i > 0)
            result += "\n";
        if (lines[i].empty() || trimmed_string(lines[i]).empty())
            result += lines[i];
        else
        {
            debuglog("IN:  \"%s\"", lines[i].c_str());
            _context = "";
            string line = _localise_string(lines[i]);
            debuglog("OUT: \"%s\"", line.c_str());
            result += line;
        }
    }

    return result;
}
