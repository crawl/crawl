/**
 * @file
 * @brief String manipulation functions that don't fit elsewhere.
 **/

#include "AppHdr.h"

#include "stringutil.h"

#include <cwctype>

#include "random.h"
#include "unicode.h"

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


string lowercase_string(const string s)
{
    string res;
    ucs_t c;
    char buf[4];
    for (const char *tp = s.c_str(); int len = utf8towc(&c, tp); tp += len)
        res.append(buf, wctoutf8(buf, towlower(c)));
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
        ch = toupper(ch);

    return s;
}

string uppercase_string(string s)
{
    return uppercase(s);
}

// Warning: this (and uppercase_first()) relies on no libc (glibc, BSD libc,
// MSVC crt) supporting letters that expand or contract, like German ß (-> SS)
// upon capitalization / lowercasing.  This is mostly a fault of the API --
// there's no way to return two characters in one code point.
// Also, all characters must have the same length in bytes before and after
// lowercasing, all platforms currently have this property.
//
// A non-hacky version would be slower for no gain other than sane code; at
// least unless you use some more powerful API.
string lowercase_first(string s)
{
    ucs_t c;
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
    ucs_t c;
    if (!s.empty())
    {
        utf8towc(&c, &s[0]);
        wctoutf8(&s[0], towupper(c));
    }
    return s;
}

int ends_with(const string &s, const char *suffixes[])
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
string wordwrap_line(string &s, int width, bool tags, bool indent)
{
    const char *cp0 = s.c_str();
    const char *cp = cp0, *space = 0;
    ucs_t c;

    while (int clen = utf8towc(&c, cp))
    {
        int cw = wcwidth(c);
        if (c == ' ')
            space = cp;
        else if (c == '\n')
        {
            space = cp;
            break;
        }
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

    const string indentation = (indent && c != '\n') ? _get_indent(s) : "";

    // eat all trailing spaces and up to one newline
    while (*cp == ' ')
        cp++;
    if (*cp == '\n')
        cp++;
    s.erase(0, cp - cp0);

    // if we had to break a line, reinsert the indendation
    if (indent && c != '\n')
        s = indentation + s;

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

vector<string> split_string(const string &sep, string s, bool trim_segments,
                            bool accept_empty_segments, int nsplits)
{
    vector<string> segments;
    int separator_length = sep.length();

    string::size_type pos;
    while (nsplits && (pos = s.find(sep)) != string::npos)
    {
        add_segment(segments, s.substr(0, pos),
                    trim_segments, accept_empty_segments);

        s.erase(0, pos + separator_length);

        if (nsplits > 0)
            --nsplits;
    }

    if (!s.empty())
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
        buff += make_stringf("%d%s ", days, terse ? ","
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
