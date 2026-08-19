#include "AppHdr.h"

#ifdef REGEX_PCRE
    // Statically link pcre on Windows
    #if defined(TARGET_OS_WINDOWS)
        #define PCRE_STATIC
    #endif

    #include <pcre.h>
#endif

#ifdef REGEX_POSIX
    #include <regex.h>
#endif

#include "pattern.h"
#include "stringutil.h"

// Resolve any backreferences in the replacement string
static string _resolve_backreferences(const string &s, const string& subst,
#if defined(REGEX_PCRE)
                                      const int* const ovector, int nmatches)
#else
                                      const regmatch_t* matches, int nmatches)
#endif
{
    // shortcut if there are no backrefs
    if (subst.find('$') == string::npos)
        return subst;

    string result = subst;

    result = replace_all(result, "$$", "LITERAL_DOLLAR_SIGN");

    // iterate submatches
    vector<string> backrefs;
    for (int i = 1; i < nmatches; i++)
    {
        // note: end is the character after the last character of the match
#if defined(REGEX_PCRE)
        int start = ovector[i * 2];
        int end = ovector[i * 2 + 1];
#else
        int start = matches[i].rm_so;
        int end = matches[i].rm_eo;
#endif

        if (start < 0 || start >= (int)s.length())
            break;

        if (end < 0 || end > (int)s.length())
            break;

        backrefs.push_back(s.substr(start, end - start));
    }

    // replace in reverse order in case there are more than 9
    // ("$1" would match on "$10" if going forwards)
    for (int i = backrefs.size(); i > 0; i--)
    {
        string backref_key = "$" + std::to_string(i);
        string backref_val = backrefs[i - 1];
        result = replace_all(result, backref_key, backref_val);
    }

    result = replace_all(result, "LITERAL_DOLLAR_SIGN", "$$");

    return result;
}

#if defined(REGEX_PCRE)
////////////////////////////////////////////////////////////////////
// Perl Compatible Regular Expressions

static void *_compile_pattern(const char *pattern, bool icase)
{
    const char *error;
    int erroffset;
    int flags = icase ? PCRE_CASELESS : 0;
    return pcre_compile(pattern,
                        flags,
                        &error,
                        &erroffset,
                        nullptr);
}

static void _free_compiled_pattern(void *cp)
{
    if (cp)
        pcre_free(cp);
}

static bool _pattern_match(void *compiled_pattern, const char *text, int length)
{
    int ovector[42];
    int pcre_rc = pcre_exec(static_cast<pcre *>(compiled_pattern),
                            nullptr,
                            text, length, 0, 0,
                            ovector, sizeof(ovector) / sizeof(*ovector));
    return pcre_rc >= 0;
}

static pattern_match _pattern_match_location(void *compiled_pattern,
                                             const char *text, int length)
{
    int ovector[42];
    int pcre_rc = pcre_exec(static_cast<pcre *>(compiled_pattern),
                            nullptr,
                            text, length, 0, 0,
                            ovector, sizeof(ovector) / sizeof(*ovector));
    if (pcre_rc >= 0)
        return pattern_match::succeeded(string(text), ovector[0], ovector[1]);
    else
        return pattern_match::failed(string(text));
}

static string _pattern_replace(void *compiled_pattern, const string& text,
                               const string& repl, int max_replacements)
{
    string result = text;
    int ovector[42];
    int pos = 0;
    int replace_count = 0;

    while (max_replacements < 0 || replace_count < max_replacements)
    {
        int pcre_rc = pcre_exec(static_cast<pcre *>(compiled_pattern),
                                nullptr,
                                result.c_str(), result.length(), pos, 0,
                                ovector, sizeof(ovector) / sizeof(*ovector));

        if (pcre_rc == 0 || pcre_rc == PCRE_ERROR_NOMATCH)
        {
            // no matches
            return result;
        }
        else if (pcre_rc < 0)
        {
            // error
            return result;
        }

        int start = ovector[0];
        int end = ovector[1];

        string replacement = _resolve_backreferences(result, repl, ovector, pcre_rc);
        result.replace(start, end - start, replacement);
        replace_count++;

        pos = start + (int)replacement.length();
        if (pos >= (int)result.length())
            break;
    }

    return result;
}

static vector<string> _pattern_capture(void *compiled_pattern, const string &text)
{
    int length = (int)text.length();
    int ovector[42];
    vector<string> result;
    int pos = 0;

    while (pos < length) {
        int pcre_rc = pcre_exec(static_cast<pcre *>(compiled_pattern),
                                nullptr,
                                text.c_str(), length, pos, 0,
                                ovector, sizeof(ovector) / sizeof(*ovector));

        if (pcre_rc == 0 || pcre_rc == PCRE_ERROR_NOMATCH)
        {
            // no matches
            return result;
        }
        else if (pcre_rc < 0)
        {
            // error
            return result;
        }

        // 0th match is the overall match, so start from 1
        for (int i = 1; i < pcre_rc; i++)
        {
            int start = ovector[i * 2];
            int end = ovector[i * 2 + 1];

            if (start < 0 || start >= length)
                continue;

            if (end < 0 || end > length)
                continue;

            result.push_back(text.substr(start, end - start));
        }

        pos = ovector[1];
    }

    return result;
}

