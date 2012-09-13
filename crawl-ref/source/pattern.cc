#include "AppHdr.h"

#include "pattern.h"

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
#else
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
#endif
