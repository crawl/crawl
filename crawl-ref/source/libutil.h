/*
 *  File:       libutil.h
 *  Summary:    System independent functions
 */

#ifndef LIBUTIL_H
#define LIBUTIL_H

#include "defines.h"
#include "enum.h"
#include <cctype>
#include <string>
#include <vector>
#include <map>

extern const char *standard_plural_qualifiers[];

// Applies a description type to a name, but does not pluralise! You
// must pluralise the name if needed. The quantity is used to prefix the
// name with a quantity if appropriate.
std::string apply_description(description_level_type desc,
                              const std::string &name,
                              int quantity = 1,
                              bool num_in_words = false);

description_level_type description_type_by_name(const char *desc);

std::string &escape_path_spaces(std::string &s);

std::string lowercase_string(std::string s);
std::string &lowercase(std::string &s);
std::string &uppercase(std::string &s);
std::string upcase_first(std::string);

void wait_for_keypress();
bool key_is_escape(int key);

#define CASE_ESCAPE case ESCAPE: case CONTROL('G'): case -1:

// Unscales a fixed-point number, rounding up.
static inline int unscale_round_up(int number, int scale)
{
    return ((number + scale - 1) / scale);
}

// Chinese rod numerals are _not_ digits for our purposes.
static inline int isadigit(int c)
{
    return (c >= '0' && c <= '9');
}

// 'ä' is a letter, but not a valid inv slot/etc.
static inline int isalower(int c)
{
    return (c >= 'a' && c <= 'z');
}

static inline int isaupper(int c)
{
    return (c >= 'A' && c <= 'Z');
}

static inline int isaalpha(int c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static inline int isaalnum(int c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
}

bool ends_with(const std::string &s, const std::string &suffix);

#ifdef UNIX
extern "C" int stricmp(const char *str1, const char *str2);
#endif
size_t strlcpy(char *dst, const char *src, size_t n);

// String "tags"
#define TAG_UNFOUND -20404
bool strip_tag(std::string &s, const std::string &tag, bool nopad = false);
bool strip_suffix(std::string &s, const std::string &suffix);
int strip_number_tag(std::string &s, const std::string &tagprefix);
bool strip_bool_tag(std::string &s, const std::string &name,
                    bool defval = false);
std::string strip_tag_prefix(std::string &s, const std::string &tagprefix);

std::string article_a(const std::string &name, bool lowercase = true);
std::string pluralise(
    const std::string &name,
    const char *stock_plural_quals[] = standard_plural_qualifiers,
    const char *no_of[] = NULL);
std::string apostrophise(const std::string &name);
std::string apostrophise_fixup(const std::string &msg);

std::string number_in_words(unsigned number, int pow = 0);
std::string number_to_string(unsigned number, bool in_words = false);

bool shell_safe(const char *file);

/**
 * Returns 1 + the index of the first suffix that matches the given string,
 * 0 if no suffixes match.
 */
int  ends_with(const std::string &s, const char *suffixes[]);

std::string strip_filename_unsafe_chars(const std::string &s);

std::string vmake_stringf(const char *format, va_list args);
std::string make_stringf(const char *format, ...);

std::string replace_all(std::string s,
                        const std::string &tofind,
                        const std::string &replacement);

std::string replace_all_of(std::string s,
                           const std::string &tofind,
                           const std::string &replacement);

int count_occurrences(const std::string &text, const std::string &searchfor);

void play_sound(const char *file);

std::string &trim_string(std::string &str);
std::string &trim_string_right(std::string &str);
std::string trimmed_string(std::string s);

inline bool starts_with(const std::string &s, const std::string &prefix)
{
    return (s.rfind(prefix, 0) != std::string::npos);
}

inline bool ends_with(const std::string &s, const std::string &suffix)
{
    if (s.length() < suffix.length())
        return false;
    return (s.find(suffix, s.length() - suffix.length()) != std::string::npos);
}

// Splits string 's' on the separator 'sep'. If trim == true, trims each
// segment. If accept_empties == true, accepts empty segments. If nsplits >= 0,
// splits on the first nsplits occurrences of the separator, and stores the
// remainder of the string as the last segment; negative values of nsplits
// split on all occurrences of the separator.
std::vector<std::string> split_string(
    const std::string &sep,
    std::string s,
    bool trim = true,
    bool accept_empties = false,
    int nsplits = -1);

inline std::string lowercase_first(std::string s)
{
    if (s.length())
        s[0] = tolower(s[0]);
    return (s);
}

inline std::string uppercase_first(std::string s)
{
    if (s.length())
        s[0] = toupper(s[0]);
    return (s);
}

template <typename Z>
std::string comma_separated_line(Z start, Z end,
                                 const std::string &andc = " and ",
                                 const std::string &comma = ", ")
{
    std::string text;
    for (Z i = start; i != end; ++i)
    {
        if (i != start)
        {
            Z tmp = i;
            if (++tmp != end)
                text += comma;
            else
                text += andc;
        }

        text += *i;
    }
    return (text);
}

#ifdef NEED_USLEEP
void usleep(unsigned long time);
#endif

#ifndef USE_TILE
coord_def cgettopleft(GotoRegion region = GOTO_CRT);
coord_def cgetpos(GotoRegion region = GOTO_CRT);
void cgotoxy(int x, int y, GotoRegion region = GOTO_CRT);
GotoRegion get_cursor_region();
#endif
coord_def cgetsize(GotoRegion region = GOTO_CRT);
void cscroll(int n, GotoRegion region);

#ifdef TARGET_OS_WINDOWS
int get_taskbar_height();
#endif

class mouse_control
{
public:
    mouse_control(mouse_mode mode)
    {
        m_previous_mode = ms_current_mode;
        ms_current_mode = mode;
    }

    ~mouse_control()
    {
        ms_current_mode = m_previous_mode;
    }

    static mouse_mode current_mode() { return ms_current_mode; }

private:
    mouse_mode m_previous_mode;
    static mouse_mode ms_current_mode;
};

#endif
