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

bool ends_with(const std::string &s, const std::string &suffix);

#ifdef UNIX
extern "C" int stricmp(const char *str1, const char *str2);
#endif

// String "tags"
#define TAG_UNFOUND -20404
bool strip_tag(std::string &s, const std::string &tag, bool nopad = false);
int strip_number_tag(std::string &s, const std::string &tagprefix);
bool strip_bool_tag(std::string &s, const std::string &name,
                    bool defval = false);
std::string strip_tag_prefix(std::string &s, const std::string &tagprefix);

std::string article_a(const std::string &name, bool lowercase = true);
std::string pluralise(
    const std::string &name,
    const char *stock_plural_quals[] = standard_plural_qualifiers,
    const char *no_of[] = NULL);

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

// Pattern matching
void *compile_pattern(const char *pattern, bool ignore_case = false);
void free_compiled_pattern(void *cp);
bool pattern_match(void *compiled_pattern, const char *text, int length);

// Globs are always available.
void *compile_glob_pattern(const char *pattern, bool ignore_case = false);
void free_compiled_glob_pattern(void *cp);
bool glob_pattern_match(void *compiled_pattern, const char *text, int length);

typedef void *(*p_compile)(const char *pattern, bool ignore_case);
typedef void (*p_free)(void *cp);
typedef bool (*p_match)(void *compiled_pattern, const char *text, int length);

std::string &trim_string( std::string &str );
std::string &trim_string_right( std::string &str );
std::string trimmed_string( std::string s );

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
void usleep( unsigned long time );
#endif

#ifndef USE_TILE
void cgotoxy(int x, int y, GotoRegion region = GOTO_CRT);
GotoRegion get_cursor_region();
#endif

template <typename T>
class unwind_var
{
public:
    unwind_var(T &val_, T newval, T reset_to) : val(val_), oldval(reset_to)
    {
        val = newval;
    }
    unwind_var(T &val_, T newval) : val(val_), oldval(val_)
    {
        val = newval;
    }
    unwind_var(T &val_) : val(val_), oldval(val_) { }
    ~unwind_var()
    {
        val = oldval;
    }

    T value() const
    {
        return val;
    }

    T original_value() const
    {
        return oldval;
    }

private:
    T &val;
    T oldval;
};

typedef unwind_var<bool> unwind_bool;

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
