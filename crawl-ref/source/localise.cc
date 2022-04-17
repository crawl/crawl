/*
 * localise.h
 * High-level localisation functions
 */

// don't need this, but forced to include because of stuff included by stringutil.h
#include "AppHdr.h"

#include <sstream>
#include <vector>
#include <cstdarg>
#include <cstdlib>
#include <regex>
#include <typeinfo>
using namespace std;

#ifdef UNIX
# include <langinfo.h>
#endif


#include "localise.h"
#include "xlate.h"
#include "stringutil.h"
#include "unicode.h"
#include "english.h"

#if 0
#define DEBUG(...) fprintf(stderr, "DEBUG: "); fprintf (stderr, __VA_ARGS__); fprintf(stderr, "\n");
#else
#define DEBUG(...)
#endif

static string _language;
static bool _paused;
static string _context;

// check if string contains the char
static inline bool _contains(const std::string& s, char c)
{
    return (s.find(c) != string::npos);
}

// format UTF-8 string using printf-style format specifier
static string _format_utf8_string(const string& fmt, const string& arg)
{
    if (fmt == "%s")
    {
        // trivial case
        return arg;
    }

    // if there are width/precision args, then make_stringf won't handle it correctly
    // (it will count bytes), so we have to handle it in a unicode-aware way
    int width = -1;
    int precision = -1;
    bool right_justify = true; // default for %s

    for (size_t i = 1; i < fmt.length(); i++)
    {
        char ch = fmt[i];
        if (ch == '-')
            right_justify = false;
        else if (ch == '.')
            precision = 0;
        else if (ch >= '0' && ch <= '9')
            if (precision >= 0)
                precision = (precision * 10) + (int)(ch - '0');
            else
            {
                if (width < 0)
                    width = (int)(ch - '0');
                else
                    width = (width * 10) + (int)(ch - '0');
            }
    }

    string result = arg;
    if (precision >= 0)
        result = chop_string(result, precision, false);

    if (width >= 0)
        result = chop_string(result, width, true, right_justify);

    return result;
}

// is this char a printf typespec (i.e. the end of %<something><char>)?
static inline bool _is_type_spec(char c)
{
    static const string type_specs = "diufFeEgGxXoscpaAn";
    return (type_specs.find(c) != string::npos);
}

// Extract arg id from format specifier
// (e.g. for the specifier "%2$d", return 2)
// Returns 0 if no positional specifier
static int _get_arg_id(const string& fmt)
{
    size_t dollar_pos = fmt.find('$');
    if (dollar_pos == string::npos)
    {
        return 0;
    }
    size_t pos_len = dollar_pos-1;
    if (pos_len < 1)
    {
        return 0;
    }
    string pos_str = fmt.substr(1, pos_len);
    int result = atoi(pos_str.c_str());
    if (errno != 0)
    {
        return 0;
    }
    return result;
}

// Remove arg id from arg format specifier
// (e.g. for "%1$d", return "%d")
static string _remove_arg_id(const string& fmt)
{
    size_t dollar_pos = fmt.find('$');
    if (dollar_pos == string::npos)
    {
        return fmt;
    }
    string result("%");
    if (dollar_pos < fmt.length()-1)
    {
        result += fmt.substr(dollar_pos+1, string::npos);
    }
    return result;
}

// translate a format spec like %d to a type like int
static const type_info* _format_spec_to_type(const string& fmt)
{
    if (fmt.length() < 2 || fmt.at(0) != '%')
    {
        return NULL;
    }

    char last_char = fmt.back();
    if (last_char == 'p')
    {
        return &typeid(void*);
    }
    else if (last_char == 's')
    {
        return &typeid(char*);
    }
    else if (last_char == 'n')
    {
        return &typeid(int*);
    }
    else if (_contains("aAeEfFgG", last_char))
    {
        if (fmt.at(fmt.length()-2) == 'L')
        {
            return &typeid(long double);
        }
        else
        {
            return &typeid(double);
        }
    }
    else if (_contains("uoxX", last_char))
    {
        // unsigned
        char penultimate = fmt.at(fmt.length()-2);
        if (penultimate == 'p')
        {
            return &typeid(ptrdiff_t);
        }
        else if (penultimate == 'z')
        {
            return &typeid(size_t);
        }
        else if (penultimate == 'j')
        {
            return &typeid(uintmax_t);
        }
        else if (penultimate == 'l')
        {
            if (fmt.length() > 2 && fmt.at(fmt.length()-3) == 'l')
            {
                return &typeid(unsigned long long);
            }
            else
            {
                return &typeid(unsigned long);
            }
        }
        else
        {
            return &typeid(unsigned);
        }
    }
    else if (_contains("cid", last_char))
    {
        // signed int
        char penultimate = fmt.at(fmt.length()-2);
        if (penultimate == 'p')
        {
            return &typeid(ptrdiff_t);
        }
        else if (penultimate == 'z')
        {
            return &typeid(size_t);
        }
        else if (penultimate == 'j')
        {
            return &typeid(intmax_t);
        }
        else if (penultimate == 'l')
        {
            if (fmt.length() > 2 && fmt.at(fmt.length()-3) == 'l')
            {
                return &typeid(long long);
            }
            else
            {
                return &typeid(long);
            }
        }
        else
        {
            return &typeid(int);
        }
    }

    return NULL;
}

