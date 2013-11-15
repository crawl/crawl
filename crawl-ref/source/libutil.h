/**
 * @file
 * @brief System independent functions
**/

#ifndef LIBUTIL_H
#define LIBUTIL_H

#include "enum.h"
#include <cctype>
#include <string>
#include <vector>
#include <map>

extern const char *standard_plural_qualifiers[];

// Applies a description type to a name, but does not pluralise! You
// must pluralise the name if needed. The quantity is used to prefix the
// name with a quantity if appropriate.
string apply_description(description_level_type desc, const string &name,
                         int quantity = 1, bool num_in_words = false);

description_level_type description_type_by_name(const char *desc);

string lowercase_string(string s);
string &lowercase(string &s);
string &uppercase(string &s);
string uppercase_first(string);
string lowercase_first(string);

bool key_is_escape(int key);

#define CASE_ESCAPE case ESCAPE: case CONTROL('G'): case -1:

// Unscales a fixed-point number, rounding up.
static inline int unscale_round_up(int number, int scale)
{
    return (number + scale - 1) / scale;
}

// Chinese rod numerals are _not_ digits for our purposes.
static inline bool isadigit(int c)
{
    return c >= '0' && c <= '9';
}

// 'ä' is a letter, but not a valid inv slot/etc.
static inline bool isalower(int c)
{
    return c >= 'a' && c <= 'z';
}

static inline bool isaupper(int c)
{
    return c >= 'A' && c <= 'Z';
}

static inline bool isaalpha(int c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static inline bool isaalnum(int c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
}

static inline ucs_t toalower(ucs_t c)
{
    return isaupper(c) ? c + 'a' - 'A' : c;
}

int numcmp(const char *a, const char *b, int limit = 0);
bool numcmpstr(string a, string b);
size_t strlcpy(char *dst, const char *src, size_t n);

int strwidth(const char *s);
int strwidth(const string &s);
string chop_string(const char *s, int width, bool spaces = true);
string chop_string(const string &s, int width, bool spaces = true);
string wordwrap_line(string &s, int cols, bool tags = false,
                     bool indent = false);

bool version_is_stable(const char *ver);

// String "tags"
#define TAG_UNFOUND -20404
bool strip_tag(string &s, const string &tag, bool nopad = false);
bool strip_suffix(string &s, const string &suffix);
int strip_number_tag(string &s, const string &tagprefix);
vector<string> strip_multiple_tag_prefix(string &s, const string &tagprefix);
string strip_tag_prefix(string &s, const string &tagprefix);
bool parse_int(const char *s, int &i);

string article_a(const string &name, bool lowercase = true);
string pluralise(const string &name,
                 const char *stock_plural_quals[] = standard_plural_qualifiers,
                 const char *no_of[] = NULL);
string apostrophise(const string &name);
string apostrophise_fixup(const string &msg);

string number_in_words(unsigned number, int pow = 0);

bool shell_safe(const char *file);

/**
 * Returns 1 + the index of the first suffix that matches the given string,
 * 0 if no suffixes match.
 */
int ends_with(const string &s, const char *suffixes[]);

string strip_filename_unsafe_chars(const string &s);

string vmake_stringf(const char *format, va_list args);
string make_stringf(PRINTF(0, ));

string replace_all(string s, const string &tofind, const string &replacement);

string replace_all_of(string s, const string &tofind, const string &replacement);

string maybe_capitalise_substring(string s);
string maybe_pick_random_substring(string s);

int count_occurrences(const string &text, const string &searchfor);

void play_sound(const char *file);

string &trim_string(string &str);
string &trim_string_right(string &str);
string trimmed_string(string s);

static inline bool starts_with(const string &s, const string &prefix)
{
    return s.rfind(prefix, 0) != string::npos;
}

static inline bool ends_with(const string &s, const string &suffix)
{
    if (s.length() < suffix.length())
        return false;
    return s.find(suffix, s.length() - suffix.length()) != string::npos;
}

// Splits string 's' on the separator 'sep'. If trim == true, trims each
// segment. If accept_empties == true, accepts empty segments. If nsplits >= 0,
// splits on the first nsplits occurrences of the separator, and stores the
// remainder of the string as the last segment; negative values of nsplits
// split on all occurrences of the separator.
vector<string> split_string(const string &sep, string s, bool trim = true,
                            bool accept_empties = false, int nsplits = -1);

template <typename Z>
string comma_separated_line(Z start, Z end, const string &andc = " and ",
                            const string &comma = ", ")
{
    string text;
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
    return text;
}

string unwrap_desc(string desc);

template <typename Z>
void erase_any(vector<Z> &vec, unsigned long which)
{
    if (which != vec.size() - 1)
        vec[which] = vec[vec.size() - 1];
    vec.pop_back();
}

template <typename Z>
static inline void swapv(Z &a, Z &b)
{
    Z tmp = a;
    a = b;
    b = tmp;
}

static inline int sqr(int x)
{
    return x * x;
}

unsigned int isqrt(unsigned int x);
int isqrt_ceil(int x);

static inline bool testbits(uint64_t flags, uint64_t test)
{
    return (flags & test) == test;
}

coord_def cgetsize(GotoRegion region = GOTO_CRT);
void cscroll(int n, GotoRegion region);

string untag_tiles_console(string s);

#ifdef TARGET_OS_WINDOWS
enum taskbar_pos
{
    TASKBAR_NO      = 0x00,
    TASKBAR_BOTTOM  = 0x01,
    TASKBAR_TOP     = 0x02,
    TASKBAR_LEFT    = 0x04,
    TASKBAR_RIGHT   = 0x08,
    TASKBAR_H       = 0x03,
    TASKBAR_V       = 0x0C
};

int get_taskbar_size();
taskbar_pos get_taskbar_pos();
void text_popup(const string& text, const wchar_t *caption);
#endif

class mouse_control
{
public:
    mouse_control(mouse_mode mode)
    {
        m_previous_mode = ms_current_mode;
        ms_current_mode = mode;

#ifdef USE_TILE_WEB
        if (m_previous_mode != ms_current_mode)
            tiles.update_input_mode(mode);
#endif
    }

    ~mouse_control()
    {
#ifdef USE_TILE_WEB
        if (m_previous_mode != ms_current_mode)
            tiles.update_input_mode(m_previous_mode);
#endif
        ms_current_mode = m_previous_mode;
    }

    static mouse_mode current_mode() { return ms_current_mode; }

private:
    mouse_mode m_previous_mode;
    static mouse_mode ms_current_mode;
};

void init_signals();
void release_cli_signals();
#endif
