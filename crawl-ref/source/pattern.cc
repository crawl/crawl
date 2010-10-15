#include "AppHdr.h"

#include "pattern.h"

///////////////////////////////////////////////////////////////////////
// Pattern matching

inline int pm_lower(int ch, bool icase)
{
    return icase? tolower(ch) : ch;
}

// Determines whether the pattern specified by 'pattern' matches the given
// text. A pattern is a simple glob, with the traditional * and ? wildcards.
static bool glob_match(const char *pattern, const char *text, bool icase)
{
    char p, t;
    bool special;

    while (true)
    {
        p = pm_lower(*pattern++, icase);
        t = pm_lower(*text++, icase);
        special = true;

        if (!p)
            return (t == 0);

        if (p == '\\' && *pattern)
        {
            p       = pm_lower(*pattern++, icase);
            special = false;
        }

        if (p == '*' && special)
        {
            // Try to match exactly at the current text position...
            if (!*pattern || glob_match(pattern, text - 1, icase))
                return (true);

            // Or skip one character in the text and try the wildcard match
            // again. If this is the end of the text, the match has failed.
            return (t ? glob_match(pattern - 1, text, icase) : false);
        }
        else if (!t || p != t && (p != '?' || !special))
            return (false);
    }
}

////////////////////////////////////////////////////////////////////
// Basic glob (always available)

struct glob_info
{
    std::string s;
    bool ignore_case;
};

void *compile_glob_pattern(const char *pattern, bool icase)
{
    // If we're using simple globs, we need to box the pattern with '*'
    std::string s = std::string("*") + pattern + "*";
    glob_info *gi = new glob_info;
    if (gi)
    {
        gi->s = s;
        gi->ignore_case = icase;
    }
    return gi;
}

void free_compiled_glob_pattern(void *compiled_pattern)
{
    delete static_cast<glob_info *>(compiled_pattern);
}

bool glob_pattern_match(void *compiled_pattern, const char *text, int length)
{
    glob_info *gi = static_cast<glob_info *>(compiled_pattern);
    return glob_match(gi->s.c_str(), text, gi->ignore_case);
}
////////////////////////////////////////////////////////////////////

#if defined(REGEX_PCRE)
////////////////////////////////////////////////////////////////////
// Perl Compatible Regular Expressions

void *compile_pattern(const char *pattern, bool icase)
{
    const char *error;
    int erroffset;
    int flags = icase? PCRE_CASELESS : 0;
    return pcre_compile(pattern,
                        flags,
                        &error,
                        &erroffset,
                        NULL);
}

void free_compiled_pattern(void *cp)
{
    if (cp)
        pcre_free(cp);
}

bool pattern_match(void *compiled_pattern, const char *text, int length)
{
    int ovector[42];
    int pcre_rc = pcre_exec(static_cast<pcre *>(compiled_pattern),
                            NULL,
                            text, length, 0, 0,
                            ovector, sizeof(ovector) / sizeof(*ovector));
    return (pcre_rc >= 0);
}

////////////////////////////////////////////////////////////////////
#elif defined(REGEX_POSIX)
////////////////////////////////////////////////////////////////////
// POSIX regular expressions

void *compile_pattern(const char *pattern, bool icase)
{
    regex_t *re = new regex_t;
    if (!re)
        return NULL;

    int flags = REG_EXTENDED | REG_NOSUB;
    if (icase)
        flags |= REG_ICASE;
    int rc = regcomp(re, pattern, flags);
    // Nonzero return code == failure
    if (rc)
    {
        delete re;
        return NULL;
    }
    return re;
}

void free_compiled_pattern(void *cp)
{
    if (cp)
    {
        regex_t *re = static_cast<regex_t *>(cp);
        regfree(re);
        delete re;
    }
}

bool pattern_match(void *compiled_pattern, const char *text, int length)
{
    regex_t *re = static_cast<regex_t *>(compiled_pattern);
    return !regexec(re, text, 0, NULL, 0);
}

////////////////////////////////////////////////////////////////////
#else

void *compile_pattern(const char *pattern, bool icase)
{
    return compile_glob_pattern(pattern, icase);
}

void free_compiled_pattern(void *cp)
{
    free_compiled_glob_pattern(cp);
}

bool pattern_match(void *compiled_pattern, const char *text, int length)
{
    return glob_pattern_match(compiled_pattern, text, length);
}

#endif