// find the nth occurrence of a char in a string
static size_t _find_nth_occurrence(const string &s, const char c, size_t n)
{
    if (n > 0)
    {
        size_t count = 0;
        size_t pos = s.find(c);
        while (pos != string::npos)
        {
            count++;
            if (count == n)
                return pos;
            pos = s.find(c, pos+1);
        }
    }
    return string::npos;
}

// split on nth occurrence of char
// prefix will contain everything up to and including the target character
// the rest of the string will be returned in suffix
// if less than n occurrences, then prefix will be full string and suffix will be empty
static void _split_after_nth_occurrence(const string &s, const char c, size_t n,
                                        string& prefix, string& suffix)
{
    if (n == 0)
    {
        prefix = "";
        suffix = s;
    }
    else
    {
        size_t pos = _find_nth_occurrence(s, c, n);
        if (pos == string::npos)
        {
            prefix = s;
            suffix = "";
            return;
        }
        else
        {
            prefix = s.substr(0, pos+1); // target char is included
            suffix = s.substr(pos+1);
        }
    }
}

// split format string into constants and format specifiers
static vector<string> _split_format(const string& fmt_str)
{
    vector<string> results;

    int token_start = 0;
    int token_len = 0;
    const char* ch = fmt_str.c_str();

    while (*ch != '\0')
    {
        token_start += token_len;
        token_len = 1;
        bool finished = false;

        if (*ch == '%' && *(ch+1) != '%')
        {
            // read format specifier
            ++ch;
            while (!finished)
            {
                finished = (*ch == '\0' || _is_type_spec(*ch));
                if (*ch != '\0')
                {
                    ++token_len;
                    ++ch;
                }
            }
        }
        else if (*ch == '{')
        {
            // read context specifier
            ++ch;
            while (!finished)
            {
                finished = (*ch == '\0' || *ch == '}');
                if (*ch != '\0')
                {
                    ++token_len;
                    ++ch;
                }
            }
        }
        else
        {
            // read literal string
            while (!finished)
            {
                bool escaped = (*ch == '\\' || (*ch == '%' && *(ch+1) == '%'));
                ++ch;
                if (*ch == '\0')
                {
                    finished = true;
                }
                else if (escaped)
                {
                    // ignore character
                    ++token_len;
                    finished = false;
                }
                else
                {
                    finished = (*ch == '%' || *ch == '{');
                    if (!finished)
                    {
                        ++token_len;
                    }
                }
            }
        }

        // extract token
        results.push_back(fmt_str.substr(token_start, token_len));
    }
    return results;
}

/**
 * Get arg types from format string.
 * Returns a map indexed by argument id, beginning with 1.
 */
static map<int, const type_info*> _get_arg_types(const string& fmt)
{
    map<int, const type_info*> results;
    vector<string> strings = _split_format(fmt);
    int arg_count = 0;
    for (vector<string>::iterator it = strings.begin() ; it != strings.end(); ++it)
    {
        if (it->at(0) == '%' && it->length() > 1 && it->at(1) != '%')
        {
            ++arg_count;
            const type_info* ti = _format_spec_to_type(*it);
            int arg_id = _get_arg_id(*it);
            if (arg_id == 0)
            {
                arg_id = arg_count;
            }
            results[arg_id] = ti;
        }
    }
    return results;
}

static bool is_determiner(const string& word)
{
    if (ends_with(word, "'s"))
        return true;

    string lower = lowercase_string(word);
    if (lower == "a" || lower == "an" || lower == "the"
             || lower == "his" || lower == "her" || lower == "its"
             || lower == "their" || lower == "your")
    {
        return true;
    }

    return false;
}

// replace all instances of given substring
// std::regex_replace would do this, but not sure if available on all platforms
static void _replace_all(std::string& str, const std::string& patt, const std::string& replace)
{
    std::string::size_type pos = 0u;
    while((pos = str.find(patt, pos)) != std::string::npos){
        str.replace(pos, patt.length(), replace);
        pos += replace.length();
    }
}

static void _resolve_escapes(string& str)
{
    _replace_all(str, "%%", "%");
    _replace_all(str, "\\{", "{");
    _replace_all(str, "\\}", "}");
}


static string _localise_annotation(const string& s)
{
    string result = xlate(s);
    if (result != s)
        return result;

    result = "";

    // separate into tokens of either all-alpha or all non-alpha characters
    // should be able to do this with regex, but I couldn't make it work
    size_t i = 0;
    while (i < s.length())
    {
        int alpha = isalpha(s[i]);
        size_t len = 1;
        while (i + len < s.length() - 1 && isalpha(s[i+len]) == alpha)
            len++;
        string token = s.substr(i, len);
        result += alpha ? xlate(token) : token;
        i += len;
    }

    return result;
}

static list<string> _localise_annotations(const list<string>& input)
{
    list<string> output;

    list<string>::const_iterator iter;
    for (iter = input.begin(); iter != input.end(); ++iter)
    {
        output.push_back(_localise_annotation(*iter));
    }

    return output;
}


/**
 * strip annotations in curly or round brackets from end of string
 *
 * @param  s            the string
 * @param  annotations  stripped annotations will be returned in this list
 * @return the string with the annotations stripped off
 */
