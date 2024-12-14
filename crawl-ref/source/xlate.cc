/**
 * @file  xlate.cc
 * @brief Low-level translation routines.
 * This works similarly to gnu gettext, except that it uses plain text files
 * (so there's no need to compile .po to .mo every time you change some text).
 **/

#include "AppHdr.h"
#include "xlate.h"
#include "clua.h"
#include "database.h"
#include "stringutil.h"
#include "regex-rules.h"

#include <cstring>
#include <chrono>
using namespace std;

#if 0
# define dprintf(...) printf(__VA_ARGS__)
#else
# define dprintf(...) (void)0
#endif

#ifdef NO_TRANSLATE
//// compile without translation logic ////

void init_xlate()
{
}

string cxlate(const string &context, const string &text_en)
{
    return text_en;
}

string cnxlate(const string &context,
        const string &singular_en, const string &plural_en, unsigned long n)
{
    return n == 1 ? singular_en : plural_en;
}

#else
//// compile with translation logic ////

// markers for embedded expressions
const string exp_start = "((";
const string exp_end = "))";

// initialise xlate
void init_xlate()
{
    dprintf("init_xlate\n");

    // do pregeneration
    for (int i = 0; i <= 100; i++)
    {
        string rulesKey = "PREGENERATE:" + to_string(i);
        string rulesStr = getTranslatedString(rulesKey);
        if (rulesStr.empty())
            continue;

        dprintf("**** Handle %s ****\n", rulesKey.c_str());
        auto start = chrono::high_resolution_clock::now();
        unsigned entries_added = 0;

        vector<string> rules = split_string("\n", rulesStr, true, false);

        // get key selection regex
        string key_select_regex;
        for (string rule: rules)
        {
            if (starts_with(rule, "SELECT:"))
            {
                rule = trimmed_string(replace_first(rule, "SELECT:", ""));
                if (starts_with(rule, "/"))
                    rule = rule.substr(1);
                if (ends_with(rule, "/"))
                    rule = rule.substr(0, rule.length()-1);

                key_select_regex = rule;
                break;
            }
        }

        dprintf("Key selection rule: /%s/\n", key_select_regex.c_str());
        if (key_select_regex.empty())
            continue;

        vector<string> keys = getTranslationKeysByRegex(key_select_regex);
        dprintf("%lu keys matched\n", keys.size());
        for (const string& orig_key: keys)
        {
            string key = orig_key;
            string value = getTranslatedString(key);
            for (string rule: rules)
            {
                if (starts_with(rule, "KEY:"))
                {
                    rule = rule.substr(4);
                    key = apply_regex_rule(rule, key);
                }
                else
                    value = apply_regex_rule(rule, value);
            }

            if (key == orig_key || getTranslatedString(key) != "")
            {
                // don't overwrite existing entries
                continue;
            }

            dprintf("Adding entry: \"%s\" -> \"%s\"\n", key.c_str(), value.c_str());
            setTranslatedString(key, value);
            entries_added++;
        }

        auto end = chrono::high_resolution_clock::now();
        auto millis = chrono::duration_cast<chrono::milliseconds>(end - start);
        dprintf("%s - %u entries added in %ld ms\n",
               rulesKey.c_str(), entries_added, millis.count());
    }
}

// translate with context
//
// context = the context in which the text is being used (optional, default=none)
//  (for disambiguating same English text used in different contexts which may require different translations)
//  if no translation is found in the specified context, will look for translation at global (no) context
// text_en = English text to be translated
// fallback_en = fall back to English if no translation is found?
string cxlate(const string &context, const string &text_en, bool fallback_en)
{
    if (text_en.empty())
        return text_en;

    string translation;

    if (context.empty())
    {
        // check for translation in global context
        translation = getTranslatedString(text_en);
    }
    else
    {
        // check for translation in specific context
        string ctx_text_en = string("{") + context + "}" + text_en;
        translation = getTranslatedString(ctx_text_en);

        if (translation.empty())
        {
            // check for translation in global context
            translation = getTranslatedString(text_en);

            if (!translation.empty())
            {
                string rules = getTranslatedString(string("GENERATE:") + context);
                if (!rules.empty())
                {
                    // convert default string to context using rules
                    translation = apply_regex_rules(rules, translation);
                }
            }
        }
    }

    if (translation.empty() && fallback_en)
    {
        dprintf("cxlate (fallback): \"%s\" (\"%s\") -> \"%s\"\n", text_en.c_str(), context.c_str(), text_en.c_str());
        return text_en;
    }
    else
    {
        dprintf("cxlate: \"%s\" (\"%s\") -> \"%s\"\n", text_en.c_str(), context.c_str(), translation.c_str());
        return translation;
    }
}

// translate with context and number
// select the plural form corresponding to number
//
// context = the context in which the text is being used (optional, default=none)
//  (for disambiguating same English text used in different contexts which may require different translations)
//  if no translation is found in the specified context, will look for translation at global (no) context
// singular_en = English singular text
// plural_en = English plural text
// n = the count of whatever it is
string cnxlate(const string &context,
        const string &singular_en, const string &plural_en, unsigned long n, bool fallback_en)
{
    if (n == 1)
    {
        // translate English singular form
        return cxlate(context, singular_en, fallback_en);
    }
    else
    {
        // translate English plural form
        string result = cxlate(context, plural_en, fallback_en);
        if (result.empty())
            return "";

        // if the target language has multiple plural forms,
        // there will be an embedded expression for choosing the right one
        if (result.substr(0, exp_start.length()) != exp_start)
        {
            // target language has only one plural form
           return result;
        }

        // Pass in the value of n to the experssion.
        // Surely there's a better way to do this.
        string lua_prefix = make_stringf("n=%ld\nreturn ", n);

        vector<string> lines = split_string("\n", result, false, false);
        for (const string& line: lines)
        {
            size_t pos1 = line.find(exp_start);
            size_t pos2 = line.rfind(exp_end);
            if (pos1 != string::npos && pos2 != string::npos)
            {
                string condition = line.substr(pos1 + exp_start.length(), pos2 - pos1 - exp_start.length());
                if (condition == "")
                {
                    // unconditional
                    return line.substr(pos2 + exp_end.length());
                }

                // evaluate the condition
                string lua = lua_prefix + condition;
                if (clua.execstring(lua.c_str(), "plural_lua", 1))
                {
                    // error
                    continue;
                }

                bool res;
                clua.fnreturns(">b", &res);
                if (res)
                    return line.substr(pos2 + exp_end.length());
            }
        }

        return plural_en;
    }
}

#endif
