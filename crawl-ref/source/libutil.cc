/*
 *  File:       libutil.cc
 *  Summary:    Functions that may be missing from some systems
 */

#include "AppHdr.h"

#include "defines.h"
#include "itemname.h" // is_vowel()
#include "libutil.h"
#include "externs.h"
#include "macro.h"
#include "message.h"
#include "stuff.h"
#include "viewgeom.h"

#include <sstream>
#include <stdio.h>
#include <ctype.h>
#include <stdarg.h>
#include <string.h>

#ifdef TARGET_OS_WINDOWS
    #undef ARRAYSZ
    #include <windows.h>
    #undef max

    #ifdef WINMM_PLAY_SOUNDS
        #include <mmsystem.h>
    #endif
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
    else if (!strcmp("base", desc))
        return DESC_BASENAME;
    else if (!strcmp("qual", desc))
        return DESC_QUALNAME;

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

void play_sound(const char *file)
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

std::string vmake_stringf(const char* s, va_list args)
{
    char buf1[8000];
    va_list orig_args;
    va_copy(orig_args, args);
    size_t len = vsnprintf(buf1, sizeof buf1, s, orig_args);
    va_end(orig_args);
    if (len < sizeof buf1)
        return (buf1);

    char *buf2 = (char*)malloc(len + 1);
    va_copy(orig_args, args);
    vsnprintf(buf2, len + 1, s, orig_args);
    va_end(orig_args);
    std::string ret(buf2);
    free(buf2);

    return (ret);
}

std::string make_stringf(const char *s, ...)
{
    va_list args;
    va_start(args, s);
    std::string ret = vmake_stringf(s, args);
    va_end(args);
    return ret;
}

std::string &escape_path_spaces(std::string &s)
{
    std::string result;
    result.clear();
#ifdef UNIX
    for (const char* ch = s.c_str(); *ch != '\0'; ++ch)
    {
        if (*ch == ' ')
        {
            result += '\\';
        }
        result += *ch;
    }
#elif defined(TARGET_OS_WINDOWS)
    if (s.find(" ") != std::string::npos
        && s.find("\"") == std::string::npos)
    {
        result = "\"" + s + "\"";
    }
    else
    {
        return s;
    }
#else
    // Not implemented for this platform.  Assume that escaping isn't
    // necessary.
    return s;
#endif
    s = result;
    return s;
}

void wait_for_keypress()
{
    // Double getchm() was necessary if first call returned zero; this
    // should theoretically be needed only for DOS.
    getchm() || getchm();
}