static string _strip_annotations(const string& s, list<string>& annotations)
{
    string rest = s;
    size_t pos;

    while ((pos = rest.find_last_of("{(")) != string::npos)
    {
        // check for matching bracket
        char last_char = rest[rest.length()-1];
        if (rest[pos] == '{' && last_char != '}')
            break;
        if (rest[pos] == '(' && last_char != ')')
            break;

        // get the leading whitespace as well
        while(pos > 0 && rest[pos-1] == ' ')
            pos--;

        annotations.push_front(rest.substr(pos));
        rest = rest.substr(0, pos);
    }
    return rest;
}

/*
 * Add annotations to the end of a string
 * No brackets or whitespace are added because it's expected that the
 * annotations contain those already
 */
static string _add_annotations(const string& s, const list<string>& annotations)
{
    string result = s;

    list<string>::const_iterator iter;
    for (iter = annotations.begin(); iter != annotations.end(); ++iter)
    {
        result += *iter;
    }

    return result;
}

/*
 * Does it start with a count (number followed by space)
 */
static bool _starts_with_count(const string& s)
{
    for (size_t i = 0; i < s.length(); i++)
    {
        char ch = s[i];
        if (ch == ' ' && i > 0)
            return true;
        else if (!isdigit(ch))
            return false;
    }

    return false;
}

/*
 * Strip count from a string like "27 arrows"
 *
 * @param  s      the string
 * @param  count  the count will be returned here
 * @return the string with the count and following spaces removed
 */
static string _strip_count(const string& s, int& count)
{
    if (s.empty() || !_starts_with_count(s))
    {
        return s;
    }

    size_t pos;
    count = stoi(s, &pos);

    if (s[pos] != ' ')
        return s;

    string rest = s.substr(pos);
    return trim_string_left(rest);
}

// search for context in curly braces before pos and strip it out
static string _strip_context(const string& s, size_t pos, string& context)
{
    size_t ctx_start, ctx_end;

    ctx_end = s.rfind("}", pos);
    if (ctx_end == string::npos || ctx_end == 0)
        return s;

    ctx_start = s.rfind("{", ctx_end-1);
    if (ctx_start == string::npos)
        return s;

    // get context, excluding curlies
    context = s.substr(ctx_start+1, ctx_end - ctx_start - 1);

    string result = s;
    // erase context, including curlies
    result.erase(ctx_start, ctx_end - ctx_start + 1);

    return result;
}

static string _strip_determiner(const string& s, string& determiner)
{
    auto pos = s.find(' ');
    if (pos != string::npos)
    {
        string prefix = s.substr(0, pos);
        if (is_determiner(prefix))
        {
            determiner = prefix;
            // ditch the space in between
            return s.substr(pos + 1);
        }
    }

    return s;
}

// strip a suffix of the form " of <whatever>" or a quoted artefact name
// don't match on "pair of " though
static string _strip_suffix(const string& s, string& suffix)
{
    // find closing quote
    size_t quote_pos = s.rfind("\"");
    // find opening quote
    if (quote_pos != string::npos && quote_pos > 0)
        quote_pos = s.rfind(" \"", quote_pos -1);

    // find " of  "
    size_t of_pos = s.rfind(" of ");
    if (of_pos != string::npos && of_pos > 0)
    {
        // don't match on "pair of "
        size_t pair_pos = s.rfind("pair", of_pos-1);
        if (pair_pos == of_pos - 4)
            of_pos = string::npos;
    }

    if (quote_pos == string::npos && of_pos == string::npos)
    {
        return s;
    }
    else if (of_pos != string::npos
             && (quote_pos == string::npos || of_pos < quote_pos))
    {
        suffix = s.substr(of_pos);
        return s.substr(0, of_pos);
    }
    else
    {
        suffix = s.substr(quote_pos);
        return s.substr(0, quote_pos);
    }
}

static string _insert_adjectives(const string& context, const string& s, const vector<string>& adjs)
{
    // translate and join adjectives
    string adj;
    for (size_t i = 0; i < adjs.size(); i++)
        adj += cxlate(context, adjs[i] + " ");

    // insert
    return replace_first(s, "%s", adj);
}

// insert adjectives at first %s, automatically extracting any context
static string _insert_adjectives(const string& s, const vector<string>& adjs)
{
    size_t pos = s.find("%s");
    if (pos == string::npos)
        return s;

    // get context
    string result = _strip_context(s, pos, _context);

    return _insert_adjectives(_context, result, adjs);
}

// check if string is actually a list of things
static bool is_list(const string& s)
{
    // specific artefacts names
    if (contains(s, "Dice, Bag, and Bottle") || contains(s, "Gyre"))
        return false;

    int bracket_depth = 0;
    for (size_t i = 0; i < s.length(); i++)
    {
        if (s[i] == '(' || s[i] == '{' || s[i] == '[')
            bracket_depth++;
        else if (s[i] == ')' || s[i] == '}' || s[i] == ']')
            bracket_depth--;
        else if (bracket_depth == 0)
        {
            if (s[i] == ',' or s[i] == ';' or s[i] == ':')
                return true;
            else if (strncmp(&s.c_str()[i], " and ", 5) == 0
                     || strncmp(&s.c_str()[i], " or ", 4) == 0)
                return true;
        }
    }

    return false;
}


