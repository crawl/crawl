/**
 * @file
 * @brief System independent functions
**/

#ifndef LIBUTIL_H
#define LIBUTIL_H

#include <cctype>
#include <map>
#include <string>
#include <vector>

#include "enum.h"

bool key_is_escape(int key);

// numeric string functions

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

bool version_is_stable(const char *ver);

// String "tags"
#define TAG_UNFOUND -20404
bool strip_tag(string &s, const string &tag, bool nopad = false);
int strip_number_tag(string &s, const string &tagprefix);
vector<string> strip_multiple_tag_prefix(string &s, const string &tagprefix);
string strip_tag_prefix(string &s, const string &tagprefix);
bool parse_int(const char *s, int &i);

// String 'descriptions'
description_level_type description_type_by_name(const char *desc);

bool shell_safe(const char *file);

void play_sound(const char *file);

string unwrap_desc(string desc);

template<class T> bool _always_true(T) { return true; }

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

// A comparator for pairs.
template<typename T>
struct greater_second
{
    bool operator()(const T & left, const T & right)
    {
        return left.second > right.second;
    }
};

template<class T>
static void deleteAll(T& collection)
{
    for (auto ptr : collection)
        delete ptr;
    collection.clear();
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
string colour_string(string in, int col);

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
