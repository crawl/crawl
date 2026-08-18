/**
 * @file localise-util.cc
 * @brief String localisation (translation) utility functions
 **/

#include "AppHdr.h"
#include "database.h"
#include "localise-util.h"
#include "pattern.h"
#include "stringutil.h"

string add_context(const string &context, const string &s)
{
    return "{" + context + "}" + s;
}

string strip_context(const string &s)
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
        results.push_back(s.substr(start + 1, end - start - 1));
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

bool is_integer_string(const string &s)
{
    if (s.empty())
        return false;

    int num_digits = 0;
    for (size_t i = 0; i < s.length(); i++)
    {
        if (isadigit(s[i]))
            num_digits++;
        else if (i == 0 && (s[i] == '+' || s[i] == '-'))
            continue;
        else
            return false;
    }

    return num_digits > 0;
}

bool is_float_string(const string &s)
{
    if (s.empty())
        return false;

    int num_points = 0;
    int digits_before = 0;
    int digits_after = 0;
    for (size_t i = 0; i < s.length(); i++)
    {
        if (isadigit(s[i]))
        {
            if (num_points == 0)
                digits_before++;
            else
                digits_after++;
        }
        else if (s[i] == '.')
        {
            num_points++;
            if (num_points > 1)
                return false;
        }
        else if (i == 0 && (s[i] == '+' || s[i] == '-'))
            continue;
        else
            return false;
    }

    return digits_after > 0;
}

static text_pattern _determiner_pattern("^(the|a|an|some|your|his|her|its|their)[ _]", true);

bool has_determiner(const string& s)
{
    return _determiner_pattern.matches(s);
}

string strip_determiner(const string& s)
{
    return _determiner_pattern.replace(s, "");
}

bool starts_with_uppercase(const string& s)
{
    return !s.empty() && isaupper(s[0]);
}

string maybe_lowercase_first(const string& s)
{
    if (starts_with_uppercase(s))
    {
        if (has_determiner(s) || starts_with(s, "Something"))
            return lowercase_first(s);
    }

    return s;
}

void separate_prefix_annotation(const string&s, string& annotation, string& rest)
{
    annotation = "";
    rest = s;
    if (s.length() < 3)
        return;

    size_t pos = string::npos;
    if (s[0] == '(')
        pos = s.find(')');
    else if (s[0] == '[')
        pos = s.find(']');

    if (pos != string::npos)
    {
        annotation = s.substr(0, pos);
        rest = s.substr(pos);
    }
}

void separate_postfix_annotation(const string&s, string& annotation, string& rest)
{
    annotation = "";
    rest = s;
    if (s.length() < 3)
        return;

    size_t last = s.length() - 1;
    size_t pos = string::npos;
    if (s[last] == ')')
        pos = s.rfind('(');
    else if (s[last] == ']')
        pos = s.rfind('[');
    else if (s[last] == '}')
        pos = s.rfind('{');

    if (pos != string::npos)
    {
        annotation = s.substr(pos);
        rest = s.substr(0, pos);
    }
}

string apply_regex_rule(const string& rule, const string& s)
{
    // need to accept empties because replacement could be empty.
    // However, this means "useless" tokens at start and end.
    vector<string> tokens = split_string("/", rule, false, true);

    try {
        string condition, pattern, replacement;
        if (tokens.size() == 5)
        {
            condition = tokens[1];
            pattern = tokens[2];
            replacement = tokens[3];
        }
        else if (tokens.size() == 4)
        {
            pattern = tokens[1];
            replacement = tokens[2];
        }
        else
        {
            // bad rule
            return s;
        }

        string result;
        text_pattern patt(pattern);
        if (condition.empty())
            result = patt.replace(s, replacement);
        else
        {
            text_pattern cond(condition);
            pattern_match match = cond.match_location(s);
            if (!match)
                return s;

            string replaced = patt.replace(match.matched_text(), replacement);
            result = replace_first(s, match.matched_text(), replaced);
        }
        return result;
    }
    catch (exception& e)
    {
        return s;
    }
}

string apply_regex_rules(const string& rules_str, string s)
{
    vector<string> rules = split_string("\n", rules_str, true, false);
    for (string rule: rules)
        s = apply_regex_rule(rule, s);
    return s;
}