// forward declarations
static string _localise_string(const string context, const string& value);
static string _localise_counted_string(const string& context, const string& singular,
                                       const string& plural, const int count);
// localise counted string when you only have the plural
static string _localise_counted_string(const string& context, const string& value);
static string _localise_list(const string& context, const string& value);


static string _localise_artefact_suffix(const string& s)
{
    if (s.empty())
        return s;

    // check if there's a specific translation for it
    string loc = xlate(s);
    if (loc != s)
        return loc;

    // otherwise translate "of" if present and quote the rest
    string fmt, arg;
    if (starts_with(s, " of "))
    {
        fmt = " of \"%s\"";
        arg = s.substr(4);
    }
    else
    {
        fmt = " \"%s\"";
        arg = s;
        if (s.length() > 3 && starts_with(s, " \"") && ends_with(s, "\""))
        {
            arg = s.substr(2, s.length()-3);
        }
    }

    fmt = xlate(fmt);
    return make_stringf(fmt.c_str(), arg.c_str());
}

static string _localise_unidentified_scroll(const string& context, const string& name)
{
    static const string pattern1 = "scroll labelled ";
    static const string pattern2 = "scrolls labelled ";

    // find start of label
    size_t pos;
    if ((pos = name.find(pattern1)) != string::npos)
        pos += pattern1.length();
    else if ((pos = name.find(pattern2)) != string::npos)
        pos += pattern2.length();
    else
        return "";

    // separate the label from the rest
    string label = name.substr(pos);
    label = cxlate(context, label);

    string rest = name.substr(0, pos);
    rest += "%s";

    string result;
    if (isdigit(rest[0]))
    {
        int count;
        string plural = _strip_count(rest, count);
        string singular = article_a(replace_first(plural, "scrolls", "scroll"));
        plural = "%d " + plural;
        result = _localise_counted_string(context, singular, plural, count);
    }
    else
    {
        // singular
        result = cxlate(context, rest);
    }

    return replace_last(result, "%s", label);
}

// localise a pair of boots/gloves
// they can have adjectives in 2 places (e.g. "an uncursed pair of glowing boots")
static string _localise_pair(const string& context, const string& name)
{
    size_t pos = name.find("pair of ");
    if (pos == string::npos)
        return "";

    string prefix = name.substr(0, pos);
    string rest = name.substr(pos);

    // break the prefix up into determiner and adjectives
    string determiner;
    vector<string> adj_group1;
    vector<string> words = split_string(" ", prefix, true, false);
    for (size_t i = 0; i < words.size(); i++)
    {
        if (i == 0)
        {
            const string word = lowercase_string(words[i]);
            if (is_determiner(word))
            {
                determiner = word == "an" ? "a" : word;
                continue;
            }
        }
        adj_group1.push_back(words[i]);
    }

    // check for unrand artefact
    string candidate = determiner + " %s" + rest;
    trim_string(candidate);
    string result = cxlate(context, candidate);
    if (result != candidate)
    {
        result = _insert_adjectives(result, adj_group1);
        return result;
    }

    // break it up further
    vector<string> adj_group2;
    string base = determiner + " %s";
    trim_string(base);
    string suffix;
    vector<string> words2 = split_string(" ", rest, true);
    for (size_t i = 0; i < words2.size(); i++)
    {
        const string& word = words2[i];
        if (word == "pair")
            base += word;
        else if (word == "of" && i > 0 && words2[i-1] == "pair")
            base += " " + word;
        else if (word == "boots" || word == "gloves")
            base += " %s" + word;
        else if (word == "of")
            suffix = " " + word;
        else if (!suffix.empty())
            suffix += " " + word;
        else
            adj_group2.push_back(word);
    }

    if (suffix.empty())
        result = cxlate(context, base);
    else
    {
        result = cxlate(context, base + suffix);
        if (result == base + suffix)
        {
            result = cxlate(context, base) + cxlate(context, suffix);
        }
    }

    // first set of adjectives (before the word "pair")
    result = _insert_adjectives(result, adj_group1);

    // second set of adjectives (before the word "boots"/"gloves")
    result = _insert_adjectives(result, adj_group2);

    return result;
}

