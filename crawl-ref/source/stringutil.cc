/**
 * @file
 * @brief String manipulation functions that don't fit elsewhere.
 **/

#include "AppHdr.h"

#include "stringutil.h"

#include <cwctype>
#include <sstream>

#include "libutil.h"
#include "random.h"
#include "unicode.h"
#include "mpr.h"

#ifndef CRAWL_HAVE_STRLCPY
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
#endif


string lowercase_string(const string &s)
{
    string res;
    char32_t c;
    char buf[4];
    for (const char *tp = s.c_str(); int len = utf8towc(&c, tp); tp += len)
    {
        // crawl breaks horribly if this is allowed to affect ascii chars,
        // so override locale-specific casing for ascii. (For example, in
        // Turkish; tr_TR.utf8 lowercase I is a dotless i that is not
        // ascii, which breaks many things.)
        if (isaalpha(tp[0]))
            res.append(1, toalower(tp[0]));
        else
            res.append(buf, wctoutf8(buf, towlower(c)));
    }
    return res;
}

string &lowercase(string &s)
{
    s = lowercase_string(s);
    return s;
}

string &uppercase(string &s)
{
    for (char &ch : s)
        ch = toupper_safe(ch);
    return s;
}

string uppercase_string(string s)
{
    return uppercase(s);
}

// Warning: this (and uppercase_first()) relies on no libc (glibc, BSD libc,
// MSVC crt) supporting letters that expand or contract, like German ß (-> SS)
// upon capitalization / lowercasing. This is mostly a fault of the API --
// there's no way to return two characters in one code point.
// Also, all characters must have the same length in bytes before and after
// lowercasing, all platforms currently have this property.
//
// A non-hacky version would be slower for no gain other than sane code; at
// least unless you use some more powerful API.
string lowercase_first(string s)
{
    char32_t c;
    if (!s.empty())
    {
        utf8towc(&c, &s[0]);
        wctoutf8(&s[0], towlower(c));
    }
    return s;
}

string uppercase_first(string s)
{
    // Incorrect due to those pesky Dutch having "ij" as a single letter (wtf?).
    // Too bad, there's no standard function to handle that character, and I
    // don't care enough.
    char32_t c;
    if (!s.empty())
    {
        utf8towc(&c, &s[0]);
        wctoutf8(&s[0], towupper(c));
    }
    return s;
}

int codepoints(string str)
{
    int len = 0;
    for (char c : str)
        if ((c & 0xc0) != 0x80)
            ++len;
    return len;
}

string padded_str(const string &s, int pad_to, bool prepend)
{
    const int padding = pad_to - codepoints(s);
    if (padding <= 0)
        return s;
    string str = s;
    if (prepend)
        str.insert(0, string(padding, ' '));
    else
        str.append(padding, ' ');
    return str;
}

int ends_with(const string &s, const char * const suffixes[])
{
    if (!suffixes)
        return 0;

    for (int i = 0; suffixes[i]; ++i)
        if (ends_with(s, suffixes[i]))
            return 1 + i;

    return 0;
}


static const string _get_indent(const string &s)
{
    size_t prefix = 0;
    if (starts_with(s, "\"")    // ASCII quotes
        || starts_with(s, "“")  // English quotes
        || starts_with(s, "„")  // Polish/German/... quotes
        || starts_with(s, "«")  // French quotes
        || starts_with(s, "»")  // Danish/... quotes
        || starts_with(s, "•")) // bulleted lists
    {
        prefix = 1;
    }
    else if (starts_with(s, "「"))  // Chinese/Japanese quotes
        prefix = 2;

    size_t nspaces = s.find_first_not_of(' ', prefix);
    if (nspaces == string::npos)
        nspaces = 0;
    if (!(prefix += nspaces))
        return "";
    return string(prefix, ' ');
}


// The provided string is consumed!
string wordwrap_line(string &s, int width, bool tags, bool indent, int force_indent)
{
    ASSERT(width > 0);

    const char *cp0 = s.c_str();
    const char *cp = cp0, *space = 0;
    char32_t c;
    bool seen_nonspace = false;

    while (int clen = utf8towc(&c, cp))
    {
        int cw = wcwidth(c);
        if (c == ' ')
        {
            if (seen_nonspace)
                space = cp;
        }
        else if (c == '\n')
        {
            space = cp;
            break;
        }
        else
            seen_nonspace = true;

        if (c == '<' && tags)
        {
            ASSERT(cw == 1);
            if (cp[1] == '<') // "<<" escape
            {
                // Note: this must be after a possible wrap, otherwise we could
                // split the escape between lines.
                cp++;
            }
            else
            {
                cw = 0;
                // Skip the whole tag.
                while (*cp != '>')
                {
                    if (!*cp)
                    {
                        // Everything so far fitted, report error.
                        string ret = s + ">";
                        s = "<lightred>ERROR: string above had unterminated tag</lightred>";
                        return ret;
                    }
                    cp++;
                }
            }
        }

        if (cw > width)
            break;

        if (cw >= 0)
            width -= cw;
        cp += clen;
    }

    if (!c)
    {
        // everything fits
        string ret = s;
        s.clear();
        return ret;
    }

    if (space)
        cp = space;
    const string ret = s.substr(0, cp - cp0);

    const string indentation = (indent && c != '\n' && seen_nonspace)
                                ? (force_indent > 0
                                    ? string(force_indent, ' ')
                                    : _get_indent(s))
                                : "";

    // eat all trailing spaces and up to one newline
    while (*cp == ' ')
        cp++;
    if (*cp == '\n')
        cp++;

#ifdef ASSERTS
    const size_t inputlength = s.length();
#endif
    s.erase(0, cp - cp0);

    // if we had to break a line, reinsert the indentation
    if (indent && c != '\n')
        s = indentation + s;

    // Make sure the remaining string actually shrank, or else we're likely
    // to throw our caller into an infinite loop.
    ASSERT(inputlength > s.length());
    return ret;
}

