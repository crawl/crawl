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
#include "skills.h"
#include "stringutil.h"
#include "unicode.h"
#include "english.h"

#if 0
#define DEBUG(...) fprintf(stderr, "DEBUG: %s: ", __FUNCTION__); fprintf (stderr, __VA_ARGS__); fprintf(stderr, "\n");
#else
#define DEBUG(...)
#endif
#define TRACE(...) fprintf(stderr, "DEBUG: %s: ", __FUNCTION__); fprintf (stderr, __VA_ARGS__); fprintf(stderr, "\n");

static string _language;
static bool _paused;
static string _context;

// forward declarations
static string _localise_string(const string context, const string& value);
static string _localise_list(const string context, const string& value);

// is this char a printf typespec (i.e. the end of %<something><char>)?
static inline bool _is_type_spec(char c)
{
    static const string type_specs = "diufFeEgGxXoscpaAn";
    return type_specs.find(c) != string::npos;
}

// Extract arg id from format specifier
// (e.g. for the specifier "%2$d", return 2)
// Returns 0 if no positional specifier
static int _get_arg_id(const string& fmt)
{
    size_t dollar_pos = fmt.find('$');
    if (dollar_pos == string::npos)
        return 0;
    size_t pos_len = dollar_pos-1;
    if (pos_len < 1)
        return 0;
    string pos_str = fmt.substr(1, pos_len);
    errno = 0;
    int result = atoi(pos_str.c_str());
    if (errno != 0)
        return 0;
    return result;
}

// Remove arg id from arg format specifier
// (e.g. for "%1$d", return "%d")
static string _remove_arg_id(const string& fmt)
{
    size_t dollar_pos = fmt.find('$');
    if (dollar_pos == string::npos)
        return fmt;
    string result("%");
    if (dollar_pos < fmt.length()-1)
        result += fmt.substr(dollar_pos+1, string::npos);
    return result;
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

// split format string into constants, format specifiers, and context specifiers
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
                    finished = true;
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
                        ++token_len;
                }
            }
        }

        // extract token
        results.push_back(fmt_str.substr(token_start, token_len));
    }
    return results;
}

// split embedded tags from actual text
static void _split_tags(string s, vector<string>& results)
{
    size_t start, end;

    do {
        start = s.find('<');
        end = s.find('>');

        if (start == string::npos || end == string::npos)
            break;

        // part before the tag
        if (start != 0)
            results.push_back(s.substr(0, start));

        // the tag
        results.push_back(s.substr(start, end-start+1));

        // part after the tag
        s = s.substr(end+1);

    } while (true);

    if (!s.empty())
        results.push_back(s);
}

// if string starts with a context, remove it and set current context to that
// TODO: Come up with a better name for this function
static string _shift_context(const string& str)
{
    // Must keep context for parameters
    if (!starts_with(str, "{"))
        return str;

    bool first = true;
    string result;
    for (string s: _split_format(str))
    {
        if (first && s.length() >= 2 && s[0] == '{' && s[s.length()-1] == '}')
            _context = s.substr(1, s.length()-2);
        else
            result += s;
        first = false;
    }
    return result;
}