// try to localise complex item name
//
// Some examples:
//   a +0 broad axe
//   a +7 broad axe of flaming
//   the +11 broad axe "Jetioplo" (weapon) {vorpal, Str+4}
//   the +18 shield of the Gong (worn) {rElec rN+ MR+ EV-5}
//   the +2 pair of boots of Boqauskewui (worn) {*Contam rN+++ rCorr SInv}
//
//
static string _localise_item_name(const string& context, const string& item)
{
    if (item.empty())
        return item;

    // try unlabelled scroll
    string result = _localise_unidentified_scroll(context, item);
    if (!result.empty())
        return result;

    // try "pair of <whatever>"
    result = _localise_pair(context, item);
    if (!result.empty() && result != item)
        return result;

    string suffix;
    string base = _strip_suffix(item, suffix);

    string determiner;
    string owner;
    base = _strip_determiner(base, determiner);

    if (ends_with(determiner,"'s"))
    {
        // determiner is a possessive like "the orc's" or "Sigmund's"
        owner = determiner;
        determiner = "%s";
    }

    int count = 0;
    base = _strip_count(base, count);

    bool success = false;

    // change his/her/its/their to a/an for ease of translation
    // (owner may not have the same gender in other languages)
    if (determiner == "his" || determiner == "her" || determiner == "its" || determiner == "their")
    {
        determiner = "a";
    }
    else if (determiner == "an")
    {
        determiner = "a";
    }

    // try to construct a string that can be translated

    const size_t max_adjectives = 3;
    for (size_t adjs = 0; adjs <= max_adjectives; adjs++)
    {
        // split base string after the nth space
        string part1, part2;
        _split_after_nth_occurrence(base, ' ', adjs, part1, part2);

        string item_en;

        if (!determiner.empty())
        {
            item_en = determiner + " ";
        }
        if (count > 0)
        {
            item_en += to_string(count) + " ";
        }
        // placeholder for adjectives
        item_en += "%s";

        // concatenate the non-adjectives
        item_en += part2;

        bool space_with_adjective = true;

        // first, try with suffix attached
        if (!suffix.empty())
        {
            string branded_item = item_en + suffix;

            result = count > 0
                     ? _localise_counted_string(context, branded_item)
                     :cxlate(context, branded_item);
            success = (result != branded_item);

            if (!success)
            {
                // try with space after adjective
                branded_item = replace_first(branded_item, "%s", "%s ");
                result = count > 0
                         ? _localise_counted_string(context, branded_item)
                         :cxlate(context, branded_item);
                success = (result != branded_item);
                if (success)
                    space_with_adjective = false;
            }
        }

        if (!success)
        {
            // now try without suffix attached
            result = count > 0
                     ? _localise_counted_string(context, item_en)
                     : cxlate(context, item_en);
            success = (result != item_en);

            if (!success)
            {
                // try with space after adjective
                item_en = replace_first(item_en, "%s", "%s ");
                result = count > 0
                         ? _localise_counted_string(context, item_en)
                         : cxlate(context, item_en);
                success = (result != item_en);
                if (success)
                    space_with_adjective = false;
            }

            if (success && !suffix.empty())
            {
                string loc_brand = cxlate(context, suffix);
                if (loc_brand != suffix)
                    result += loc_brand;
                else
                {
                    // assume it's an artefact name
                    result += _localise_artefact_suffix(suffix);
                }
            }
        }

        if (success)
        {
            if (!owner.empty())
            {
                owner = cxlate(context, owner);
                result = replace_first(result, "%s", owner);
            }

            // insert adjectives
            size_t pos = result.find("%s");
            if (pos != string:: npos)
            {
                // find the context for the adjectives
                string new_context = context;
                size_t ctx_pos = result.rfind("{", pos);
                if (ctx_pos != string::npos)
                {
                    size_t ctx_end = result.find("}", ctx_pos);
                    if (ctx_end != string::npos)
                    {
                        new_context = result.substr(ctx_pos + 1, ctx_end - ctx_pos - 1);
                        result.erase(ctx_pos, ctx_end - ctx_pos +1);
                        // remember the context because it can affect later grammar
                        // (e.g. in romance languages, gender of subject affects predicate adjectives)
                        _context = new_context;
                    }
                }
                // localise adjectives
                vector<string> adjectives = split_string(" ", part1);
                string prefix_adjs;
                string postfix_adjs;
                for (size_t k = 0; k < adjs; k++)
                {
                    string adj_en = adjectives[k];
                    if (space_with_adjective)
                        adj_en += " ";
                    string adj = cxlate(new_context, adj_en);
                    if (!adj.empty() && adj[0] == ' ')
                        postfix_adjs += adj;
                    else
                        prefix_adjs += adj;
                }
                if (count_occurrences(result, "%s") == 1)
                {
                    result = replace_first(result, "%s", prefix_adjs + postfix_adjs);
                }
                else
                {
                    result = replace_first(result, "%s", prefix_adjs);
                    result = replace_first(result, "%s", postfix_adjs);
                }
            }

            return result;
        }

    }

    // failed
    return item;
}

// localise a string containing a list of things (joined by commas, "and", "or")
// does nothing if input is not a list
static string _localise_list(const string& context, const string& s)
{
    DEBUG("start _localise_list: context='%s', s='%s'", context.c_str(), s.c_str());

    static vector<string> separators = {", or ", ", and ",
                                        "; or ", "; and ",
                                        ", ", "; ", ": ",
                                        " or ", " and "};

    // annotations can contain commas, so remove them for the moment
    list<string> annotations;
    string value = _strip_annotations(s, annotations);

    _context = context;

    std::vector<string>::iterator it;
    for (it = separators.begin(); it != separators.end(); ++it)
    {
        string sep = *it;
        // split on the first separator only
        vector<string> tokens = split_string(sep, value, false, true, 1);
        // there should only ever be 1 or 2 tokens
        if (tokens.size() == 2)
        {
            // restore annotations
            tokens[1] = _add_annotations(tokens[1], annotations);

            sep = cxlate(context, sep);
            // the tokens could be lists themselves
            string tok0 = _localise_string(context, tokens[0]);

            // If we were called with a non-empty context, we want to make sure it persists.
            // (e.g. German cases - the whole list should be in the same case)
            // However, if we have an empty context, we want to allow any change to the global context to persist.
            // (this could be important if this is not actually a list, but just comething with a comma, etc. in it)
            if (!context.empty())
                _context = context;

            string tok1 = _localise_string(_context, tokens[1]);
            return tok0 + sep + tok1;
        }
    }

    // not a list
    return s;
}

