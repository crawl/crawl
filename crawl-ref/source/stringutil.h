/**
 * @file
 * @brief String manipulation functions that don't fit elsewhere.
 **/

#ifndef STRINGS_H
#define STRINGS_H

#include "config.h"

#ifdef CRAWL_HAVE_STRLCPY
#include <cstring>
#else
size_t strlcpy(char *dst, const char *src, size_t n);
#endif

string lowercase_string(const string s);
string &lowercase(string &s);
string &uppercase(string &s);
string uppercase_string(string s);
string lowercase_first(string);
string uppercase_first(string);

/**
 * Returns 1 + the index of the first suffix that matches the given string,
 * 0 if no suffixes match.
 */
int ends_with(const string &s, const char *suffixes[]);

string wordwrap_line(string &s, int cols, bool tags = false,
                     bool indent = false);

string strip_filename_unsafe_chars(const string &s);

string vmake_stringf(const char *format, va_list args);
string make_stringf(PRINTF(0, ));

bool strip_suffix(string &s, const string &suffix);

string replace_all(string s, const string &tofind, const string &replacement);

string replace_all_of(string s, const string &tofind, const string &replacement);

string maybe_capitalise_substring(string s);
string maybe_pick_random_substring(string s);

int count_occurrences(const string &text, const string &searchfor);

string &trim_string(string &str);
string &trim_string_right(string &str);
string trimmed_string(string s);

/**
 * Find the enumerator e between begin and end that satisfies pred(e) and
 * whose name, as given by namefunc(e), has the earliest occurrence of the
 * substring spec.
 *
 * @tparam Enum     An integer-like or C-style enum type no larger than
 *                  size_t. More specifically, Enum must be implicitly
 *                  convertible to size_t, and must be explicitly convertible
 *                  from size_t with static_cast. There should be no gaps
 *                  in enumerator values between begin and end.
 *
 * @param spec      The substring to search for.
 * @param begin     The beginning of the enumerator range to search in.
 * @param end       One past the end of the enum range to search in.
 * @param pred      A function from Enum to bool. Enumerators that do not
 *                  satisfy the predicate are ignored.
 * @param namefunc  A function from Enum to string or const char * giving
 *                  the name of the enumerator.
 * @return The enumerator that satisfies pred and whose name contains the
 *         spec substring beginning at the earliest position. If no such
 *         enumerator exists, returns end. If there are multiple strings
 *         containing the spec as a prefix, returns the shortest such string
 *         (so exact matches are preferred); otherwise ties are broken in
 *         an unspecified manner.
 */
template<class Enum, class Pred, class NameFunc>
Enum find_earliest_match(const string &spec, Enum begin, Enum end,
                         Pred pred, NameFunc namefunc)
{
    Enum selected = end;
    const size_t speclen = spec.length();
    size_t bestpos = string::npos;
    size_t bestlen = string::npos;
    for (size_t i = begin; i < (size_t) end; ++i)
    {
        const Enum curr = static_cast<Enum>(i);

        if (!pred(curr))
            continue;

        const string name = lowercase_string(namefunc(curr));
        const size_t pos = name.find(spec);
        const size_t len = name.length();

        if (pos < bestpos || pos == 0 && len < bestlen)
        {
            // Exit early if we found an exact match.
            if (pos == 0 && len == speclen)
                return curr;

            // npos is never less than bestpos, so the spec was found.
            bestpos = pos;
            if (pos == 0)
                bestlen = len;
            selected = curr;
        }
    }
    return selected;
}

template <typename Z, typename F>
string comma_separated_fn(Z start, Z end, F stringify,
                          const string &andc = " and ",
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

        text += stringify(*i);
    }
    return text;
}

template <typename Z>
string comma_separated_line(Z start, Z end, const string &andc = " and ",
                            const string &comma = ", ")
{
    return comma_separated_fn(start, end, [] (const string &s) { return s; },
                              andc, comma);
}

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

// time

string make_time_string(time_t abs_time, bool terse = false);
string make_file_time(time_t when);

#endif