bool key_is_escape(int key)
{
    switch (key)
    {
    CASE_ESCAPE
        return (true);
    default:
        return (false);
    }
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

#ifdef UNIX
extern "C" int stricmp(const char *str1, const char *str2)
{
    int ret = 0;

    // No need to check for *str1.  If str1 ends, then tolower(*str1) will be
    // 0, ret will be -1, and the loop will break.
    while (!ret && *str2)
    {
        unsigned char c1 = tolower(*str1);
        unsigned char c2 = tolower(*str2);

        ret = c1 - c2;
        str1++;
        str2++;
    }

    if (ret < 0)
        ret = -1;
    else if (ret > 0)
        ret = 1;

    return (ret);
}
#endif

bool strip_suffix(std::string &s, const std::string &suffix)
{
    if (ends_with(s, suffix))
    {
        s.erase(s.length() - suffix.length(), suffix.length());
        trim_string(s);
        return (true);
    }
    return false;
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
            if (starts_with(name, "one-"))
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
    else if (ends_with(name, "s"))
    {
        return name;
    }
    else if (ends_with(name, "y"))
    {
        if (name == "y")
            return ("ys");
        // sensibility -> sensibilities
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
    else if (ends_with(name, "sheep") || ends_with(name, "fish")
             || ends_with(name, "folk") || ends_with(name, "shedu")
             // "shedu" is male, "lammasu" is female of the same creature
             || ends_with(name, "lammasu") || ends_with(name, "lamassu"))
    {
        return name;
    }
    else if (ends_with(name, "ch") || ends_with(name, "sh")
             || ends_with(name, "x"))
    {
        // To handle cockroaches and sphinxes, and in case there's some monster
        // ending with sh (except fish, which are caught in the previous check).
        return name + "es";
    }
    else if (ends_with(name, "simulacrum"))
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
    else if (name == "ophan" || name == "cherub")
    {
        // unlike "angel" which is fully assimilated and "cherub" which may be
        // pluralized both ways, "ophan" always uses hebrew pluralization
        return name + "im";
    }

    return name + "s";
}

std::string apostrophise(const std::string &name)
{
    if (name.empty())
        return (name);

    if (name == "it" || name == "It")
        return (name + "s");

    const char lastc = name[name.length() - 1];
    return (name + (lastc == 's' ? "'" : "'s"));
}

std::string apostrophise_fixup(const std::string &msg)
{
    if (msg.empty())
        return (msg);

    // XXX: This is rather hackish.
    return (replace_all(msg, "s's", "s'"));
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
        s.replace(found, find.length(), repl);
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
        s.replace(found, 1, replacement);
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

std::string trimmed_string(std::string s)
{
    trim_string(s);
    return (s);
}

// also used with macros
std::string &trim_string(std::string &str)
{
    str.erase(0, str.find_first_not_of(" \t\n\r"));
    str.erase(str.find_last_not_of(" \t\n\r") + 1);

    return (str);
}

std::string &trim_string_right(std::string &str)
{
    str.erase(str.find_last_not_of(" \t\n\r") + 1);
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

std::vector<std::string> split_string(const std::string &sep,
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

# ifdef TARGET_OS_WINDOWS
void usleep(unsigned long time)
{
    ASSERT(time > 0);
    ASSERT(!(time % 1000));
    Sleep(time/1000);
}
# else

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
# endif
#endif

#ifndef USE_TILE
coord_def cgettopleft(GotoRegion region)
{
    switch (region)
    {
    case GOTO_MLIST:
        return crawl_view.mlistp;
    case GOTO_STAT:
        return crawl_view.hudp;
    case GOTO_MSG:
        return crawl_view.msgp;
    case GOTO_DNGN:
        return crawl_view.viewp;
    case GOTO_CRT:
    default:
        return crawl_view.termp;
    }
}

coord_def cgetpos(GotoRegion region)
{
    const coord_def where = coord_def(wherex(), wherey());
    return (where - cgettopleft(region) + coord_def(1, 1));
}

static GotoRegion _current_region = GOTO_CRT;

void cgotoxy(int x, int y, GotoRegion region)
{
    _current_region = region;
    const coord_def tl = cgettopleft(region);
    const coord_def sz = cgetsize(region);

    ASSERT_SAVE(x >= 1 && x <= sz.x);
    ASSERT_SAVE(y >= 1 && y <= sz.y);

    gotoxy_sys(tl.x + x - 1, tl.y + y - 1);
}

GotoRegion get_cursor_region()
{
    return (_current_region);
}
#endif // USE_TILE

coord_def cgetsize(GotoRegion region)
{
    switch (region)
    {
    case GOTO_MLIST:
        return crawl_view.mlistsz;
    case GOTO_STAT:
        return crawl_view.hudsz;
    case GOTO_MSG:
        return crawl_view.msgsz;
    case GOTO_DNGN:
        return crawl_view.viewsz;
    case GOTO_CRT:
    default:
        return crawl_view.termsz;
    }
}

void cscroll(int n, GotoRegion region)
{
    // only implemented for the message window right now
    if (region == GOTO_MSG)
        scroll_message_window(n);
}


mouse_mode mouse_control::ms_current_mode = MOUSE_MODE_NORMAL;

size_t strlcpy(char *dst, const char *src, size_t n)
{
    if (!n)
        return strlen(src);

    const char *s = src;

    while (--n > 0)
        if (!(*dst++ = *s++))
            break;

    if (!n)
    {
        *dst++ = 0;
        while (*s++)
            ;
    }

    return s - src - 1;
}

#ifdef TARGET_OS_WINDOWS
int get_taskbar_height()
{
    RECT rect;
    HWND taskbar = FindWindow("Shell_traywnd", NULL);
    if(taskbar && GetWindowRect(taskbar, &rect)) {
        return rect.bottom - rect.top;
    }
    return 0;
}
#endif