static string _localise_multiple_sentences(const string& context, const string& s)
{
    vector<string> sentences;
    size_t pos;
    size_t last = 0;

    for (pos = 0; pos < s.length(); pos++)
    {
        if (pos == s.length() - 1)
        {
            sentences.push_back(s.substr(last, pos - last + 1));
        }
        else
        {
            char c = s[pos];
            if (c == '.' || c == '?' || c == '!')
            {
                if (s[pos+1] == ' ')
                {
                    sentences.push_back(s.substr(last, pos - last + 1));
                    last = pos + 1;
                }
            }
        }
    }

    if (sentences.size() <= 1)
        return s;

    string result;
    int count = 0;
    for (string sentence: sentences)
    {
        if (sentence == " ")
        {
            result += sentence;
            continue;
        }
        string translated = _localise_string(context, sentence);
        if (count > 0 && translated == sentence)
        {
            // try without the leading space
            translated = _localise_string(context, sentence.substr(1));
            result += cxlate(context, " ", false);
            result += translated;
        }
        else
        {
            result += translated;
        }
        count++;
    }
    return result;
}

// localise a string with count
static string _localise_counted_string(const string& context, const string& singular,
                                       const string& plural, const int count)
{
    string result;
    result = cnxlate(context, singular, plural, count);
    ostringstream cnt;
    cnt << count;
    result = replace_first(result, "%d", cnt.str());
    return result;
}

// localise counted string when you only have the plural
static string _localise_counted_string(const string& context, const string& value)
{
    if (value.empty() || !_starts_with_count(value))
    {
        return value;
    }

    int count;
    string plural = _strip_count(value, count);
    string singular = article_a(singularise(plural));
    plural = "%d " + plural;

    return _localise_counted_string(context, singular, plural, count);
}

// localise counted string when you only have the plural
static string _localise_ghost_name(const string& context, const string& value)
{
    const string suffix = "'s ghost";
    if (!ends_with(value, {suffix.c_str()}))
        return value;

    string name = value.substr(0, value.length() - suffix.length());
    string fmt = cxlate(context, "%s's ghost");
    return make_stringf(fmt.c_str(), name.c_str());
}

static string _localise_location(const string& context, const string& value)
{
    if (starts_with(value, "on level "))
    {
        const regex pattern("^on level ([0-9]+) ");
        smatch sm;
        if (regex_search(value, sm, pattern) && sm.size() == 2)
        {
            string level = sm[1].str();
            string temp = replace_first(value, level, "%d");
            string result = cxlate(context, temp, false);
            return replace_first(result, "%d", level);
        }
    }
    else if (starts_with(value, "between levels "))
    {
        const regex pattern("^between levels ([0-9]+) and ([0-9]+) ");
        smatch sm;
        if (regex_search(value, sm, pattern) && sm.size() == 2)
        {
            string level1 = sm[1].str();
            string level2 = sm[2].str();
            string temp = replace_first(value, level1, "%d");
            temp = replace_first(value, temp, "%d");

            string result = cxlate(context, temp, false);
            result = replace_first(result, "%d", level1);
            result = replace_first(result, "%d", level2);
            return result;
        }
    }
    return "";
}

// localise a string
static string _localise_string(const string context, const string& value)
{
    DEBUG("start _localise_string: context='%s', value='%s'", context.c_str(), value.c_str());

    if (value.empty())
    {
        return value;
    }

    if (contains(value, "\n"))
    {
        // split lines and localise individually
        ostringstream oss;
        vector<string> lines = split_string("\n", value, false, true);
        for (size_t i = 0; i < lines.size(); i++)
        {
            if (i > 0)
                oss << "\n";
            oss << _localise_string(context, lines[i]);
        }
        return oss.str();
    }

    // try simple translation
    string result;
    result = cxlate(context, value, false);
    if (!result.empty())
        return result;
    
    // handle strings like "on level 3 of the dungeon"
    result = _localise_location(context, value);
    if (!result.empty())
        return result;

    // try treating as a plural
    result = _localise_counted_string(context, value);
    if (result != value)
        return result;

    if (is_list(value))
    {
        return _localise_list(context, value);
    }

    // try treating as multiple sentences
    result = _localise_multiple_sentences(context, value);
    if (result != value)
        return result;

    // try treating as ghost name
    result = _localise_ghost_name(context, value);
    if (result != value)
        return result;

    if (regex_search(value, regex("^[a-zA-Z] - ")))
    {
        // has an inventory letter at the front
        string inv_letter = value.substr(0, 4);
        return inv_letter + _localise_string(context, value.substr(4));
    }
    else if (value[0] == '[')
    {
        // has an annotation at the front
        size_t pos = value.find("] ");
        if (pos != string::npos)
        {
            string annotation = value.substr(0, pos+2);
            return annotation + _localise_string(context, value.substr(pos+2));
        }
    }

    // remove annotations
    list<string> annotations;
    string rest = _strip_annotations(value, annotations);
    if (!annotations.empty())
    {
        result = _localise_string(context, rest);
        annotations = _localise_annotations(annotations);
        return _add_annotations(result, annotations);
    }

    // try treating it as an item name
    result = _localise_item_name(context, value);

    return result;
}

