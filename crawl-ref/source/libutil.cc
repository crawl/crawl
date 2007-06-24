/*
 *  File:       libutil.cc
 *  Summary:    Functions that may be missing from some systems
 *  Written by: ?
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 *
 *  Change History (most recent first):
 *
 *      <1> 2001/Nov/01        BWR     Created
 *
 */

#include "AppHdr.h"
#include "defines.h"
#include "initfile.h"
#include "libutil.h"
#include "externs.h"
#include "macro.h"
#include "stuff.h"
#include <stdio.h>
#include <ctype.h>
#include <stdarg.h>
#include <string.h>

#ifdef WIN32CONSOLE
    #include <windows.h>

    #ifdef WINMM_PLAY_SOUNDS
        #include <mmsystem.h>
    #endif
#endif

#ifdef REGEX_PCRE
    // Statically link pcre on Windows
    #ifdef WIN32CONSOLE
        #define PCRE_STATIC
    #endif

    #include <pcre.h>
#endif

#ifdef REGEX_POSIX
    // Do we still need to include sys/types.h?
    #include <sys/types.h>
    #include <regex.h>
#endif

// Should return true if the filename contains nothing that
// the shell can do damage with.
bool shell_safe(const char *file)
{
    int match = strcspn(file, "\\`$*?|><&\n!");
    return !(match >= 0 && file[match]);
}

void play_sound( const char *file )
{
#if defined(WINMM_PLAY_SOUNDS)
    // Check whether file exists, is readable, etc.?
    if (file && *file)
        sndPlaySound(file, SND_ASYNC | SND_NODEFAULT);
#elif defined(SOUND_PLAY_COMMAND)
    char command[255];
    command[0] = 0;
    if (file && *file && (strlen(file) + strlen(SOUND_PLAY_COMMAND) < 255)
            && shell_safe(file))
    {
        snprintf(command, sizeof command, SOUND_PLAY_COMMAND, file);
        system(command);
    }
#endif
}

std::string strip_filename_unsafe_chars(const std::string &s)
{
    return replace_all_of(s, " .", "");
}

std::string make_stringf(const char *s, ...)
{
    va_list args;
    va_start(args, s);

    char buf[400];
    vsnprintf(buf, sizeof buf, s, args);

    va_end(args);

    return (buf);
}

std::string &uppercase(std::string &s)
{
    for (unsigned i = 0, sz = s.size(); i < sz; ++i)
        s[i] = toupper(s[i]);
    return (s);
}

std::string &lowercase(std::string &s)
{
    for (unsigned i = 0, sz = s.size(); i < sz; ++i)
        s[i] = tolower(s[i]);
    return (s);
}

std::string lowercase_string(std::string s)
{
    lowercase(s);
    return (s);
}

bool ends_with(const std::string &s, const std::string &suffix)
{
    if (s.length() < suffix.length())
        return false;
    return (s.substr(s.length() - suffix.length()) == suffix);
}

int ends_with(const std::string &s, const char *suffixes[])
{
    if (!suffixes)
        return (0);
    for (int i = 0; suffixes[i]; ++i)
    {
        if (ends_with(s, suffixes[i]))
            return (1 + i);
    }
    return (0);
}

// Pluralises a monster or item name. This'll need to be updated for
// correctness whenever new monsters/items are added.
std::string pluralise(const std::string &name, 
                      const char *no_of[])
{
    std::string::size_type pos;

    // Pluralise first word of names like 'eye of draining' or
    // 'scrolls labeled FOOBAR', but only if the whole name is not
    // suffixed by a supplied modifier, such as 'zombie' or 'skeleton'
    if ( (pos = name.find(" of ")) != std::string::npos 
            && !ends_with(name, no_of) )
        return pluralise(name.substr(0, pos)) + name.substr(pos);
    else if ( (pos = name.find(" labeled ")) != std::string::npos
              && !ends_with(name, no_of) )
        return pluralise(name.substr(0, pos)) + name.substr(pos);
    else if (ends_with(name, "us"))
        // Fungus, ufetubus, for instance.
        return name.substr(0, name.length() - 2) + "i";
    else if (ends_with(name, "larva") || ends_with(name, "amoeba"))
        // Giant amoebae sounds a little weird, to tell the truth.
        return name + "e";
    else if (ends_with(name, "ex"))
        // Vortex; vortexes is legal, but the classic plural is cooler.
        return name.substr(0, name.length() - 2) + "ices";
    else if (ends_with(name, "cyclops"))
        return name.substr(0, name.length() - 1) + "es";
    else if (ends_with(name, "y"))
        return name.substr(0, name.length() - 1) + "ies";
    else if (ends_with(name, "elf") || ends_with(name, "olf"))
        // Elf, wolf. Dwarfs can stay dwarfs, if there were dwarfs.
        return name.substr(0, name.length() - 1) + "ves";
    else if (ends_with(name, "mage"))
        // mage -> magi
        return name.substr(0, name.length() - 1) + "i";
    else if ( ends_with(name, "sheep") || ends_with(name, "manes")
            || ends_with(name, "fish") )
        // Maybe we should generalise 'manes' to ends_with("es")?
        return name;
    else if (ends_with(name, "ch") || ends_with(name, "sh") 
            || ends_with(name, "x"))
        // To handle cockroaches, fish and sphinxes. Fish will be netted by
        // the previous check anyway.
        return name + "es";
    else if (ends_with(name, "um"))
        // simulacrum -> simulacra
        return name.substr(0, name.length() - 2) + "a";
    else if (ends_with(name, "efreet"))
        // efreet -> efreeti. Not sure this is correct.
        return name + "i";

    return name + "s";
}