// replace all instances of given substring
// std::regex_replace would do this, but not sure if available on all platforms
static void _replace_all(std::string& str, const std::string& patt, const std::string& replace)
{
    std::string::size_type pos = 0u;
    while ((pos = str.find(patt, pos)) != std::string::npos){
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

/*
 * Does it start with an id letter (e.g. "a - a +0 short sword")
 */
static bool _starts_with_menu_id(const string& s)
{
    if (s.length() < 4)
        return false;

    return isalpha(s[0]) && s.substr(1,3) == " - ";
}

static string _strip_menu_id(const string& s, string& id)
{
    if (_starts_with_menu_id(s))
    {
        id = s.substr(0, 4);
        return s.substr(4);
    }
    else
        return s;
}

/*
 * Is this a count (natural number)?
 */
static bool _is_count(const string& s)
{
    for (size_t i = 0; i < s.length(); i++)
    {
        char ch = s[i];
        // note: isdigit() can be affected by locale
        if (ch < '0' || ch > '9')
            return false;
    }

    return !s.empty();
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
        // note: isdigit() can be affected by locale
        else if (ch < '0' || ch > '9')
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
        return s;

    size_t pos;
    count = stoi(s, &pos);

    if (s[pos] != ' ')
        return s;

    string rest = s.substr(pos);
    return trim_string_left(rest);
}

static bool _is_determiner(const string& word)
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
    else if (_is_count(lower))
        return true;

    return false;
}

static string _normalise_determiner(const string& word)
{
    string lower = lowercase_string(word);
    // third-person possessive determiners are hard to translate because
    // grammatical gender in the target language might not match English
    if (lower == "his" || lower == "her" || lower == "its" || lower == "their")
        return "a";
    else if (lower == "an")
        return "a";
    else
        return lower;
}

// localise a string with count
static string _localise_counted_string(const string& context, const string& singular,
                                       const string& plural, const int count)
{
    DEBUG("context='%s', singular='%s', plural='%s', count=%d", \
          context.c_str(), singular.c_str(), plural.c_str(), count);

    string result;
    result = cnxlate(context, singular, plural, count, false);
    result = replace_first(result, "%d", to_string(count));
    return result;
}

// localise counted string when you only have the plural
static string _localise_counted_string(const string& context, const string& value)
{
    if (value.empty() || !_starts_with_count(value))
        return "";

    DEBUG("context='%s', value='%s'", context.c_str(), value.c_str());

    int count;
    string plural = _strip_count(value, count);
    plural = "%d " + plural;

    string singular;
    if (plural == "%d gold" || plural == "%d Gold")
        singular = plural; // "1 gold", not "a gold"
    else
        singular = article_a(singularise(plural));

    return _localise_counted_string(context, singular, plural, count);
}

/// localise a floating point number stored as string
static string _localise_float(const string& value)
{
    string decimal_point = xlate("DECIMAL_POINT", false);
    if (decimal_point == "")
        return value;

    return replace_all_of(value, ".,", decimal_point);
}

// localise counted string when you only have the plural
// does NOT fall back to English
static string _localise_possibly_counted_string(const string& context, const string& value)
{
    if (_starts_with_count(value))
    {
        string result = _localise_counted_string(context, value);
        return result;
    }
    else
        return cxlate(context, value, false);
}

static string _localise_annotation_element(const string& s)
{
    if (s.empty())
        return "";

    DEBUG("s='%s'", s.c_str());

    // try translating whole thing
    string result = xlate(s, false);
    if (!result.empty())
        return result;

    string rest = s;
    string prefix, suffix;

    size_t pos = rest.find_first_not_of(" ");
    if (pos == string::npos)
        return s;
    prefix = rest.substr(0, pos);
    rest = rest.substr(pos);

    pos = rest.find_last_not_of(" ");
    if (pos == string::npos)
        return s;
    suffix = rest.substr(pos+1);
    rest = rest.substr(0, pos+1);

    if (!prefix.empty() || !suffix.empty())
        return prefix + _localise_annotation_element(rest) + suffix;

    if (isdigit(s[0]))
    {
        result =  _localise_counted_string("", s);
        return result.empty() ? s : result;
    }

    vector<string> tokens = split_string(" ", s, false, true);
    for (size_t idx = 0; idx < tokens.size(); idx++)
    {
        if (idx > 0)
            result += " ";

        string tok = tokens[idx];
        if (tok.empty())
            continue;

        // check for alphas
        int last_alpha_pos = -1;
        for (int k = (int)tok.length() - 1; k >= 0; k--)
        {
            if (isalpha(tok[k]))
            {
                last_alpha_pos = k;
                break;
            }
        }

        if (last_alpha_pos < 0)
        {
            result += tok;
            continue;
        }

        string ltok = xlate(tok, false);
        if (!ltok.empty())
            result += ltok;
        else if (last_alpha_pos < (int)tok.length() - 1)
        {
            string tok_suffix = tok.substr(last_alpha_pos+1);
            tok = tok.substr(0, last_alpha_pos+1);
            result += xlate(tok, true) + tok_suffix;
        }
        else
            result += tok;
    }

    return result;

}

static string _localise_annotation(const string& s)
{
    if (s.empty())
        return "";

    DEBUG("s='%s'", s.c_str());

    // try translating whole thing
    string result = xlate(s, false);
    if (!result.empty())
        return result;

    // try without the leading space
    if (s[0] == ' ')
    {
        result = xlate(s.substr(1), false);
        if (!result.empty())
            return string(" ") + result;
    }

    string rest = s;
    string prefix, suffix;

    size_t pos = rest.find_first_not_of("([{ ");
    if (pos == string::npos)
        return s;
    prefix = rest.substr(0, pos);
    rest = rest.substr(pos);

    pos = rest.find_last_not_of(")]} ");
    if (pos == string::npos)
        return s;
    suffix = rest.substr(pos+1);
    rest = rest.substr(0, pos+1);

    if (!prefix.empty() || !suffix.empty())
        return prefix + _localise_annotation(rest) + suffix;

    vector<string> tokens = split_string(",", s, false, true);

    bool first = true;
    for (string token: tokens)
    {
        if (first)
            first = false;
        else
            result += ",";
        result += _localise_annotation_element(token);
    }

    return result;
}

static list<string> _localise_annotations(const list<string>& input)
{
    list<string> output;

    list<string>::const_iterator iter;
    for (iter = input.begin(); iter != input.end(); ++iter)
        output.push_back(_localise_annotation(*iter));

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
        while (pos > 0 && rest[pos-1] == ' ')
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
        result += *iter;

    return result;
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

static void _strip_determiner(const string& s, string& determiner, string& base)
{
    auto pos = s.find(' ');
    if (pos != string::npos)
    {
        string prefix = s.substr(0, pos);
        if (_is_determiner(prefix))
        {
            determiner = prefix;
            // ditch the space in between
            base = s.substr(pos + 1);
            return;
        }
    }

    base = s;
}

static void _separate_first_word(const string& s, string& word, string& rest)
{
    auto pos = s.find(' ');
    if (pos == string::npos)
    {
        word = s;
        rest = "";
    }
    else
    {
        word = s.substr(0, pos);
        rest = s.substr(pos + 1);
    }
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
        return s;
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

// localise adjectives
// it's assumed that the main string is already localised
static string _localise_adjectives(const string& s, const vector<string>& adjs)
{
    string result = s;
    size_t pos = result.find("%s");
    if (pos == string:: npos)
        return result;

    // find the context for the adjectives
    size_t ctx_end = result.rfind("}", pos);
    if (ctx_end != string::npos)
    {
        size_t ctx_pos = result.rfind("{", ctx_end);
        if (ctx_pos != string::npos)
        {
            _context = result.substr(ctx_pos + 1, ctx_end - ctx_pos - 1);
            result.erase(ctx_pos, ctx_end - ctx_pos + 1);
        }
    }

    // some languages (e.g. French) have adjectives both before and after the noun
    string prefix_adjs;
    string postfix_adjs;

    for (const string& adj_en: adjs)
    {
        string adj = cxlate(_context, adj_en + " ", true);
        if (adj[0] == ' ')
            postfix_adjs += adj;
        else
            prefix_adjs += adj;
    }

    if (count_occurrences(result, "%s") == 1)
        result = replace_first(result, "%s", prefix_adjs + postfix_adjs);
    else
    {
        result = replace_first(result, "%s", prefix_adjs);
        result = replace_first(result, "%s", postfix_adjs);
    }

    return result;
}

// localise adjectives
// it's assumed that the main string is already localised
static string _localise_adjectives(const string& s, const string& adj_str)
{
    vector<string> adjectives = split_string(" ", adj_str);
    return _localise_adjectives(s, adjectives);
}


static string _localise_string_with_adjectives(const string& s)
{
    DEBUG("context='%s', value='%s'", _context.c_str(), s.c_str());

    // separate determiner
    string determiner, rest;
    _strip_determiner(s, determiner, rest);
    determiner = _normalise_determiner(determiner);

    // try to translate the biggest base string we can
    string adjectives, result;
    string prefix = determiner + " %s";
    result = _localise_possibly_counted_string(_context, prefix + rest);
    while (result.empty())
    {
        string adj;
        _separate_first_word(rest, adj, rest);
        if (rest.empty())
            return "";
        adjectives += adj + " ";
        result = _localise_possibly_counted_string(_context, prefix + rest);
    }

    if (result.empty())
        return "";

    DEBUG("base=%s, adjectives=%s", result.c_str(), adjectives.c_str());
    result = _localise_adjectives(result, adjectives);
    return _shift_context(result);
}

// get fist tag of form "<foo>" or "</foo>"
// return empty string if none found
static string _get_first_tag(const string& s)
{
    size_t start = s.find('<');
    if (start == string::npos || start == s.length() - 1)
        return "";

    size_t end = s.find('>', start + 1);
    if (end == string::npos)
        return "";

    return s.substr(start, end - start + 1);
}

static string _localise_item_suffix(const string& s)
{
    if (s.empty())
        return s;

    // check if there's a specific translation for it
    string loc = xlate(s, false);
    if (!loc.empty())
        return loc;

    // otherwise, assume artefact name
    // translate "of" if present and quote the rest
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
            arg = s.substr(2, s.length()-3);
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
    static const string pair_of = "pair of ";

    size_t pos = name.find(pair_of);
    if (pos == string::npos)
        return "";

    DEBUG("context='%s', name='%s'", context.c_str(), name.c_str());

    string prefix = name.substr(0, pos);
    string rest = name.substr(pos);

    DEBUG("prefix='%s', rest='%s'", prefix.c_str(), rest.c_str());

    // break the prefix up into determiner and adjectives
    string determiner;
    vector<string> adj_group1;
    vector<string> words = split_string(" ", prefix, true, false);
    for (size_t i = 0; i < words.size(); i++)
    {
        if (i == 0)
        {
            const string word = lowercase_string(words[i]);
            if (_is_determiner(word))
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
    DEBUG("candidate='%s'", candidate.c_str());
    string result = cxlate(context, candidate, false);
    if (result != "")
    {
        result = _insert_adjectives(result, adj_group1);
        return result;
    }

    // break it up further
    string suffix;
    pos = rest.find(" of ", pair_of.length());
    if (pos != string::npos)
    {
        suffix = rest.substr(pos);
        rest = rest.substr(0, pos);
    }

    DEBUG("determiner='%s', rest='%s', suffix='%s'", \
          determiner.c_str(), rest.c_str(), suffix.c_str());

    vector<string> adj_group2;
    string base = determiner.empty() ? string("%s") : (determiner + " %s");
    base += pair_of + "%s";
    rest = rest.substr(pair_of.length());
    vector<string> words2 = split_string(" ", rest, true);

    size_t cnt = words2.size();
    if (cnt >= 2 && words2[cnt-2] == "quick" && words2[cnt-1] == "blades")
    {
        base += words2[cnt-2] + " " + words2[cnt-1];
        cnt -= 2;
    }
    else if (cnt > 0)
    {
        base += words2[cnt-1];
        cnt--;
    }

    for (size_t i = 0; i < cnt; i++)
         adj_group2.push_back(words2[i]);

    DEBUG("base='%s', suffix='%s'", base.c_str(), suffix.c_str());

    if (suffix.empty())
        result = cxlate(context, base);
    else
    {
        result = cxlate(context, base + suffix, false);
        if (result.empty())
            result = cxlate(context, base) + cxlate(context, suffix);
    }

    DEBUG("before inserting adjectives: result='%s'", result.c_str());

    // first set of adjectives (before the word "pair")
    result = _insert_adjectives(result, adj_group1);

    // second set of adjectives (before the word "boots"/"gloves")
    result = _insert_adjectives(result, adj_group2);

    DEBUG("result='%s'", result.c_str());
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
    DEBUG("context='%s', value='%s'", context.c_str(), item.c_str());

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

    string determiner, base, owner;
    _strip_determiner(item, determiner, base);

    if (ends_with(determiner,"'s"))
    {
        // determiner is a possessive like "the orc's" or "Sigmund's"
        owner = determiner;
        determiner = "%s";
    }

    int count = 0;
    base = _strip_count(base, count);

    // change his/her/its/their to a/an for ease of translation
    // (owner may not have the same gender in other languages)
    if (determiner == "his" || determiner == "her" || determiner == "its" || determiner == "their")
        determiner = "a";
    else if (determiner == "an")
        determiner = "a";

    // try to construct a string that can be translated

    const size_t max_adjectives = 3;
    for (size_t adjs = 0; adjs <= max_adjectives; adjs++)
    {
        // split base string after the nth space
        string part1, part2;
        _split_after_nth_occurrence(base, ' ', adjs, part1, part2);

        string item_en;

        if (!determiner.empty())
            item_en = determiner + " ";
        if (count > 0)
            item_en += to_string(count) + " ";
        // placeholder for adjectives
        item_en += "%s";

        // concatenate the non-adjectives
        item_en += part2;

        bool space_with_adjective = true;

        result = _localise_possibly_counted_string(context, item_en);

        if (result.empty())
        {
            // try with space after adjective
            item_en = replace_first(item_en, "%s", "%s ");
            result = _localise_possibly_counted_string(context, item_en);
            if (!result.empty())
                space_with_adjective = false;
        }

        if (!result.empty())
        {
            result = _shift_context(result);
            string new_context = _context;

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
                string prefix_adjs;
                string postfix_adjs;
                if (!space_with_adjective && ends_with(part1, " "))
                    part1 = part1.substr(0, part1.length()-1);
                // first, try to localise adjectives as a block
                prefix_adjs = cxlate(new_context, part1, false);
                if (prefix_adjs.empty() && ends_with(part1, " "))
                {
                    prefix_adjs = cxlate(new_context, part1.substr(0, part1.length()-1), false);
                    if (!prefix_adjs.empty())
                        prefix_adjs += " ";
                }
                if (prefix_adjs.empty())
                {
                    // localise one-by-one
                    vector<string> adjectives = split_string(" ", part1);
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
                }
                if (count_occurrences(result, "%s") == 1)
                    result = replace_first(result, "%s", prefix_adjs + postfix_adjs);
                else
                {
                    result = replace_first(result, "%s", prefix_adjs);
                    result = replace_first(result, "%s", postfix_adjs);
                }
            }

            return result;
        }

    }

    string suffix;
    base = _strip_suffix(item, suffix);
    if (!suffix.empty()) {
        // try to translate base without suffix
        result = cxlate(context, base, false);
        if (!result.empty())
            result = _shift_context(result);
        else
            result = _localise_item_name(context, base);
        if (!result.empty()) {
            string suffix_result = _localise_item_suffix(suffix);
            DEBUG("returning base+suffix: '%s' + '%s'",
                result.c_str(), suffix_result.c_str());
            result += suffix_result;
            return result;
        }
    }

    // failed
    return "";
}

// list separators, order is important - we want to get the longest possible
// match at any given point
static const vector<string> seps =
{
    ", and ", "; and ", " and ",
    ", or ", "; or ", " or ",
    ", ", "; ", ": ", ",", ";"
};

static bool is_list_separator(const string& s)
{
    if (s.empty() || s.length() > 6)
        return false;

    for (const string& sep: seps)
    {
        if (s == sep)
            return true;
    }

    return false;
}

// split list of things into separate strings
// ignore lists that are contained within tags or brackets
static void _split_list(string s, vector<string>& result)
{
    // these specific artifact names contain list separators
    // temporarily replace them to avoid false matches
    static const vector< pair<string,string> > replacements =
    {
        { "Dice, Bag, and Bottle", "dice_bag_bottle" },
        { "\"Gyre\" and \"Gimble\"", "gyre_gimble" },
    };
    bool replaced = false;
    for (auto r: replacements)
    {
        if (contains(s, r.first))
        {
            s = replace_all(s, r.first, r.second);
            replaced = true;
        }
    }

    // ignore lists inside brackets of any description
    size_t next = 0;
    int bracket_depth = 0;
    for (size_t i = 0; i < s.length(); i++)
    {
        if (s[i] == '(' || s[i] == '{' || s[i] == '[')
            bracket_depth++;
        else if (s[i] == ')' || s[i] == '}' || s[i] == ']')
        {
            bracket_depth--;
        }
        else if (bracket_depth == 0)
        {
            for (const string& sep: seps)
            {
                if (s.compare(i, sep.length(), sep) == 0)
                {
                    if (i > next)
                        result.push_back(s.substr(next, i-next));
                    result.push_back(sep);
                    next = i + sep.length();
                    i += sep.length() - 1;
                }
            }
        }
    }

    if (next < s.length())
        result.push_back(s.substr(next));

    if (result.size() == 1)
        result.clear();

    if (!result.empty())
    {
        // make sure whole list is not conatined in a tag
        string start_tag = _get_first_tag(result[0]);
        if (!start_tag.empty())
        {
            string end_tag = start_tag;
            end_tag.replace(0, 2, "</");
            if (!contains(result[0], end_tag))
                result.clear();
        }
    }

    // undo temporary replacements
    if (replaced)
    {
        for (string& res: result)
        {
            for (auto r: replacements)
                res = replace_all(res, r.second, r.first);
        }
    }
}

// localise a string containing a list of things (joined by commas, "and", "or")
// returns empty string if input is not a list
static string _localise_list(const string context, const string& s)
{
    static const text_pattern determiner_patt("^(a|an|the|your|[0-9]+) ", true);

    vector<string> substrings;
    _split_list(s, substrings);
    if (substrings.size() < 2)
        return "";

    DEBUG("context='%s', s='%s'", context.c_str(), s.c_str());

    string result;
    for (string sub: substrings)
    {
        // Make sure the "higher-level" context persists (e.g. in German, the whole list should be in the same case).
        // Also, if it's a list of substantives, we don't want the gender to carry over to the next item.
        if (!context.empty() || determiner_patt.matches(sub))
            _context = context;

        result += _localise_string(_context, sub);
    }

    return result;
}

static string _localise_multiple_sentences(const string& context, const string& s)
{
    vector<string> sentences;
    size_t pos;
    size_t last = 0;

    for (pos = 0; pos < s.length(); pos++)
    {
        if (pos == s.length() - 1)
            sentences.push_back(s.substr(last, pos - last + 1));
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
            result += translated;
        count++;
    }
    return result;
}

static string _localise_shop_name(const string& context, const string& value)
{
    static const string suffixes[] =
    {
        "Shoppe", "Boutique", "Emporium", "Shop",
        "General Store", "Distillery", "Assorted Antiques"
    };

    bool is_shop_name = false;
    for (const string& suffix: suffixes)
    {
        if (ends_with(value, suffix))
        {
            is_shop_name = true;
            break;
        }
    }
    if (!is_shop_name)
        return "";

    // extract owner name
    size_t pos = value.find("'s ");
    if (pos == string::npos)
        return "";
    string owner = value.substr(0, pos);

    // parameterize owner name
    string shop_name = replace_first(value, owner, "@Owner@");

    // translate parameterized shop name
    shop_name = cxlate(context, shop_name, false);
    if (shop_name.empty())
        return "";

    // put owner name back
    shop_name = replace_first(shop_name, "@Owner@", owner);

    return shop_name;
}

// localise ghost/illusion name
static string _localise_ghost_name(const string& context, const string& value)
{
    static const string ghost_suffix = "'s ghost";
    static const string illusion_suffix = "'s illusion";
    string suffix;

    if (ends_with(value, ghost_suffix))
        suffix = ghost_suffix;
    else if (ends_with(value, illusion_suffix))
        suffix = illusion_suffix;
    else
        return "";

    string name = value.substr(0, value.length() - suffix.length());
    string fmt = cxlate(context, string("%s") + suffix);
    return make_stringf(fmt.c_str(), name.c_str());
}

/// localise pan lord name
static string _localise_pan_lord_name(const string& context, const string& value)
{
    static const string suffix = " the pandemonium lord";

    size_t pos = value.find(suffix);
    if (pos == string::npos || pos == 0)
        return "";

    string name = value.substr(0, pos);
    string fmt = string("%s") + value.substr(pos);
    fmt = cxlate(context, fmt, true);

    return replace_first(fmt, "%s", name);
}

static string _localise_jiyva_long_name(const string& context, const string& value)
{
    if (!starts_with(value, "Jiyva J")) {
        return "";
    }

    // extract the randomly-generated second name
    vector<string> words = split_string(" ", value, true, false);
    if (words.size() < 2)
        return "";
    string second_name = words[1];

    string format = replace_first(value, second_name, "%s");
    string result = cxlate(context, format, true);
    result = replace_first(result, "%s", second_name);

    return result;
}

static bool _is_derived_monster(const string& s)
{
    static const vector<string> derived = {
        "skeleton", "skeletons", "zombie", "zombies", "simulacrum", "simulacra",
        "corpse", "corpses", // not really a monster, but derived similarly
    };
    for (const string& d: derived)
        if (ends_with(s, d))
            return true;
    return false;
}

static string _localise_derived_monster_name(const string& context, const string& value)
{
    if (!_is_derived_monster(value))
        return "";

    DEBUG("context='%s', value='%s'", context.c_str(), value.c_str());

    string determiner, rest;
    _strip_determiner(value, determiner, rest);
    determiner = _normalise_determiner(determiner);
    if (!determiner.empty())
        determiner += " ";

    vector<string> words = split_string(" ", rest, true, false);
    string derived = words[words.size() - 1];
    words.pop_back();

    string result, original;
    size_t start;

    // check for a specific translation
    for (start = 0; start < words.size(); start++)
    {
        string candidate;
        for (size_t i = start; i < words.size(); i++)
        {
            if (!candidate.empty())
                candidate += " ";
            candidate += words[i];
        }
        candidate += " " + derived;
        result = _localise_possibly_counted_string(context, determiner + "%s" + candidate);
        if (result.empty())
            result = _localise_possibly_counted_string(context, determiner + candidate);
        if (!result.empty())
            break;
    }

    if (result.empty())
    {
        // determine the original monster
        for (start = 0; start < words.size(); start++)
        {
            string candidate;
            for (size_t i = start; i < words.size(); i++)
            {
                if (!candidate.empty())
                    candidate += " ";
                candidate += words[i];
            }
            original = cxlate(context, candidate, false);
            if (!original.empty())
                break;
            original = cxlate(context, "%s" + candidate, false);
            if (!original.empty())
            {
                original = replace_first(original, "%s", "");
                break;
            }
        }

        // discard context from original monster
        string ctx = _context;
        original = _shift_context(original);
        _context = ctx;
        if (original.empty())
            return "";

        string base = "%s@monster@ " + derived;
        result = _localise_possibly_counted_string(context, determiner + base);
        DEBUG("result=%s", result.c_str());
    }

    vector<string> adjectives;
    for (size_t i = 0; i < start; i++)
        adjectives.push_back(words[i]);

    if (result.empty() && adjectives.empty() && !original.empty())
    {
        // try without adjective placeholder
        string base = "@monster@ " + derived;
        result = _localise_possibly_counted_string(context, determiner + base);
        DEBUG("result=%s", result.c_str());
    }

    if (result.empty())
        return "";

    result = replace_first(result, "@monster@", original);
    result = _localise_adjectives(result, adjectives);

    return result;
}

static string _localise_monster_name(const string& context, const string& value)
{
    DEBUG("context='%s', value='%s'", context.c_str(), value.c_str());

    string result = _localise_ghost_name(context, value);
    if (!result.empty())
        return result;

    result = _localise_pan_lord_name(context, value);
    if (!result.empty())
        return result;

    // make sure it doesn't contain "of the", which would indicate an item
    if (contains(value, " of the "))
        return "";

    // substring " the " could mean a name like "Boghold the orc warlord"
    string prefix, base;
    size_t pos = value.find(" the ");
    if (pos == string::npos)
        base = value;
    else
    {
        // prefix, e.g. "Boghold " (including space)
        prefix = value.substr(0, pos+1);

        // base name, e.g. "the orc warlord"
        base = value.substr(pos+1);
    }

    result = _localise_derived_monster_name(context, base);
    if (!result.empty())
        return prefix + result;

    result = cxlate(context, base, false);
    if (!result.empty())
        return prefix + result;

    result = _localise_string_with_adjectives(base);
    if (!result.empty())
        return prefix + result;

    return "";
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

    // handle string like "Orc:2"
    size_t pos = value.find(':');
    if (pos != string::npos && pos != 0 && pos < value.length()-1)
    {
        string branch = value.substr(0, pos);
        string level = value.substr(pos + 1);
        if (is_all_alphas(branch) && is_all_digits(level))
            return cxlate("branch_abbrev", branch, true) + ":" + level;
    }

    return "";
}

static string _localise_thing_in_location(const string& context, const string& value)
{
    size_t pos = value.find(" on ");
    if (pos == string::npos || pos == 0)
        return "";

    DEBUG("context='%s', value='%s'", context.c_str(), value.c_str());

    string thing = value.substr(0, pos);
    thing = _localise_string(context, thing);

    string location = value.substr(pos);
    if (starts_with(location, " on this"))
        location = xlate(location);
    else
        location = localise(" on %s", location.substr(4));

    // will this work for all languages?
    return thing + location;
}

// tokenise a trsing with @foo@ parameters into params and plain strings
static void tokenise_string_with_params(const string &s, vector<string>& results)
{
    results.clear();
    bool param = false;
    size_t pos = 0;
    size_t prev = 0;
    while ((pos = s.find('@', pos)) != string::npos)
    {
        string tok;
        // include closing @ if param
        if (param)
            pos++;
        tok = s.substr(prev, pos - prev);
        if (!tok.empty())
            results.push_back(tok);
        param = !param;
        prev = pos;
        // move past opening @
        if (param)
            pos++;
    }

    // get the last bit
    string tok = s.substr(prev, s.length() - prev);
    if (!tok.empty())
        results.push_back(tok);
}

static string _reverse_engineer_parameterised_string(const string& s, const string& candidate)
{
    vector<string> tokens;
    map<string, string> params;
    tokenise_string_with_params(candidate, tokens);
    size_t pos = 0;
    string param_name;
    for (const string &tok: tokens)
    {
        if (starts_with(tok, "@") && ends_with(tok, "@"))
        {
            // param
            param_name = tok.substr(1, tok.length() - 2);
        }
        else
        {
            // not param
            if (pos == 0)
            {
                if (!starts_with(s, tok))
                    return "";
            }
            else
            {
                size_t prev = pos;
                pos = s.find(tok, pos);
                if (pos == string::npos)
                    return "";
                if (!param_name.empty())
                {
                    string param_value = s.substr(prev, pos - prev);
                    params[param_name] = param_value;
                    param_name = "";
                }
            }
            pos += tok.length();
        }
    }

    return localise(candidate, params);
}

/*
 * Lua code can only pass a single string to mpr(), so any parameterisation has
 * to be handled on the Lua code, and all we see is the completed string. If the
 * number of possible permutations is small, we can just provide a translation
 * for ech one, but in some cases, the number of permutations is to great for
 * that to be feasible, so we're forced to reverse engineer.
 *
 * Unfortunately, this is tightly coupled to the Lua code (yuk).
 */
static string _reverse_engineer_parameterised_string(const string& s)
{
    static const string candidates[] = {
        // autofight.lua
        "No spell in slot @slot@!",
        // automagic.lua
        "You don't have enough magic to cast @spell_name@!",
        "Automagic will cast spell in slot @slot@ (@spell_name@).",
        "Automagic enabled, will cast spell in slot @slot@ (@spell_name@).",
        // lm_trove.lua
        "This portal requires the presence of @item_name@ to function.",
        "The portal requires @item_name@ for entry.",
        "This portal needs @item_name@ to function.",
        "You don't have @item_name@ with you.",
        // arena_sprint.des
        "Score multiplier: @style_mult@x",
        "You now have @style_points@ arena points (gained @style_gain@).",
        "ROUND @round_id@!",
        // zigsprint.des
        "Welcome to arena @teleport_spot@.",
        "Welcome to arena @arena_number@.",
    };

    string result;

    // avoid infinite loop
    if (contains(s, '@'))
        return "";

    for (const string c: candidates)
    {
        result = _reverse_engineer_parameterised_string(s, c);
        if (!result.empty())
            return result;
    }

    // timed portal noise (lm_tmsg.lua)
    if (starts_with(s, "You hear the ")
        and (contains(s, "avalanche of sand")
             or contains(s, "avalanche of rocks")
             or contains(s, "melting archway")
             or contains(s, "magical portal")
             or (contains(s, "tolling") && contains(s, "bell"))
             or (contains(s, "whistling") && contains(s, "wind"))
             or (contains(s, "rusting") && contains(s, "drain"))
             or (contains(s, "creaking") && contains(s, "portcullis"))
             or (contains(s, "beating") && contains(s, "drum"))))
    {
        string msg;
        string adjective;
        for (const string adj: {"stately ", "brisk ", "urgent ", "frantic "})
            if (contains(s, adj))
            {
                adjective = adj;
                msg = replace_first(s, adj, "@adjective@");
                break;
            }

        if (msg == "")
            msg = replace_first(s, "the ", "the @adjective@");

        return localise(msg, {{"adjective", adjective}});
    }

    return "";
}

// localise a string
static string _localise_string(const string context, const string& value)
{
    DEBUG("context='%s', value='%s'", context.c_str(), value.c_str());

    if (value.empty() || !localisation_active())
        return value;

    _context = context;

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
        return _shift_context(result);

    // do the gross stuff
    result = _reverse_engineer_parameterised_string(value);
    if (result != "")
        return result;

    if (_starts_with_menu_id(value))
    {
        string id;
        string rest = _strip_menu_id(value, id);
        return id + _localise_string(context, rest);
    }

    // if value is a list separator and we didn't get a hit from cxlate(),
    // then there's no point wasting any more time - just use the English one
    if (is_list_separator(value))
        return value;

    // try treating as list
    result = _localise_list(context, value);
    if (!result.empty())
        return result;

    // split out any embedded tags
    vector<string> strings;
    _split_tags(value, strings);
    if (strings.size() > 1)
    {
        result = "";
        for (string s: strings)
        {
            // localise the text, but not the tags
            if (s.length() >= 2 && s[0] == '<' && s[s.length()-1] == '>')
                result += s;
            else
                result += _localise_string(context, s);
        }
        return result;
    }

    if (starts_with(value, "buy "))
    {
        string item = value.substr(4);
        return localise("buy %s", item);
    }

    // handle strings like "on level 3 of the dungeon"
    result = _localise_location(context, value);
    if (!result.empty())
        return result;

    // handle strings like "a +0 short sword on level 3 of the dungeon"
    result = _localise_thing_in_location(context, value);
    if (!result.empty())
        return result;

    // try treating as a plural
    result = _localise_counted_string(context, value);
    if (!result.empty())
        return result;

    // try treating as multiple sentences
    result = _localise_multiple_sentences(context, value);
    if (result != value)
        return result;

    // try treating as a shop name
    result = _localise_shop_name(context, value);
    if (!result.empty())
        return result;

    // try treating as Jiyva long name
    result = _localise_jiyva_long_name(context, value);
    if (!result.empty())
        return result;

    // try treating as monster name
    result = _localise_monster_name(context, value);
    if (!result.empty())
        return result;

    _context = context;
    result = _localise_string_with_adjectives(value);
    if (!result.empty())
        return result;


    if (value[0] == '[')
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
    if (!result.empty()) {
        DEBUG("context='%s', value='%s': success as item name: %s",
              context.c_str(), value.c_str(), result.c_str());
        return result;
    }

    // failed
    DEBUG("context='%s', value='%s': failed. returning original value",
          context.c_str(), value.c_str());
    return value;
}

static string _localise_string(const string& context, const LocalisationArg& arg)
{
    if (arg.plural.empty())
        return _localise_string(context, arg.stringVal);

    string result;
    result = _localise_counted_string(context, arg.stringVal,
                                      arg.plural, arg.count);
    return result.empty() ? arg.plural : result;
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
        stringVal = value;
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
    return !_paused && !_language.empty() && _language != "en";
}

static string _build_string(const vector<LocalisationArg>& args, bool translate)
{
    if (args.empty())
        return "";

    _context = "";

    // first argument is the format string
    LocalisationArg fmt_arg = args.at(0);

    // translate format string
    string fmt_xlated;
    if (fmt_arg.translate && translate)
        fmt_xlated = _localise_string("", fmt_arg);
    else
        fmt_xlated = fmt_arg.stringVal;

    // get arg types for original English string
    arg_type_map_t arg_types;
    get_arg_types(fmt_arg.stringVal, arg_types);

    // now tokenize the translated string
    vector<string> strings = _split_format(fmt_xlated);

    ostringstream ss;

    int arg_count = 0;
    for (vector<string>::iterator it = strings.begin() ; it != strings.end(); ++it)
    {
        if (it->at(0) == '{' && it->length() > 1)
        {
            _context = it->substr(1, it->length() - 2); // strip curly brackets
            if (!translate)
            {
                // don't strip context from English string
                ss << *it;
            }
        }
        else if (it->length() > 1 && it->at(0) == '%' && it->at(1) != '%')
        {
            // this is a format specifier like %s, %d, etc.

            ++arg_count;
            int arg_id = _get_arg_id(*it);
            arg_id = (arg_id == 0 ? arg_count : arg_id);

            arg_type_map_t::iterator type_entry = arg_types.find(arg_id);

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
                const type_info* type = format_spec_to_type(fmt_spec);
                const type_info* expected_type = type_entry->second;

                if (expected_type == NULL || type == NULL || *type != *expected_type)
                {
                    // something's wrong - skip this arg
                }
                else if (*type == typeid(char*))
                {
                    // arg is string
                    if (arg.translate && translate)
                    {
                        string argx = _localise_string(_context, arg);
                        ss << format_utf8_string(fmt_spec, argx);
                    }
                    else
                        ss << make_stringf(fmt_spec.c_str(), arg.stringVal.c_str());
                }
                else {
                    if (*type == typeid(long double))
                    {
                        string val = make_stringf(fmt_spec.c_str(), arg.longDoubleVal);
                        if (arg.translate && translate)
                            ss << _localise_float(val);
                        else
                            ss << val;
                    }
                    else if (*type == typeid(double))
                    {
                        string val = make_stringf(fmt_spec.c_str(), arg.doubleVal);
                        if (arg.translate && translate)
                            ss << _localise_float(val);
                        else
                            ss << val;
                    }
                    else if (*type == typeid(long long) || *type == typeid(unsigned long long))
                        ss << make_stringf(fmt_spec.c_str(), arg.longLongVal);
                    else if (*type == typeid(long) || *type == typeid(unsigned long))
                        ss << make_stringf(fmt_spec.c_str(), arg.longVal);
                    else if (*type == typeid(int) || *type == typeid(unsigned int))
                        ss << make_stringf(fmt_spec.c_str(), arg.intVal);
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

    return result;

}

string localise(const vector<LocalisationArg>& args)
{
    _context = "";

    // trivial cases
    if (args.empty())
        return "";
    else if (args.size() == 1)
    {
        if (localisation_active())
            return _localise_string("", args.at(0).stringVal);
        else
            return args.at(0).stringVal;
    }

    string english = _build_string(args, false);
    if (!localisation_active() || english.empty())
        return english;

    // check for translation of completed english string
    string result = xlate(english, false);
    if (!result.empty())
        return _shift_context(result);

    // translate piecemeal
    return _build_string(args, true);
}

string vlocalise(const string& fmt_str, va_list argp)
{
    if (!localisation_active())
        return vmake_stringf(fmt_str.c_str(), argp);

    va_list args;
    va_copy(args, argp);

    vector<LocalisationArg> niceArgs;

    niceArgs.push_back(LocalisationArg(fmt_str));

    // get arg types for original English string
    arg_type_map_t arg_types;
    get_arg_types(fmt_str, arg_types);

    int last_arg_id = 0;
    arg_type_map_t::iterator it;
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
            niceArgs.push_back(LocalisationArg(va_arg(args, char*)));
        else if (arg_type == typeid(long double))
            niceArgs.push_back(LocalisationArg(va_arg(args, long double)));
        else if (arg_type == typeid(double))
            niceArgs.push_back(LocalisationArg(va_arg(args, double)));
        else if (arg_type == typeid(long long) || arg_type == typeid(unsigned long long))
            niceArgs.push_back(LocalisationArg(va_arg(args, long long)));
        else if (arg_type == typeid(long) || arg_type == typeid(unsigned long))
            niceArgs.push_back(LocalisationArg(va_arg(args, long)));
        else if (arg_type == typeid(int) || arg_type == typeid(unsigned int))
            niceArgs.push_back(LocalisationArg(va_arg(args, int)));
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
        return (int)res;
    else
    {
        // failed
        return (int)ch;
    }
}

/**
 * @brief Fix some stuff that only works in English
 */
static void _fix_parameters(string& text, map<string, string>& params)
{
    if (contains(text, "your @") || contains(text, "Your @"))
    {
        // make sure hand number is right (because player may have sacrificed one to Ru)
        if (contains(text, "your @hands@") || contains(text, "Your @hands@"))
        {
            const string* hands = map_find(params, "hands");
            if (hands != nullptr && !hands->empty())
            {
                if (!ends_with(*hands, "s") && !ends_with(*hands, "ae"))
                {
                    // actually singular
                    text = replace_all(text, "@hands@", "@hand@");
                    params["hand"] = *hands;
                }
            }
        }

        // Translation of "your" may vary depending on grammatical gender/case,
        // so we need to make the parameter @your_foo@ instead of your @foo@.
        while (true)
        {
            size_t pos = text.find("your @");
            if (pos == string::npos)
                pos = text.find("Your @");
            if (pos == string::npos)
                break;

            string your = (text[pos] == 'Y' ? "Your" : "your");

            size_t start = pos + strlen("your @");
            size_t end = text.find('@', start);
            if (end != string::npos)
            {
                string param_name = text.substr(start, end-start);
                auto iter = params.find(param_name);
                if (iter != params.end()) {
                    string param_value = iter->second;

                    param_name = your + "_" + param_name;
                    param_value = your + " " + param_value;
                    params[param_name] = param_value;
                }
            }

            text = replace_first(text, your + " @", "@" + your + "_");
        }
    }
}

/**
 * Localise a string with embedded @foo@ style parameters
 */
string localise(const string& text_in, const map<string, string>& params_in, bool localise_text)
{
    static int nested_calls = 0;

    if (text_in.empty())
        return "";

    string english = replace_keys(text_in, params_in);
    if (!localisation_active())
        return english;

    // check if there's a translation for the completed English string
    string result = xlate(english, false);
    if (!result.empty())
        return result;

    _context = "";
    string text = text_in;
    map<string, string> params = params_in;
    if (localise_text)
    {
        _fix_parameters(text, params);
        text  = xlate(text, true);
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
            if (!value_en)
                value_en = map_find(params, lowercase_string(key));

            if (value_en)
            {
                // translate value and insert
                string value;
                if (key == "player_name")
                {
                    // don't try to translate player name
                    value = *value_en;
                }
                else
                {
                    value = _localise_string(_context, *value_en);
                    // allow nesting, but avoid infinite loops from circular refs
                    if (nested_calls < 5 && value.find("@") != string::npos)
                    {
                        nested_calls++;
                        value = localise(value, params, false);
                        nested_calls--;
                    }
                }
                if (!key.empty() && isupper(key[0]))
                    value = uppercase_first(value);
                res << value;
            }
            else
            {
                // no value - leave the @foo@ tag
                if (!_context.empty())
                    res << "{" << _context << "}";
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

/**
 * Localise a player title
 *
 * We can't localise at point of generation because we need the English version
 * to be stored in the scores file, so we have to deconstruct after the fact.
 */
string localise_player_title(const string& text)
{
    if (text.empty() || !localisation_active())
        return text;

    // try simple translation first
    string result = xlate(text, false);
    if (!result.empty())
        return result;

    string determiner;
    string title;
    if (starts_with(text, "the "))
    {
        determiner = text.substr(0, 4);
        title = text.substr(4);
    }
    else
        title = text;

    vector<string> vtitles;
    get_variable_player_titles(vtitles);

    for (const string& vtitle: vtitles)
    {
        size_t param_start = vtitle.find('@');
        size_t param_end = vtitle.rfind('@');
        if (param_start == string::npos || param_start == param_end)
            continue;

        string param = vtitle.substr(param_start, param_end - param_start + 1);

        string rest;
        if (param_start == 0)
        {
            // param is at start
            rest = vtitle.substr(param_end+1);
            if (!ends_with(title, rest))
                continue;
        }
        else if (param_end == vtitle.length() - 1)
        {
            // param is at end
            rest = vtitle.substr(0, param_start);
            if (!starts_with(title, rest))
                continue;
        }

        string param_name = param.substr(1, param.length()-2);
        string param_val = replace_first(title, rest, "");
        map<string, string> params = { { param_name, param_val } };
        return localise(determiner + vtitle, params);
    }

    return text;
}