static string _localise_string(const string& context, const LocalisationArg& arg)
{
    if (arg.plural.empty())
        return _localise_string(context, arg.stringVal);
    else
        return _localise_counted_string(context, arg.stringVal,
                                        arg.plural, arg.count);
}

void LocalisationArg::init()
{
    intVal = 0;
    longVal = 0L;
    longLongVal = 0L;
    doubleVal = 0.0;
    longDoubleVal = 0.0;
    count = 1;
}

LocalisationArg::LocalisationArg()
{
    init();
    translate = true;
}

LocalisationArg::LocalisationArg(const string& value, bool translat)
    : stringVal(value), translate(translat)
{
    init();
}

LocalisationArg::LocalisationArg(const string& value, const string& plural_val, const int num, bool translat)
    : stringVal(value), plural(plural_val), translate(translat)
{
    init();
    count = num;
}

LocalisationArg::LocalisationArg(const char* value, bool translat)
    : translate(translat)
{
    init();
    if (value != nullptr)
    {
        stringVal = value;
    }
}

LocalisationArg::LocalisationArg(const int value, bool translat)
    : translate(translat)
{
    init();
    intVal = value;
}

LocalisationArg::LocalisationArg(const long value, bool translat)
    : translate(translat)
{
    init();
    longVal = value;
}

LocalisationArg::LocalisationArg(const long long value, bool translat)
    : translate(translat)
{
    init();
    longLongVal = value;
}

LocalisationArg::LocalisationArg(const double value, bool translat)
    : translate(translat)
{
    init();
    doubleVal = value;
}

LocalisationArg::LocalisationArg(const long double value, bool translat)
    : translate(translat)
{
    init();
    longDoubleVal = value;
}

void init_localisation(const string& lang)
{
    _language = lang;
#ifdef UNIX
    if (lang != "" && lang != "en")
    {
        if (strcasecmp(nl_langinfo(CODESET), "UTF-8"))
        {
            fprintf(stderr, "Languages other than English require a UTF-8 locale.\n");
            exit(1);
        }
    }
#endif
}

void pause_localisation()
{
    _paused = true;
}

void unpause_localisation()
{
    _paused = false;
}

const string& get_localisation_language()
{
    return _language;
}

bool localisation_active()
{
    return (!_paused && !_language.empty() && _language != "en");
}

string localise(const vector<LocalisationArg>& args)
{
    if (args.empty())
    {
        return "";
    }

    bool success = false;
    _context = "";

    // first argument is the format string
    LocalisationArg fmt_arg = args.at(0);

    // translate format string
    string fmt_xlated;
    if (fmt_arg.translate && localisation_active())
    {
        fmt_xlated = _localise_string("", fmt_arg);
        success = (fmt_xlated != fmt_arg.stringVal);
    }
    else
    {
        fmt_xlated = fmt_arg.stringVal;
    }

    if (args.size() == 1 || fmt_xlated.empty())
    {
        // We're done here
        return fmt_xlated;
    }

    // get arg types for original English string
    map<int, const type_info*> arg_types = _get_arg_types(fmt_arg.stringVal);

    // now tokenize the translated string
    vector<string> strings = _split_format(fmt_xlated);

    ostringstream ss;

    int arg_count = 0;
    for (vector<string>::iterator it = strings.begin() ; it != strings.end(); ++it)
    {
        if (it->at(0) == '{' && it->length() > 1)
        {
            _context = it->substr(1, it->length() - 2); // strip curly brackets
        }
        else if (it->length() > 1 && it->at(0) == '%' && it->at(1) != '%')
        {
            // this is a format specifier like %s, %d, etc.

            ++arg_count;
            int arg_id = _get_arg_id(*it);
            arg_id = (arg_id == 0 ? arg_count : arg_id);

            map<int, const type_info*>::iterator type_entry = arg_types.find(arg_id);

            // range check arg id
            if (type_entry == arg_types.end() || (size_t)arg_id >= args.size())
            {
                // argument id is out of range - just regurgitate the original string
                ss << *it;
            }
            else
            {
                const LocalisationArg& arg = args.at(arg_id);

                string fmt_spec = _remove_arg_id(*it);
                const type_info* type = _format_spec_to_type(fmt_spec);
                const type_info* expected_type = type_entry->second;

                if (expected_type == NULL || type == NULL || *type != *expected_type)
                {
                    // something's wrong - skip this arg
                }
                else if (*type == typeid(char*))
                {
                    // arg is string
                    if (arg.translate && localisation_active())
                    {
                        string argx = _localise_string(_context, arg);
                        ss << _format_utf8_string(fmt_spec, argx);
                        if (argx != arg.stringVal)
                            success = true;
                    }
                    else
                    {
                        ss << make_stringf(fmt_spec.c_str(), arg.stringVal.c_str());
                    }
                }
                else {
                    if (*type == typeid(long double))
                    {
                        ss << make_stringf(fmt_spec.c_str(), arg.longDoubleVal);
                    }
                    else if (*type == typeid(double))
                    {
                        ss << make_stringf(fmt_spec.c_str(), arg.doubleVal);
                    }
                    else if (*type == typeid(long long) || *type == typeid(unsigned long long))
                    {
                        ss << make_stringf(fmt_spec.c_str(), arg.longLongVal);
                    }
                    else if (*type == typeid(long) || *type == typeid(unsigned long))
                    {
                        ss << make_stringf(fmt_spec.c_str(), arg.longVal);
                    }
                    else if (*type == typeid(int) || *type == typeid(unsigned int))
                    {
                        ss << make_stringf(fmt_spec.c_str(), arg.intVal);
                    }
                }
            }
         }
        else
        {
            // plain string (but could have escapes)
            string str = *it;
            _resolve_escapes(str);
            ss << str;
        }
    }

    string result = ss.str();

    if (localisation_active() && args.size() > 1 && fmt_arg.translate && !success)
    {
        // there may be a translation for the completed string
        result = _localise_string("", result);
    }

    return result;

}

