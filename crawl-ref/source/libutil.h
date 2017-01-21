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

static inline char32_t toalower(char32_t c)
{
    return isaupper(c) ? c + 'a' - 'A' : c;
}

// Same thing with signed int, so we can pass though -1 undisturbed.
static inline int toalower(int c)
{
    return isaupper(c) ? c + 'a' - 'A' : c;
}

int numcmp(const char *a, const char *b, int limit = 0);
bool numcmpstr(const string &a, const string &b);

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

#ifdef USE_SOUND
void play_sound(const char *file);
#endif

string unwrap_desc(string desc);

/** Ignore any number of arguments and return true.
 *
 * @return true
 */
template<class ... Ts> bool always_true(Ts ...) { return true; }

/** Remove an element from a vector without preserving order.
 *  The indicated element is replaced by the last element of the vector.
 *
 * @tparam Z The value type of the vector.
 * @param vec The vector to modify.
 * @param which The index of the element to remove.
 */
template <typename Z>
void erase_any(vector<Z> &vec, unsigned long which)
{
    if (which != vec.size() - 1)
        vec[which] = std::move(vec[vec.size() - 1]);
    vec.pop_back();
}

/** A comparator to compare pairs by their second elements.
 *
 * @tparam T  The type of the pairs (not the second element!)
 * @param left,right  The pairs to be compared.
 * @return true if left.second > right.second, false otherwise.
 */
template<typename T>
struct greater_second
{
    bool operator()(const T & left, const T & right)
    {
        return left.second > right.second;
    }
};

/** Delete all the pointers in a container and clear the container.
 *
 * @tparam T The type of the container; must be a container of pointers
 *           to non-arrays.
 * @param collection The container to clear.
 */
template<class T>
static void deleteAll(T& collection)
{
    for (auto ptr : collection)
        delete ptr;
    collection.clear();
}

/** Find a map element by key, and return a pointer to its value.
 *
 * @tparam M The type of the map; may be const.
 * @param map The map in which to do the lookup.
 * @param obj The key to search for.
 * @return a pointer to the value associated with obj if found,
 *         otherwise null. The pointer is const if the map is.
 */
template<class M>
auto map_find(M &map, const typename M::key_type &obj)
    -> decltype(&map.begin()->second)
{
    auto it = map.find(obj);
    return it == map.end() ? nullptr : &it->second;
}

/** Find a map element by key, and return its value or a default.
 *  Only intended for use with simple map values where copying is not
 *  a concern; use map_find if the returned value should not be
 *  copied.
 *
 * @tparam M The type of the map; may be const.
 * @param map The map in which to do the lookup.
 * @param key The key to search for.
 * @param unfound The object to return if the key was not found.
 *
 * @return a copy of the value associated with key if found, otherwise
 *         a copy of unfound.
 */
template<class M>
typename M::mapped_type lookup(M &map, const typename M::key_type &key,
                               const typename M::mapped_type &unfound)
{
    auto it = map.find(key);
    return it == map.end() ? unfound : it->second;
}

// Delete when we upgrade to C++14!
template<typename T, typename... Args>
unique_ptr<T> make_unique(Args&&... args)
{
    return unique_ptr<T>(new T(forward<Args>(args)...));
}

/** Remove from a container all elements matching a predicate.
 *
 * @tparam C the container type. Must be reorderable (not a map or set!),
 *           and must provide an erase() method taking two iterators.
 * @tparam P the predicate type, typically but not necessarily
 *           function<bool (const C::value_type &)>
 * @param container The container to modify.
 * @param pred The predicate to test.
 *
 * @post The container contains only those elements for which pred(elt)
 *       returns false, in the same relative order.
 */
template<class C, class P>
void erase_if(C &container, P pred)
{
    container.erase(remove_if(begin(container), end(container), pred),
                    end(container));
}

/** Remove from a container all elements with a given value.
 *
 * @tparam C the container type. Must be reorderable (not a map or set!),
 *           must provide an erase() method taking two iterators, and
 *           must provide a value_type type member.
 * @param container The container to modify.
 * @param val The value to remove.
 *
 * @post The container contains only those elements not equal to val, in the
 *       in the same relative order.
 */
template<class C>
void erase_val(C &container, const typename C::value_type &val)
{
    container.erase(remove(begin(container), end(container), val),
                    end(container));
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

template<class E, int Exp>
static inline bool testbits(enum_bitfield<E, Exp> flags,
                            enum_bitfield<E, Exp> test)
{
    return (flags & test) == test;
}

template<class E, int Exp>
static inline bool testbits(enum_bitfield<E, Exp> flags, E test)
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
