/*
 *  File:       libutil.cc
 *  Summary:    Functions that may be missing from some systems
 *  Written by: ?
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 */

#include "AppHdr.h"
REVISION("$Rev$");

#include "defines.h"
#include "directn.h"
#include "initfile.h"
#include "itemname.h" // is_vowel()
#include "libutil.h"
#include "externs.h"
#include "macro.h"
#include "stuff.h"
#include <sstream>
#include <stdio.h>
#include <ctype.h>
#include <stdarg.h>
#include <string.h>

#if defined(WIN32CONSOLE) || defined(WIN32TILES)
    #undef ARRAYSZ
    #include <windows.h>
    #undef max

    #ifdef WINMM_PLAY_SOUNDS
        #include <mmsystem.h>
    #endif
#endif

#ifdef REGEX_PCRE
    // Statically link pcre on Windows
    #if defined(WIN32CONSOLE) || defined(WIN32TILES) || defined(DOS)
        #define PCRE_STATIC
    #endif

    #include <pcre.h>
#endif

#ifdef REGEX_POSIX
    // Do we still need to include sys/types.h?
    #include <sys/types.h>
    #include <regex.h>
#endif

description_level_type description_type_by_name(const char *desc)
{
    if (!desc)
        return DESC_PLAIN;

    if (!strcmp("The", desc))
        return DESC_CAP_THE;
    else if (!strcmp("the", desc))
        return DESC_NOCAP_THE;
    else if (!strcmp("A", desc))
        return DESC_CAP_A;
    else if (!strcmp("a", desc))
        return DESC_NOCAP_A;
    else if (!strcmp("Your", desc))
        return DESC_CAP_YOUR;
    else if (!strcmp("your", desc))
        return DESC_NOCAP_YOUR;
    else if (!strcmp("its", desc))
        return DESC_NOCAP_ITS;
    else if (!strcmp("worn", desc))
        return DESC_INVENTORY_EQUIP;
    else if (!strcmp("inv", desc))
        return DESC_INVENTORY;
    else if (!strcmp("none", desc))
        return DESC_NONE;

    return DESC_PLAIN;
}

std::string number_to_string(unsigned number, bool in_words)
{
    return (in_words? number_in_words(number) : make_stringf("%u", number));
}

std::string apply_description(description_level_type desc,
                              const std::string &name,
                              int quantity, bool in_words)
{
    switch (desc)
    {
    case DESC_CAP_THE:
        return ("The " + name);
    case DESC_NOCAP_THE:
        return ("the " + name);
    case DESC_CAP_A:
        return (quantity > 1 ? number_to_string(quantity, in_words) + name
                             : article_a(name, false));
    case DESC_NOCAP_A:
        return (quantity > 1 ? number_to_string(quantity, in_words) + name
                             : article_a(name, true));
    case DESC_CAP_YOUR:
        return ("Your " + name);
    case DESC_NOCAP_YOUR:
        return ("your " + name);
    case DESC_PLAIN:
    default:
        return (name);
    }
}