string vlocalise(const string& fmt_str, va_list argp)
{
    if (!localisation_active())
    {
        return vmake_stringf(fmt_str.c_str(), argp);
    }

    va_list args;
    va_copy(args, argp);

    vector<LocalisationArg> niceArgs;

    niceArgs.push_back(LocalisationArg(fmt_str));

    // get arg types for original English string
    map<int, const type_info*> arg_types = _get_arg_types(fmt_str);

    int last_arg_id = 0;
    map<int, const type_info*>::iterator it;
    for (it = arg_types.begin(); it != arg_types.end(); ++it)
    {
        const int arg_id = it->first;
        const type_info& arg_type = *(it->second);

        if (arg_id != last_arg_id + 1)
        {
            // something went wrong
            break;
        }

        if (arg_type == typeid(char*))
        {
            niceArgs.push_back(LocalisationArg(va_arg(args, char*)));
        }
        else if (arg_type == typeid(long double))
        {
            niceArgs.push_back(LocalisationArg(va_arg(args, long double)));
        }
        else if (arg_type == typeid(double))
        {
            niceArgs.push_back(LocalisationArg(va_arg(args, double)));
        }
        else if (arg_type == typeid(long long) || arg_type == typeid(unsigned long long))
        {
            niceArgs.push_back(LocalisationArg(va_arg(args, long long)));
        }
        else if (arg_type == typeid(long) || arg_type == typeid(unsigned long))
        {
            niceArgs.push_back(LocalisationArg(va_arg(args, long)));
        }
        else if (arg_type == typeid(int) || arg_type == typeid(unsigned int))
        {
            niceArgs.push_back(LocalisationArg(va_arg(args, int)));
        }
        else if (arg_type == typeid(ptrdiff_t))
        {
            va_arg(args, ptrdiff_t);
            niceArgs.push_back(LocalisationArg());
        }
        else if (arg_type == typeid(size_t))
        {
            va_arg(args, size_t);
            niceArgs.push_back(LocalisationArg());
        }
        else if (arg_type == typeid(intmax_t) || arg_type == typeid(uintmax_t))
        {
            va_arg(args, intmax_t);
            niceArgs.push_back(LocalisationArg());
        }
        else
        {
            va_arg(args, void*);
            niceArgs.push_back(LocalisationArg());
        }

        last_arg_id = arg_id;
    }

    va_end(args);

    return localise(niceArgs);
}

/**
 * Get the localised equivalent of a single-character
 * (Mostly for prompt answers like Y/N)
 */
int localise_char(char ch)
{
    if (!localisation_active())
        return (int)ch;

    string en(1, ch);

    string loc = xlate(en);

    char32_t res;
    if (utf8towc(&res, loc.c_str()))
    {
        return (int)res;
    }
    else
    {
        // failed
        return (int)ch;
    }
}

/**
 * Localise a string with embedded @foo@ style parameters
 */
string localise(const string& text_en, const map<string, string>& params)
{
    if (text_en.empty())
        return "";
    else if (!localisation_active())
        return replace_keys(text_en, params);

    _context = "";
    string text = xlate(text_en, false);
    if (text.empty())
    {
        // try to translate completed English string
        return xlate(replace_keys(text_en, params));
    }

    size_t at = 0;
    size_t last = 0;
    ostringstream res;
    while ((at = text.find_first_of("@{", last)) != string::npos)
    {
        res << text.substr(last, at - last);
        size_t end;
        if (text[at] == '{')
        {
            // context specifier
            end = text.find('}', at + 1);
            if (end == string::npos)
                break;

            _context = text.substr(at + 1, end - at - 1);
        }
        else
        {
            // parameter
            end = text.find('@', at + 1);
            if (end == string::npos)
                break;

            const string key = text.substr(at + 1, end - at - 1);
            const string* value_en = map_find(params, key);
            if (value_en)
            {
                // translate value and insert
                string value = _localise_string(_context, *value_en);
                if (!key.empty() && isupper(key[0]))
                    value = uppercase_first(value);
                res << value;
            }
            else
            {
                // no value - leave the @foo@ tag
                res << '@' << key << '@';
            }
        }

        last = end + 1;
    }

    if (!last)
        return text;

    res << text.substr(last);
    return res.str();
}

/**
 * Localise a string with a specific context
 */
string localise_contextual(const string& context, const string& text_en)
{
    if (!localisation_active())
        return text_en;
    else
        return _localise_string(context, text_en);
}
