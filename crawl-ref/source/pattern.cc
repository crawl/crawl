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
                        NULL);
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
                            NULL,
                            text, length, 0, 0,
                            ovector, sizeof(ovector) / sizeof(*ovector));
    return (pcre_rc >= 0);
}

////////////////////////////////////////////////////////////////////
#else
////////////////////////////////////////////////////////////////////
// POSIX regular expressions

static void *_compile_pattern(const char *pattern, bool icase)
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
    regex_t *re = static_cast<regex_t *>(compiled_pattern);
    return !regexec(re, text, 0, NULL, 0);
}

////////////////////////////////////////////////////////////////////
#endif

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
    compiled_pattern = NULL;
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
    compiled_pattern = NULL;
    isvalid = true;
    // We don't change ignore_case
    return *this;
}

bool text_pattern::operator== (const text_pattern &tp) const
{
    if (this == &tp)
        return true;

    return (pattern == tp.pattern && ignore_case == tp.ignore_case);
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