string strip_filename_unsafe_chars(const string &s)
{
    return replace_all_of(s, " .&`\"\'|;{}()[]<>*%$#@!~?", "");
}

string vmake_stringf(const char* s, va_list args)
{
    char buf1[8000];
    va_list orig_args;
    va_copy(orig_args, args);
    size_t len = vsnprintf(buf1, sizeof buf1, s, orig_args);
    va_end(orig_args);
    if (len < sizeof buf1)
        return buf1;

    char *buf2 = (char*)malloc(len + 1);
    va_copy(orig_args, args);
    vsnprintf(buf2, len + 1, s, orig_args);
    va_end(orig_args);
    string ret(buf2);
    free(buf2);

    return ret;
}

string make_stringf(const char *s, ...)
{
    va_list args;
    va_start(args, s);
    string ret = vmake_stringf(s, args);
    va_end(args);
    return ret;
}

bool strip_suffix(string &s, const string &suffix)
{
    if (ends_with(s, suffix))
    {
        s.erase(s.length() - suffix.length(), suffix.length());
        trim_string(s);
        return true;
    }
    return false;
}

string replace_all(string s, const string &find, const string &repl)
{
    ASSERT(!find.empty());
    string::size_type start = 0;
    string::size_type found;

    while ((found = s.find(find, start)) != string::npos)
    {
        s.replace(found, find.length(), repl);
        start = found + repl.length();
    }

    return s;
}

// Replaces all occurrences of any of the characters in tofind with the
// replacement string.
string replace_all_of(string s, const string &tofind, const string &replacement)
{
    ASSERT(!tofind.empty());
    string::size_type start = 0;
    string::size_type found;

    while ((found = s.find_first_of(tofind, start)) != string::npos)
    {
        s.replace(found, 1, replacement);
        start = found + replacement.length();
    }

    return s;
}

// Capitalise phrases encased in @CAPS@ ... @NOCAPS@. If @NOCAPS@ is
// missing, change the rest of the line to uppercase.
string maybe_capitalise_substring(string s)
{
    string::size_type start = 0;
    while ((start = s.find("@CAPS@", start)) != string::npos)
    {
        string::size_type cap_start  = start + 6;
        string::size_type cap_end    = string::npos;
        string::size_type end        = s.find("@NOCAPS@", cap_start);
        string::size_type length     = string::npos;
        string::size_type cap_length = string::npos;
        if (end != string::npos)
        {
            cap_end = end + 8;
            cap_length = end - cap_start;
            length = cap_end - start;
        }
        string substring = s.substr(cap_start, cap_length);
        trim_string(substring);
        s.replace(start, length, uppercase(substring));
    }
    return s;
}

/**
 * Make @-replacements on the given text.
 *
 * @param text         the string to be processed
 * @param replacements contains information on what replacements are to be made.
 * @returns a string with substitutions based on the arguments. For example, if
 *          given "baz@foo@" and { "foo", "bar" } then this returns "bazbar".
 *          If a string not in replacements is found between @ signs, then the
 *          original, unedited string is returned.
 */
string replace_keys(const string &text, const map<string, string>& replacements)
{
    string::size_type at = 0, last = 0;
    ostringstream res;
    while ((at = text.find('@', last)) != string::npos)
    {
        res << text.substr(last, at - last);
        const string::size_type end = text.find('@', at + 1);
        if (end == string::npos)
            break;

        const string key = text.substr(at + 1, end - at - 1);
        const string* value = map_find(replacements, key);

        if (!value)
            return text;

        res << *value;

        last = end + 1;
    }
    if (!last)
        return text;

    res << text.substr(last);
    return res.str();
}

