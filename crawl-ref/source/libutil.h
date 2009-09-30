/*
 *  File:       libutil.h
 *  Summary:    System independent functions
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 */

#ifndef LIBUTIL_H
#define LIBUTIL_H

#include "AppHdr.h"
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

#ifdef NEED_SNPRINTF
int snprintf( char *str, size_t size, const char *format, ... );
#endif

#ifndef USE_TILE
void cgotoxy(int x, int y, int region = GOTO_CRT);
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

class base_pattern
{
public:
    virtual ~base_pattern() { }

    virtual bool valid() const = 0;
    virtual bool matches(const std::string &s) const = 0;
};

template <p_compile pcomp, p_free pfree, p_match pmatch>
class basic_text_pattern : public base_pattern
{
public:
    basic_text_pattern(const std::string &s, bool icase = false)
        : pattern(s), compiled_pattern(NULL),
          isvalid(true), ignore_case(icase)
    {
    }

    basic_text_pattern()
        : pattern(), compiled_pattern(NULL),
         isvalid(false), ignore_case(false)
    {
    }

    basic_text_pattern(const basic_text_pattern &tp)
        : base_pattern(tp),
          pattern(tp.pattern),
          compiled_pattern(NULL),
          isvalid(tp.isvalid),
          ignore_case(tp.ignore_case)
    {
    }

    ~basic_text_pattern()
    {
        if (compiled_pattern)
            pfree(compiled_pattern);
    }

    const basic_text_pattern &operator= (const basic_text_pattern &tp)
    {
        if (this == &tp)
            return tp;

        if (compiled_pattern)
            pfree(compiled_pattern);
        pattern = tp.pattern;
        compiled_pattern = NULL;
        isvalid      = tp.isvalid;
        ignore_case  = tp.ignore_case;
        return *this;
    }

    const basic_text_pattern &operator= (const std::string &spattern)
    {
        if (pattern == spattern)
            return *this;

        if (compiled_pattern)
            pfree(compiled_pattern);
        pattern = spattern;
        compiled_pattern = NULL;
        isvalid = true;
        // We don't change ignore_case
        return *this;
    }

    bool compile() const
    {
        return !empty()?
            !!(compiled_pattern = pcomp(pattern.c_str(), ignore_case))
          : false;
    }

    bool empty() const
    {
        return !pattern.length();
    }

    bool valid() const
    {
        return isvalid &&
            (compiled_pattern || (isvalid = compile()));
    }

    bool matches(const char *s, int length) const
    {
        return valid() && pmatch(compiled_pattern, s, length);
    }

    bool matches(const char *s) const
    {
        return matches(std::string(s));
    }

    bool matches(const std::string &s) const
    {
        return matches(s.c_str(), s.length());
    }

    const std::string &tostring() const
    {
        return pattern;
    }

private:
    std::string pattern;
    mutable void *compiled_pattern;
    mutable bool isvalid;
    bool ignore_case;
};

typedef
basic_text_pattern<compile_pattern,
                   free_compiled_pattern, pattern_match> text_pattern;

typedef
basic_text_pattern<compile_glob_pattern,
                   free_compiled_glob_pattern,
                   glob_pattern_match> glob_pattern;


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
