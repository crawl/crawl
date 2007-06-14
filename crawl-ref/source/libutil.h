/*
 *  File:       libutil.h
 *  Summary:    System indepentant functions 
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 *
 *  Change History (most recent first):
 *
 *      <1>   2001/Nov/01        BWR     Created
 *
 */

#ifndef LIBUTIL_H
#define LIBUTIL_H

#include "AppHdr.h"
#include "defines.h"
#include <cctype>
#include <string>
#include <vector>

void cursorxy(int x, int y);

// Converts a key to a direction key, converting keypad and other sequences
// to vi key sequences (shifted/control key directions are also handled). Non
// direction keys (hopefully) pass through unmangled.
int unmangle_direction_keys(int keyin, int keymap = 0,
                            bool fake_ctrl = true, bool fake_shift = true);

std::string lowercase_string(std::string s);
std::string &lowercase(std::string &s);
std::string &uppercase(std::string &s);
bool ends_with(const std::string &s, const std::string &suffix);
std::string pluralise(const std::string &name, 
                      const char *no_of[] = NULL);

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

// getch() that returns a consistent set of values for all platforms.
int c_getch();

void play_sound(const char *file);

// Pattern matching
void *compile_pattern(const char *pattern, bool ignore_case = false);
void free_compiled_pattern(void *cp);
bool pattern_match(void *compiled_pattern, const char *text, int length);

void get_input_line( char *const buff, int len );

class input_history;

// In view.cc, declared here for default argument to cancelable_get_line()
int get_number_of_cols(void);

// Returns zero if user entered text and pressed Enter, otherwise returns the 
// key pressed that caused the exit, usually Escape.
//
// If keyproc is provided, it must return 1 for normal processing, 0 to exit
// normally (pretend the user pressed Enter), or -1 to exit as if the user
// pressed Escape
int cancelable_get_line( char *buf, 
                         int len, 
                         int wrapcol = get_number_of_cols(),
                         input_history *mh = NULL,
                         int (*keyproc)(int &c) = NULL );

std::string & trim_string( std::string &str );
std::string trimmed_string( std::string s );

// Splits string 's' on the separator 'sep'. If trim == true, trims each
// segment. If accept_empties == true, accepts empty segments. If nsplits >= 0,
// splits on the first nsplits occurrences of the separator, and stores the
// remainder of the string as the last segment; negative values of nsplits
// split on all occurrences of the separator.
std::vector<std::string> split_string(
        const char *sep, 
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

// Keys that getch() must return for keys Crawl is interested in.
enum KEYS
{
    CK_ENTER  = '\r',
    CK_BKSP   = 8,
    CK_ESCAPE = ESCAPE,

    // 128 is off-limits because it's the code that's used when running
    CK_DELETE = 129,

    // This sequence of enums should not be rearranged.
    CK_UP,
    CK_DOWN,
    CK_LEFT,
    CK_RIGHT,

    CK_INSERT,

    CK_HOME,
    CK_END,
    CK_CLEAR,

    CK_PGUP,
    CK_PGDN,

    CK_SHIFT_UP,
    CK_SHIFT_DOWN,
    CK_SHIFT_LEFT,
    CK_SHIFT_RIGHT,

    CK_SHIFT_INSERT,

    CK_SHIFT_HOME,
    CK_SHIFT_END,
    CK_SHIFT_CLEAR,

    CK_SHIFT_PGUP,
    CK_SHIFT_PGDN,

    CK_CTRL_UP,
    CK_CTRL_DOWN,
    CK_CTRL_LEFT,
    CK_CTRL_RIGHT,

    CK_CTRL_INSERT,

    CK_CTRL_HOME,
    CK_CTRL_END,
    CK_CTRL_CLEAR,

    CK_CTRL_PGUP,
    CK_CTRL_PGDN
};

// Sets a boolean to a new value in the scope of the object instance.
class unwind_bool
{
public:
    unwind_bool(bool &val_, bool newval) : val(val_), oldval(val_)
    {
        val = newval;
    }
    ~unwind_bool()
    {
        val = oldval;
    }
private:
    bool &val;
    bool oldval;
};

class cursor_control
{
public:
    cursor_control(bool cursor_enabled) 
        : cstate(is_cursor_enabled()), smartcstate(is_smart_cursor_enabled())
    {
        enable_smart_cursor(false);
        set_cursor_enabled(cursor_enabled);
    }
    ~cursor_control() {
        set_cursor_enabled(cstate);
        enable_smart_cursor(smartcstate);
    }
private:
    bool cstate;
    bool smartcstate;
};

// Reads lines of text; used internally by cancelable_get_line.
class line_reader
{
public:
    line_reader(char *buffer, size_t bufsz, 
                int wrap_col = get_number_of_cols());

    typedef int (*keyproc)(int &key);

    int read_line(bool clear_previous = true);

    std::string get_text() const;

    void set_input_history(input_history *ih);
    void set_keyproc(keyproc fn);

protected:
    void cursorto(int newcpos);
    int process_key(int ch);
    void backspace();
    void killword();

    bool is_wordchar(int c);

private:
    char            *buffer;
    size_t          bufsz;
    input_history   *history;
    int             start_x, start_y;
    keyproc         keyfn;
    int             wrapcol;

    // These are subject to change during editing.
    char            *cur;
    int             length;
    int             pos;
};

class base_pattern
{
public:
    virtual ~base_pattern() { };

    virtual bool valid() const = 0;
    virtual bool matches(const std::string &s) const = 0;
};

class text_pattern : public base_pattern
{
public:
    text_pattern(const std::string &s, bool icase = false) 
        : pattern(s), compiled_pattern(NULL),
          isvalid(true), ignore_case(icase)
    {
    }

    text_pattern()
        : pattern(), compiled_pattern(NULL),
         isvalid(false), ignore_case(false)
    {
    }

    text_pattern(const text_pattern &tp)
        : base_pattern(tp),
          pattern(tp.pattern),
          compiled_pattern(NULL),
          isvalid(tp.isvalid),
          ignore_case(tp.ignore_case)
    {
    }

    ~text_pattern()
    {
        if (compiled_pattern)
            free_compiled_pattern(compiled_pattern);
    }

    const text_pattern &operator= (const text_pattern &tp)
    {
        if (this == &tp)
            return tp;

        if (compiled_pattern)
            free_compiled_pattern(compiled_pattern);
        pattern = tp.pattern;
        compiled_pattern = NULL;
        isvalid      = tp.isvalid;
        ignore_case  = tp.ignore_case;
        return *this;
    }

    const text_pattern &operator= (const std::string &spattern)
    {
        if (pattern == spattern)
            return *this;

        if (compiled_pattern)
            free_compiled_pattern(compiled_pattern);
        pattern = spattern;
        compiled_pattern = NULL;
        isvalid = true;
        // We don't change ignore_case
        return *this;
    }

    bool compile() const
    {
        return !empty()?
            !!(compiled_pattern = compile_pattern(pattern.c_str(), ignore_case))
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
        return valid() && pattern_match(compiled_pattern, s, length);
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


#endif