// Should return true if the filename contains nothing that
// the shell can do damage with.
bool shell_safe(const char *file)
{
    int match = strcspn(file, "\\`$*?|><&\n!;");
    return (match < 0 || !file[match]);
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
    return replace_all_of(s, " .&`\"\'|;{}()[]<>*%$#@!~?", "");
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

std::string upcase_first(std::string s)
{
    if (!s.empty())
        s[0] = toupper(s[0]);
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

int ends_with(const std::string &s, const char *suffixes[])
{
    if (!suffixes)
        return (0);

    for (int i = 0; suffixes[i]; ++i)
        if (ends_with(s, suffixes[i]))
            return (1 + i);

    return (0);
}

// Returns true if s contains tag 'tag', and strips out tag from s.
bool strip_tag(std::string &s, const std::string &tag, bool skip_padding)
{
    if (s == tag)
    {
        s.clear();
        return (true);
    }

    std::string::size_type pos;

    if (skip_padding)
    {
        if ((pos = s.find(tag)) != std::string::npos)
        {
            s.erase(pos, tag.length());
            trim_string(s);
            return (true);
        }
        return (false);
    }

    if ((pos = s.find(" " + tag + " ")) != std::string::npos)
    {
        // Leave one space intact.
        s.erase(pos, tag.length() + 1);
        trim_string(s);
        return (true);
    }

    if ((pos = s.find(tag + " ")) == 0
        || ((pos = s.find(" " + tag)) != std::string::npos
            && pos + tag.length() + 1 == s.length()))
    {
        s.erase(pos, tag.length() + 1);
        trim_string(s);
        return (true);
    }

    return (false);
}

std::string strip_tag_prefix(std::string &s, const std::string &tagprefix)
{
    std::string::size_type pos = s.find(tagprefix);

    while (pos && pos != std::string::npos && !isspace(s[pos - 1]))
    {
        pos = s.find(tagprefix, pos + 1);
    }

    if (pos == std::string::npos)
        return ("");

    std::string::size_type ns = s.find(" ", pos);
    if (ns == std::string::npos)
        ns = s.length();

    const std::string argument =
        s.substr(pos + tagprefix.length(), ns - pos - tagprefix.length());

    s.erase(pos, ns - pos + 1);
    trim_string(s);

    return (argument);
}

// Get a boolean flag from embedded tags in a string, using "<flag>"
// for true and "no_<flag>" for false. If neither tag is found,
// returns the default value.
bool strip_bool_tag(std::string &s, const std::string &name, bool defval)
{
    if (strip_tag(s, name))
        return (true);
    if (strip_tag(s, "no_" + name))
        return (false);
    return (defval);
}

int strip_number_tag(std::string &s, const std::string &tagprefix)
{
    const std::string num = strip_tag_prefix(s, tagprefix);
    return (num.empty()? TAG_UNFOUND : atoi(num.c_str()));
}

// Naively prefix A/an to a noun.
std::string article_a(const std::string &name, bool lowercase)
{
    if (!name.length())
        return name;

    const char *a  = lowercase? "a "  : "A ";
    const char *an = lowercase? "an " : "An ";
    switch (name[0])
    {
        case 'a': case 'e': case 'i': case 'o': case 'u':
        case 'A': case 'E': case 'I': case 'O': case 'U':
            // XXX: Hack
            if (starts_with(name, "one-way"))
                return a + name;
            return an + name;
        default:
            return a + name;
    }
}

const char *standard_plural_qualifiers[] =
{
    " of ", " labeled ", NULL
};

// Pluralises a monster or item name.  This'll need to be updated for
// correctness whenever new monsters/items are added.
std::string pluralise(const std::string &name,
                      const char *qualifiers[],
                      const char *no_qualifier[])
{
    std::string::size_type pos;

    if (qualifiers)
    {
        for (int i = 0; qualifiers[i]; ++i)
            if ((pos = name.find(qualifiers[i])) != std::string::npos
                && !ends_with(name, no_qualifier))
            {
                return pluralise(name.substr(0, pos)) + name.substr(pos);
            }
    }

    if (!name.empty() && name[name.length() - 1] == ')'
        && (pos = name.rfind(" (")) != std::string::npos)
    {
        return (pluralise(name.substr(0, pos)) + name.substr(pos));
    }

    if (ends_with(name, "us"))
    {
        // Fungus, ufetubus, for instance.
        return name.substr(0, name.length() - 2) + "i";
    }
    else if (ends_with(name, "larva") || ends_with(name, "amoeba")
          || ends_with(name, "antenna"))
    {
        // Giant amoebae sounds a little weird, to tell the truth.
        return name + "e";
    }
    else if (ends_with(name, "ex"))
    {
        // Vortex; vortexes is legal, but the classic plural is cooler.
        return name.substr(0, name.length() - 2) + "ices";
    }
    else if (ends_with(name, "cyclops"))
    {
        return name.substr(0, name.length() - 1) + "es";
    }
    else if (ends_with(name, "y"))
    {
        if (name == "y")
            return ("ys");
        // sensibilitiy -> sensibilities
        else if (name[name.length() - 2] == 'i')
            return name.substr(0, name.length() - 1) + "es";
        // day -> days, boy -> boys, etc
        else if (is_vowel(name[name.length() - 2]))
            return name + "s";
        // jelly -> jellies
        else
            return name.substr(0, name.length() - 1) + "ies";
    }
    else if (ends_with(name, "fe"))
    {
        // knife -> knives
        return name.substr(0, name.length() - 2) + "ves";
    }
    else if (ends_with(name, "staff"))
    {
        // staff -> staves
        return name.substr(0, name.length() - 2) + "ves";
    }
    else if (ends_with(name, "f") && !ends_with(name, "ff"))
    {
        // elf -> elves, but not hippogriff -> hippogrives.
        return name.substr(0, name.length() - 1) + "ves";
    }
    else if (ends_with(name, "mage"))
    {
        // mage -> magi
        return name.substr(0, name.length() - 1) + "i";
    }
    else if (ends_with(name, "sheep") || ends_with(name, "manes")
              || ends_with(name, "fish") || ends_with(name, "folk"))
    {
        // Maybe we should generalise 'manes' to ends_with("es")?
        return name;
    }
    else if (ends_with(name, "ch") || ends_with(name, "sh")
             || ends_with(name, "x"))
    {
        // To handle cockroaches and sphinxes, and in case there's some monster
        // ending with sh (except fish, which are caught in the previous check).
        return name + "es";
    }
    else if (ends_with(name, "um"))
    {
        // simulacrum -> simulacra
        return name.substr(0, name.length() - 2) + "a";
    }
    else if (ends_with(name, "efreet"))
    {
        // efreet -> efreeti. Not sure this is correct.
        return name + "i";
    }
    else if (name == "foot")
        return "feet";

    return name + "s";
}

static std::string pow_in_words(int pow)
{
    switch (pow)
    {
    case 0:
        return "";
    case 3:
        return " thousand";
    case 6:
        return " million";
    case 9:
        return " billion";
    case 12:
    default:
        return " trillion";
    }
}

static std::string tens_in_words(unsigned num)
{
    static const char *numbers[] = {
        "", "one", "two", "three", "four", "five", "six", "seven",
        "eight", "nine", "ten", "eleven", "twelve", "thirteen", "fourteen",
        "fifteen", "sixteen", "seventeen", "eighteen", "nineteen"
    };
    static const char *tens[] = {
        "", "", "twenty", "thirty", "forty", "fifty", "sixty", "seventy",
        "eighty", "ninety"
    };

    if (num < 20)
        return numbers[num];

    int ten = num / 10, digit = num % 10;
    return std::string(tens[ten])
             + (digit ? std::string("-") + numbers[digit] : "");
}

static std::string join_strings(const std::string &a, const std::string &b)
{
    if (!a.empty() && !b.empty())
        return (a + " " + b);

    return (a.empty() ? b : a);
}

static std::string hundreds_in_words(unsigned num)
{
    unsigned dreds = num / 100, tens = num % 100;
    std::string sdreds = dreds? tens_in_words(dreds) + " hundred" : "";
    std::string stens  = tens? tens_in_words(tens) : "";
    return join_strings(sdreds, stens);
}

std::string number_in_words(unsigned num, int pow)
{
    if (pow == 12)
        return number_in_words(num, 0) + pow_in_words(pow);

    unsigned thousands = num % 1000, rest = num / 1000;
    if (!rest && !thousands)
        return ("zero");

    return join_strings((rest? number_in_words(rest, pow + 3) : ""),
                        (thousands? hundreds_in_words(thousands)
                                    + pow_in_words(pow)
                                  : ""));
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

std::string &trim_string_right( std::string &str )
{
    str.erase( str.find_last_not_of( " \t\n\r" ) + 1 );
    return (str);
}

static void add_segment( std::vector<std::string> &segs,
                         std::string s,
                         bool trim,
                         bool accept_empty)
{
    if (trim && !s.empty())
        trim_string(s);

    if (accept_empty || !s.empty())
        segs.push_back(s);
}

std::vector<std::string> split_string( const std::string &sep,
                                       std::string s,
                                       bool trim_segments,
                                       bool accept_empty_segments,
                                       int nsplits)
{
    std::vector<std::string> segments;
    int separator_length = sep.length();

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

    timer.tv_sec  = (time / 1000000L);
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

#ifndef USE_TILE
void cgotoxy(int x, int y, int region)
{
    ASSERT(x >= 1);
    ASSERT(y >= 1);
    switch(region)
    {
    case GOTO_MLIST:
        ASSERT(x <= crawl_view.mlistsz.x);
        ASSERT(y <= crawl_view.mlistsz.y);
        gotoxy_sys(x + crawl_view.mlistp.x - 1, y + crawl_view.mlistp.y - 1);
        break;
    case GOTO_STAT:
        ASSERT(x <= crawl_view.hudsz.x);
        ASSERT(y <= crawl_view.hudsz.y);
        gotoxy_sys(x + crawl_view.hudp.x - 1, y + crawl_view.hudp.y - 1);
        break;
    case GOTO_MSG:
        ASSERT(x <= crawl_view.msgsz.x);
        ASSERT(y <= crawl_view.msgsz.y);
        gotoxy_sys(x + crawl_view.msgp.x - 1, y + crawl_view.msgp.y - 1);
        break;
    case GOTO_CRT:
    default:
        gotoxy_sys(x, y);
        break;
    }
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

mouse_mode mouse_control::ms_current_mode = MOUSE_MODE_NORMAL;
