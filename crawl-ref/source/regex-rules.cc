#include <string>
#include <vector>
using namespace std;

#include "regex-rules.h"
#include "regex-wrapper.h"
#include "stringutil.h"

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
        if (condition.empty())
            result = regexp_replace(s, pattern, replacement);
        else
        {
            string match = regexp_search(s, condition);
            if (match == "")
                return s;

            string replaced = regexp_replace(match, pattern, replacement);
            result = replace_first(s, match, replaced);
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
