/**
 * @file localise-util.cc
 * @brief String localisation (translation) utility functions
 **/

#include "AppHdr.h"
#include "database.h"
#include "localise-util.h"
#include "pattern.h"
#include "stringutil.h"

//#define debuglog(...) {fprintf(stderr, "DEBUG: %s: ", __FUNCTION__); fprintf (stderr, __VA_ARGS__); fprintf(stderr, "\n");}

#ifndef debuglog
#define debuglog(...) {}
#endif

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

static const text_pattern _determiner_pattern(
    "^(the|a|an|some|your|his|her|its|their)[ _]", true
);

bool has_determiner(const string& s)
{
    return _determiner_pattern.matches(s);
}

string strip_determiner(const string& s)
{
    return _determiner_pattern.replace(s, "");
}

bool is_adverb(const string& s)
{
    string trimmed = trimmed_string(s);
    return trimmed == "very" || ends_with(trimmed, "ly");
}

bool is_determiner(const string& s)
{
    const static vector<string> determiners = {
        "the", "a", "an", "some", "@num@"
    };
    string temp = lowercase_string(trimmed_string(s));
    return std::find(determiners.begin(), determiners.end(), temp) != determiners.end();
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

void separate_end_punctuation(const string& s, string& punct, string& rest)
{
    size_t pos = s.find_last_not_of(".!?");
    if (pos == string::npos)
    {
        punct = s;
        rest = "";
    }
    else
    {
        punct = s.substr(pos + 1);
        rest = s.substr(0, pos + 1);
    }
}

string get_end_punctuation(const string& s)
{
    string punct, rest;
    separate_end_punctuation(s, punct, rest);
    return punct;
}

void separate_menu_letter_prefix(const string& s, string& prefix, string& rest)
{
    prefix = "";
    rest = s;

    if (s.length() < 3)
        return;

    if (isaalpha(s[0]) || isadigit(s[0]))
    {
        if (s.length() >= 4 && s.substr(1, 3) == " - ")
        {
            prefix = s.substr(0, 4);
            rest = s.substr(4);
        }
        else if (s.length() >= 3 && s.substr(1, 2) == ") ")
        {
            prefix = s.substr(0, 3);
            rest = s.substr(3);
        }
    }
}

void separate_prefix_annotation(const string& s, string& annotation, string& rest)
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

    if (pos < s.length() - 1 && s[pos+1] == ':')
    {
        // not an annotation
        return;
    }

    if (pos != string::npos)
    {
        annotation = s.substr(0, pos + 1);
        rest = s.substr(pos + 1);
    }
}

void separate_postfix_annotation(const string& s, string& annotation, string& rest)
{
    annotation = "";
    rest = s;
    if (s.length() < 3)
        return;

    size_t last = s.length() - 1;
    size_t pos = string::npos;
    if (s[last] == ')')
        pos = s.find(')') == last ? s.find('(') : s.rfind('(');
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

vector<string> tokenise_parameterised_string(const string& s)
{
    vector<string> result;

    size_t curr = 0;
    size_t next = s.find_first_of("{@");

    while (curr < s.length())
    {
        if (next == string::npos)
        {
            result.push_back(s.substr(curr));
            break;
        }
        else if (next > curr)
        {
            result.push_back(s.substr(curr, next - curr));
            curr = next;
        }

        char terminator = s[next] == '@' ? '@' : '}';
        size_t end = s.find(terminator, next + 1);

        if (end == string::npos)
        {
            result.push_back(s.substr(next));
            break;
        }
        else
        {
            result.push_back(s.substr(next, end - next + 1));
            curr = end + 1;
        }

        next = s.find_first_of("{@", curr);
    }

    return result;
}

vector<string> tokenise_comma_separated_list(const string& s)
{
    // looks like a list, but isn't
    static const string false_alarm = "Dice, Bag, and Bottle";

    vector<string> result;

    static const vector<string> seps = {
        ", ", "; ", " and ", " or "
    };

    result.push_back(replace_all(s, false_alarm, "FALSE_ALARM"));

    for (auto sep: seps)
    {
        vector<string> temp = result;
        result.clear();

        for (auto token: temp)
        {
            auto temp2 = split_string(sep, token, false, true);
            for (size_t i = 0; i < temp2.size(); i++)
            {
                if (i != 0)
                    result.push_back(sep);
                result.push_back(temp2[i]);
            }
        }
    }

    // combine things like ", and " into one separator
    for (size_t i = 1; i < result.size(); i++)
    {
        if (result[i - 1] != ", " && result[i - 1] != "; ")
            continue;
        else if (starts_with(result[i], "and "))
        {
            result[i - 1] += "and ";
            result[i] = result[i].substr(strlen("and "));
        }
        else if (starts_with(result[i], "or "))
        {
            result[i - 1] += "or ";
            result[i] = result[i].substr(strlen("or "));
        }
    }

    for (size_t i = 0; i < result.size(); i++)
        result[i] = replace_all(result[i], "FALSE_ALARM", false_alarm);

    return result;
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

            int start = match.start_pos();
            int end = match.end_pos();
            if (start < 0 || end > (int)s.length() || end <= start)
                return s;

            string matched_text = s.substr(start, end - start);
            string replaced = patt.replace(matched_text, replacement);
            result = s;
            result.replace(start, end - start, replaced);
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