////////////////////////////////////////////////////////////////////
#else
////////////////////////////////////////////////////////////////////
// POSIX regular expressions

static void *_compile_pattern(const char *pattern, bool icase)
{
    regex_t *re = new regex_t;
    if (!re)
        return nullptr;

    int flags = REG_EXTENDED;
    if (icase)
        flags |= REG_ICASE;
    int rc = regcomp(re, pattern, flags);
    // Nonzero return code == failure
    if (rc)
    {
        delete re;
        return nullptr;
    }
    return re;
}

static void _free_compiled_pattern(void *cp)
{
    if (cp)
    {
        regex_t *re = static_cast<regex_t *>(cp);
        regfree(re);
        delete re;
    }
}

static bool _pattern_match(void *compiled_pattern, const char *text, int length)
{
    UNUSED(length);
    regex_t *re = static_cast<regex_t *>(compiled_pattern);
    return !regexec(re, text, 0, nullptr, 0);
}

static pattern_match _pattern_match_location(void *compiled_pattern,
                                             const char *text, int length)
{
    UNUSED(length);
    regmatch_t match;
    regex_t *re = static_cast<regex_t *>(compiled_pattern);
    if (!regexec(re, text, 1, &match, 0))
        return pattern_match::succeeded(string(text), match.rm_so, match.rm_eo);
    else
        return pattern_match::failed(string(text));
}

static string _pattern_replace(void *compiled_pattern, const string& text,
                               const string& repl, int max_replacements)
{
    const int nmatches = 20;
    regmatch_t matches[nmatches];
    regex_t *re = static_cast<regex_t *>(compiled_pattern);
    int replace_count = 0;
    string result = text;
    int pos = 0;

    while (max_replacements < 0 || replace_count < max_replacements)
    {
        // start after the last match, in case replacement matches the pattern,
        // which would cause an infinite loop if matching from the start every time
        string rest = result.substr(pos);
        if (regexec(re, rest.c_str(), nmatches, matches, 0) != 0)
            return result;

        // note: end is the character after the last character of the match
        int start = matches[0].rm_so;
        int end = matches[0].rm_eo;

        if (start < 0 || start >= (int)rest.length())
            break;

        if (end < 0 || end > (int)rest.length())
            break;

        string replacement = _resolve_backreferences(rest, repl, matches, nmatches);
        result.replace(pos + start, end - start, replacement);

        pos += start + (int)replacement.length();
        replace_count++;
    }
    return result;
}

static vector<string> _pattern_capture(void *compiled_pattern, const string &text)
{
    vector<string> result;
    const int nmatches = 20;
    regmatch_t matches[nmatches];
    regex_t *re = static_cast<regex_t *>(compiled_pattern);
    int pos = 0;

    while (pos < (int)text.length())
    {
        string rest = text.substr(pos);
        if (regexec(re, rest.c_str(), nmatches, matches, 0) != 0)
            break;

        // iterate submatches
        for (int i = 1; i < nmatches; i++)
        {
            // note: end is the character after the last character of the match
            int start = matches[i].rm_so;
            int end = matches[i].rm_eo;

            if (start < 0 || start >= (int)rest.length())
                break;

            if (end < 0 || end > (int)rest.length())
                break;

            result.push_back(rest.substr(start, end - start));
        }

        // 0th element is the overall match
        int overall_end = matches[0].rm_eo;
        if (overall_end < 0 || overall_end >= (int)rest.length())
            break;
        pos += overall_end;
    }

    return result;
}

////////////////////////////////////////////////////////////////////
#endif

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
    if (compiled_pattern)
        _free_compiled_pattern(compiled_pattern);
}

const text_pattern &text_pattern::operator= (const text_pattern &tp)
{
    if (this == &tp)
        return tp;

    if (compiled_pattern)
        _free_compiled_pattern(compiled_pattern);
    pattern = tp.pattern;
    compiled_pattern = nullptr;
    isvalid      = tp.isvalid;
    ignore_case  = tp.ignore_case;
    return *this;
}

const text_pattern &text_pattern::operator= (const string &spattern)
{
    if (pattern == spattern)
        return *this;

    if (compiled_pattern)
        _free_compiled_pattern(compiled_pattern);
    pattern = spattern;
    compiled_pattern = nullptr;
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

bool text_pattern::compile() const
{
    return !empty()?
        !!(compiled_pattern = _compile_pattern(pattern.c_str(), ignore_case))
      : false;
}

bool text_pattern::matches(const char *s, int length) const
{
    return valid() && _pattern_match(compiled_pattern, s, length);
}

pattern_match text_pattern::match_location(const char *s, int length) const
{
    if (valid())
        return _pattern_match_location(compiled_pattern, s, length);
    else
        return pattern_match::failed(string(s));
}

string text_pattern::replace(const string& s, const string& repl,
                             int max_replacements) const
{
    if (valid())
        return _pattern_replace(compiled_pattern, s, repl, max_replacements);
    else
        return s;
}

vector<string> text_pattern::capture(const string& s) const
{
    if (valid())
        return _pattern_capture(compiled_pattern, s);
    else
        return vector<string>();
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