// For each set of [phrase|term|word] contained in the string, replace the set with a random subphrase.
// NOTE: Doesn't work for nested patterns!
string maybe_pick_random_substring(string s)
{
    string::size_type start = 0;
    while ((start = s.find("[", start)) != string::npos)
    {
        string::size_type end = s.find("]", start);
        if (end == string::npos)
            break;

        string substring = s.substr(start + 1, end - start - 1);
        vector<string> split = split_string("|", substring, false, true);
        int index = random2(split.size());
        s.replace(start, end + 1 - start, split[index]);
    }
    return s;
}

int count_occurrences(const string &text, const string &s)
{
    ASSERT(!s.empty());
    int nfound = 0;
    string::size_type pos = 0;

    while ((pos = text.find(s, pos)) != string::npos)
    {
        ++nfound;
        pos += s.length();
    }

    return nfound;
}

// also used with macros
string &trim_string(string &str)
{
    str.erase(0, str.find_first_not_of(" \t\n\r"));
    str.erase(str.find_last_not_of(" \t\n\r") + 1);

    return str;
}

string &trim_string_right(string &str)
{
    str.erase(str.find_last_not_of(" \t\n\r") + 1);
    return str;
}

string trimmed_string(string s)
{
    trim_string(s);
    return s;
}

static void add_segment(vector<string> &segs, string s, bool trim,
                        bool accept_empty)
{
    if (trim && !s.empty())
        trim_string(s);

    if (accept_empty || !s.empty())
        segs.push_back(s);
}

set<size_t> find_escapes(const string &s)
{
    set<size_t> ret;
    size_t i = 0;
    while ((i = s.find("\\", i)) != string::npos)
        if (i < s.size())
            ret.insert(++i);
    return ret;
}

string deescape(string s, const set<size_t> &escapes)
{
    for (auto i = escapes.rbegin(); i != escapes.rend(); ++i)
        s.erase(*i, 1);
    return s;
}

/// simply remove backslashes protecting any escaped characters. Intended for
/// use after moret interesting escape processing has happened.
string deescape(string s)
{
    set<size_t> escapes = find_escapes(s);
    return deescape(std::move(s), escapes);
}

/**
 * Split a string using separator `sep`
 *
 * @param sep the separator to split on
 * @param s the string to split
 * @param trim_segments if true, trim whitespace on the resulting segments.
 *        Default: true
 * @param accept_empty_segments if true, allow length 0 segments. If false,
 *        the separator characters around empty segments will be consumed, bu
 *        the resulting vector will not include the empty segment
 *        Default: false
 * @param nsplits the maximum number of times to split; the final returned
 *        element will be the remainder. If negative, there is no limit on the
 *        number of splits.
 *        Default: -1
 * @param ignore_escapes if set to true, will interpret a `\` symbol before
 *        any instances of `sep` as escaping those instances. This `\` will be
 *        removed, but no other escapes will be processed (including `\\`).
 *        Default: false
 *
 * @return a vector of the resulting segments after the split
 */
vector<string> split_string(const string &sep, string s, bool trim_segments,
                            bool accept_empty_segments, int nsplits,
                            bool ignore_escapes)
{
    vector<string> segments;
    int separator_length = sep.length();
    set<size_t> escapes; // annoying to have to always create...
    if (ignore_escapes)
        escapes = find_escapes(s);

    size_t pos = 0;
    size_t original_pos = 0; // original position of s[0]
    while (nsplits && (pos = s.find(sep, pos)) != string::npos)
    {
        if (escapes.count(pos + original_pos))
        {
            ASSERT(pos > 0); // should be impossible for char 0 to be escaped
            // erase the backslash
            s.erase(pos - 1, 1);
            original_pos++;
            pos += separator_length - 1;
            continue;
        }
        add_segment(segments, s.substr(0, pos),
                    trim_segments, accept_empty_segments);

        s.erase(0, pos + separator_length);
        original_pos += pos + separator_length;
        pos = 0;

        if (nsplits > 0)
            --nsplits;
    }

    // consume any escaped separators in the remainder; not a very elegant approach
    if (ignore_escapes)
        s = replace_all(s, make_stringf("\\%s", sep.c_str()), sep);
    add_segment(segments, s, trim_segments, accept_empty_segments);

    return segments;
}


// Crude, but functional.
string make_time_string(time_t abs_time, bool terse)
{
    const int days  = abs_time / 86400;
    const int hours = (abs_time % 86400) / 3600;
    const int mins  = (abs_time % 3600) / 60;
    const int secs  = abs_time % 60;

    string buff;
    if (days > 0)
    {
        buff += make_stringf("%d %s ", days, terse ? ","
                             : days > 1 ? "days" : "day");
    }
    return buff + make_stringf("%02d:%02d:%02d", hours, mins, secs);
}

string make_file_time(time_t when)
{
    if (tm *loc = TIME_FN(&when))
    {
        return make_stringf("%04d%02d%02d-%02d%02d%02d",
                            loc->tm_year + 1900,
                            loc->tm_mon + 1,
                            loc->tm_mday,
                            loc->tm_hour,
                            loc->tm_min,
                            loc->tm_sec);
    }
    return "";
}
