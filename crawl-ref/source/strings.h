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

string lowercase_string(string s);
string &lowercase(string &s);
string &uppercase(string &s);
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