std::string replace_all(std::string s,
                        const std::string &find,
                        const std::string &repl)
{
    std::string::size_type start = 0;
    std::string::size_type found;
   
    while ((found = s.find(find, start)) != std::string::npos)
    {
        s.replace( found, find.length(), repl );
        start = found + repl.length();
    }

    return (s);
}

// Replaces all occurrences of any of the characters in tofind with the
// replacement string.
std::string replace_all_of(std::string s,
                           const std::string &tofind,
                           const std::string &replacement)
{
    std::string::size_type start = 0;
    std::string::size_type found;
   
    while ((found = s.find_first_of(tofind, start)) != std::string::npos)
    {
        s.replace( found, 1, replacement );
        start = found + replacement.length();
    }

    return (s);
}

int count_occurrences(const std::string &text, const std::string &s)
{
    int nfound = 0;
    std::string::size_type pos = 0;

    while ((pos = text.find(s, pos)) != std::string::npos)
    {
        ++nfound;
        pos += s.length();
    }

    return (nfound);
}

std::string trimmed_string( std::string s )
{
    trim_string(s);
    return (s);
}

// also used with macros
std::string &trim_string( std::string &str )
{
    str.erase( 0, str.find_first_not_of( " \t\n\r" ) );
    str.erase( str.find_last_not_of( " \t\n\r" ) + 1 );

    return (str);
}

static void add_segment(std::vector<std::string> &segs,
                        std::string s,
                        bool trim,
                        bool accept_empty)
{
    if (trim && !s.empty())
        trim_string(s);

    if (accept_empty || !s.empty())
        segs.push_back(s);
}

std::vector<std::string> split_string(
        const char *sep, 
        std::string s,
        bool trim_segments,
        bool accept_empty_segments,
        int nsplits)
{
    std::vector<std::string> segments;
    int separator_length = strlen(sep);

    std::string::size_type pos;
    while (nsplits && (pos = s.find(sep)) != std::string::npos)
    {
        add_segment(segments, s.substr(0, pos),
                    trim_segments, accept_empty_segments);
        
        s.erase(0, pos + separator_length);
        
        if (nsplits > 0)
            --nsplits;
    }

    if (!s.empty())
        add_segment(segments, s, trim_segments, accept_empty_segments);

    return segments;
}

// The old school way of doing short delays via low level I/O sync.  
// Good for systems like old versions of Solaris that don't have usleep.
#ifdef NEED_USLEEP

#include <sys/time.h>
#include <sys/types.h>
#include <sys/unistd.h>

void usleep(unsigned long time)
{
    struct timeval timer;

    timer.tv_sec = (time / 1000000L);
    timer.tv_usec = (time % 1000000L);

    select(0, NULL, NULL, NULL, &timer);
}
#endif

// Not the greatest version of snprintf, but a functional one that's 
// a bit safer than raw sprintf().  Note that this doesn't do the
// special behaviour for size == 0, largely because the return value 
// in that case varies depending on which standard is being used (SUSv2 
// returns an unspecified value < 1, whereas C99 allows str == NULL
// and returns the number of characters that would have been written). -- bwr
#ifdef NEED_SNPRINTF

#include <string.h>

int snprintf( char *str, size_t size, const char *format, ... )
{
    va_list argp;
    va_start( argp, format );

    char *buff = new char [ 10 * size ];  // hopefully enough 
    if (!buff)
        end(1, false, "Out of memory\n");

    vsprintf( buff, format, argp );
    strncpy( str, buff, size );
    str[ size - 1 ] = 0;

    int ret = strlen( str );  
    if ((unsigned int) ret == size - 1 && strlen( buff ) >= size)
        ret = -1;

    delete [] buff;

    va_end( argp );

    return (ret);
}

#endif

///////////////////////////////////////////////////////////////////////
// Pattern matching

inline int pm_lower(int ch, bool icase)
{
    return icase? tolower(ch) : ch;
}

// Determines whether the pattern specified by 'pattern' matches the given
// text. A pattern is a simple glob, with the traditional * and ? wildcards.
static bool glob_match( const char *pattern, const char *text, bool icase )
{
    char p, t;
    bool special;

    for (;;)
    {
        p = pm_lower(*pattern++, icase);
        t = pm_lower(*text++, icase);
        special = true;

        if (!p) return t == 0;
        if (p == '\\' && *pattern)
        {
            p       = pm_lower(*pattern++, icase);
            special = false;
        }

        if (p == '*' && special)
            // Try to match exactly at the current text position...
            return !*pattern || glob_match(pattern, text - 1, icase)? true :
                // Or skip one character in the text and try the wildcard
                // match again. If this is the end of the text, the match has
                // failed.
                t? glob_match(pattern - 1, text, icase) : false;
        else if (!t || (p != t && (p != '?' || !special)))
            return false;
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
    delete static_cast<glob_info *>( compiled_pattern );
}

bool glob_pattern_match(void *compiled_pattern, const char *text, int length)
{
    glob_info *gi = static_cast<glob_info *>( compiled_pattern );
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
              text,
              length,
              0,
              0,
              ovector,
              sizeof(ovector) / sizeof(*ovector));
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
        regex_t *re = static_cast<regex_t *>( cp );
        regfree(re);
        delete re;
    }
}

bool pattern_match(void *compiled_pattern, const char *text, int length)
{
    regex_t *re = static_cast<regex_t *>( compiled_pattern );
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
