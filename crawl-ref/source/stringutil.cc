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
string wordwrap_line(string &s, int width, bool tags, bool indent)
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
                               ? _get_indent(s) : "";

    // eat all trailing spaces and up to one newline
    while (*cp == ' ')
        cp++;
    if (*cp == '\n')
        cp++;

#ifdef ASSERTS
    const size_t inputlength = s.length();
#endif
    s.erase(0, cp - cp0);

    // if we had to break a line, reinsert the indendation
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

// characters that end a printf format specifier
static const char* type_specs = "%diufFeEgGxXoscpaAn";

// split printf-style format string into plain strings and format specifiers
void split_format_string(const char* s, vector<string>& tokens)
{
    if (!s)
        return;

    const char* starttok = s;
    const char* endtok;
    while (*starttok != '\0')
    {
        if (*starttok != '%')
        {
            // plain string
            endtok = starttok;
            while (*(endtok+1) != '\0' && *(endtok+1) != '%')
                ++endtok;
        }
        else
        {
            endtok = starttok+1;
            while (*endtok != '\0' && !strchr(type_specs, *endtok))
                ++endtok;
            if (*endtok == '\0')
            {
                // malformed
                --endtok;
            }
        }
        tokens.push_back(string(starttok, endtok - starttok + 1));
        starttok = endtok + 1;
    }
}

void split_format_string(const string &s, vector<string>& tokens)
{
    split_format_string(s.c_str(), tokens);
}

// translate a printf format spec like "%d" to a type like int
const type_info* format_spec_to_type(const string& fmt)
{
    if (fmt.length() < 2 || fmt.at(0) != '%')
        return nullptr;

    char last_char = fmt.back();
    char penultimate = fmt.at(fmt.length()-2);

    if (last_char == 's')
    {
        // string
        if (penultimate == 'l')
            return &typeid(wchar_t*);
        else
            return &typeid(char*);
    }
    else if (strchr("iduoxX", last_char))
    {
        // integer
        // signed or unsigned doesn't matter - the size is the same
        if (penultimate == 'l')
        {
            if (fmt.length() > 2 && fmt.at(fmt.length()-3) == 'l')
                return &typeid(long long);
            else
                return &typeid(long);
        }
        else if (penultimate == 't')
            return &typeid(ptrdiff_t);
        else if (penultimate == 'z')
            return &typeid(size_t);
        else if (penultimate == 'j')
            return &typeid(intmax_t);
        else if (penultimate == 'I')
            if (strchr("id", last_char))
                return &typeid(ptrdiff_t);
            else
                return &typeid(size_t);
        else if (penultimate == 'q')
            return &typeid(int64_t);
        else if (contains(fmt, "I32"))
            return &typeid(int32_t);
        else if (contains(fmt, "I64"))
            return &typeid(int64_t);
        else
            return &typeid(int);
    }
    else if (strchr("aAeEfFgG", last_char))
    {
        // floating point
        if (fmt.at(fmt.length()-2) == 'L')
            return &typeid(long double);
        else
            return &typeid(double);
    }
    else if (last_char == 'c')
    {
        // char is promoted to int
        if (penultimate == 'l')
            return &typeid(wint_t);
        else
            return &typeid(int);
    }
    else if (last_char == 'p')
        return &typeid(void*);
    else if (last_char == 'n')
    {
        // actually a pointer to some type of int,
        // but we store as void* and work it out later
        return &typeid(void*);
    }

    return nullptr;
}

typedef map<int, const type_info*> arg_type_map_t;

// get arg types from a tokenised printf format string
void get_arg_types(const vector<string>& tokens, arg_type_map_t &results)
{
    int arg_count = 0;
    vector<string>::const_iterator it;
    for (it = tokens.begin(); it != tokens.end(); ++it)
    {
        const string &s = *it;
        if (s.length() < 2 || s.at(0) != '%' || ends_with(s, "%"))
            continue;

        // explictly numbered arguments?
        bool explicit_arg_ids = contains(s, "$");

        // handle dynamic width/precision
        size_t pos = 0;
        while ((pos = s.find('*', pos)) != string::npos)
        {
            ++pos;
            ++arg_count;
            int arg_id = 0;
            if (explicit_arg_ids)
                arg_id = atoi(s.substr(pos).c_str());
            if (arg_id == 0)
                arg_id = arg_count;
            results.insert(std::make_pair(arg_id, &typeid(int)));
        }

        // now the arg itself
        ++arg_count;
        int arg_id = 0;
        if (explicit_arg_ids)
            arg_id = atoi(s.substr(1).c_str());
        if (arg_id == 0)
            arg_id = arg_count;
        const type_info* ti = format_spec_to_type(*it);
        results[arg_id] = ti;
    }
}

// get arg types from an untokenised printf format string
void get_arg_types(const string& s, arg_type_map_t &results)
{
    vector<string> tokens;
    split_format_string(s.c_str(), tokens);
    get_arg_types(tokens, results);
}

typedef union {
    char* s;
    wchar_t* ws;
    int i;
    long l;
    long long ll;
    intmax_t im;
    double d;
    long double ld;
    void* pv;
    ptrdiff_t ptrdiff;
    size_t sz;
    wint_t wi;
    int32_t i32;
    int64_t i64;
    uint32_t ui32;
    uint64_t ui64;
} arg_t;

typedef map<int, arg_t> arg_map_t;

static void _pop_va_args(va_list args, const arg_type_map_t &types, arg_map_t &results)
{
    arg_type_map_t::const_iterator iter;
    for (iter = types.begin(); iter != types.end(); ++iter)
    {
        int arg_id = iter->first;
        arg_t arg;
        if (iter->second == &typeid(char*))
            arg.s = va_arg(args, char*);
        else if (iter->second == &typeid(wchar_t*))
            arg.ws = va_arg(args, wchar_t*);
        else if (iter->second == &typeid(int))
            arg.i = va_arg(args, int);
        else if (iter->second == &typeid(long))
            arg.l = va_arg(args, long);
        else if (iter->second == &typeid(long long))
            arg.ll = va_arg(args, long long);
        else if (iter->second == &typeid(intmax_t))
            arg.im = va_arg(args, intmax_t);
        else if (iter->second == &typeid(double))
            arg.d = va_arg(args, double);
        else if (iter->second == &typeid(long double))
            arg.ld = va_arg(args, long double);
        else if (iter->second == &typeid(void*))
            arg.pv = va_arg(args, void*);
        else if (iter->second == &typeid(ptrdiff_t))
            arg.ptrdiff = va_arg(args, ptrdiff_t);
        else if (iter->second == &typeid(size_t))
            arg.sz = va_arg(args, size_t);
        else if (iter->second == &typeid(wint_t))
            arg.wi = va_arg(args, wint_t);
        else if (iter->second == &typeid(int32_t))
            arg.i32 = va_arg(args, int32_t);
        else if (iter->second == &typeid(int64_t))
            arg.i64 = va_arg(args, int64_t);
        else if (iter->second == &typeid(uint32_t))
            arg.ui32 = va_arg(args, uint32_t);
        else if (iter->second == &typeid(uint64_t))
            arg.ui64 = va_arg(args, uint64_t);
        else
            arg.i = va_arg(args, int);

        results[arg_id] = arg;
    }
}

// format UTF-8 string using printf-style format specifier
string format_utf8_string(const string& fmt, const string& arg)
{
    if (fmt == "%s")
    {
        // trivial case
        return arg;
    }

    // if there are width/precision args, then sprintf won't handle it correctly
    // (it will count bytes), so we have to handle it in a unicode-aware way
    int width = -1;
    int precision = -1;
    bool right_justify = true; // default for %s

    for (size_t i = 1; i < fmt.length(); i++)
    {
        char ch = fmt[i];
        if (ch == '-')
            right_justify = false;
        else if (ch == '.')
            precision = 0;
        else if (ch >= '0' && ch <= '9')
            if (precision >= 0)
                precision = (precision * 10) + (int)(ch - '0');
            else
            {
                if (width < 0)
                    width = (int)(ch - '0');
                else
                    width = (width * 10) + (int)(ch - '0');
            }
    }

    string result = arg;

    if (precision >= 0)
        result = chop_string(result, precision, false);

    if (width > 0)
        result = pad_string(result, width, right_justify);

    return result;
}

string vmake_stringf(const char* s, va_list args)
{
    if (!s || *s == '\0')
        return "";

    vector<string> tokens;
    arg_type_map_t arg_types;
    arg_map_t arg_values;

    split_format_string(s, tokens);
    get_arg_types(tokens, arg_types);

    va_list args_copy;
    va_copy(args_copy, args);
    _pop_va_args(args_copy, arg_types, arg_values);
    va_end(args_copy);

    char buf[200];

    stringstream ss;

    int arg_count = 0;
    for (string tok: tokens)
    {
        if (!starts_with(tok, "%"))
        {
            // plain string
            ss << tok;
        }
        else if (ends_with(tok, "%"))
        {
            // escaped percent sign
            ss << '%';
        }
        else {
            string fmt(tok);

            // explictly numbered arguments?
            bool explicit_arg_ids = contains(fmt, "$");

            // handle dynamic width/precision
            size_t pos = 0;
            while ((pos = fmt.find('*')) != string::npos)
            {
                ++arg_count;
                int arg_id = 0;
                if (explicit_arg_ids)
                    arg_id = atoi(fmt.substr(pos+1).c_str());
                if (arg_id == 0)
                    arg_id = arg_count;

                auto arg_it = arg_values.find(arg_id);
                if (arg_it == arg_values.end())
                {
                    ss << "(missing arg #" << arg_id << ")";
                    continue;
                }
                sprintf(buf, "%d", arg_it->second.i);

                // replace "*" or "*<num>$" with actual width/precision value
                size_t end = fmt.find('$', pos);
                if (end == string::npos)
                    end = pos;
                fmt.replace(pos, end - pos + 1, buf);
            }

            arg_count++;
            int arg_id = 0;
            if (explicit_arg_ids)
                arg_id = atoi(fmt.substr(1).c_str());
            if (arg_id == 0)
                arg_id = arg_count;

            // remove arg id
            pos = fmt.find('$', pos);
            if (pos != string::npos)
                fmt.replace(1, pos, buf);

            const type_info* arg_type = arg_types[arg_id];
            auto arg_it = arg_values.find(arg_id);
            if (arg_type == nullptr || arg_it == arg_values.end())
            {
                ss << "(missing arg #" << arg_id << ")";
                continue;
            }
            arg_t arg = arg_it->second;

            if (arg_type == &typeid(char*))
            {
                // string
                if (fmt == "%s")
                    ss << (arg.s ? arg.s : ""); // trivial case
                else
                    ss << format_utf8_string(fmt, arg.s ? arg.s : "");
            }
            else if (arg_type == &typeid(wchar_t*))
            {
                // wide char string (shouldn't happen, but just in case)
                size_t len = snprintf(buf, sizeof buf, fmt.c_str(), arg.ws);
                if (len >= sizeof buf)
                {
                    // too big for buf
                    char* buf2 = new char[len + 1];
                    snprintf(buf2, len + 1, fmt.c_str(), arg.ws);
                    ss << buf2;
                    delete[] buf2;
                }
                else if (len > 0)
                    ss << buf;
            }
            else if (ends_with(fmt, "n"))
            {
                // output the number of characters printed so far to the arg
                if (!arg.pv)
                    continue;
                size_t count = strwidth(ss.str());
                if (ends_with(fmt, "lln"))
                    *(long long*)arg.pv = (long long)count;
                else if (ends_with(fmt, "ln"))
                    *(long*)arg.pv = (long)count;
                else if (ends_with(fmt, "hhn"))
                    *(char*)arg.pv = (char)count;
                else if (ends_with(fmt, "hn"))
                    *(short*)arg.pv = (short)count;
                else if (ends_with(fmt, "jn"))
                    *(intmax_t*)arg.pv = (intmax_t)count;
                else if (ends_with(fmt, "zn"))
                    *(size_t*)arg.pv = (size_t)count;
                else if (ends_with(fmt, "tn"))
                    *(ptrdiff_t*)arg.pv = (ptrdiff_t)count;
                else
                    *(int*)arg.pv = (int)count;
            }
            else
            {
                // numeric or char
                buf[0] = '\0';
                if (arg_type == &typeid(int))
                    snprintf(buf, sizeof buf, fmt.c_str(), arg.i);
                else if (arg_type == &typeid(long))
                    snprintf(buf, sizeof buf, fmt.c_str(), arg.l);
                else if (arg_type == &typeid(long long))
                    snprintf(buf, sizeof buf, fmt.c_str(), arg.ll);
                else if (arg_type == &typeid(intmax_t))
                    snprintf(buf, sizeof buf, fmt.c_str(), arg.im);
                else if (arg_type == &typeid(double))
                    snprintf(buf, sizeof buf, fmt.c_str(), arg.d);
                else if (arg_type == &typeid(long double))
                    snprintf(buf, sizeof buf, fmt.c_str(), arg.ld);
                else if (arg_type == &typeid(void*))
                    snprintf(buf, sizeof buf, fmt.c_str(), arg.pv);
                else if (arg_type == &typeid(ptrdiff_t))
                    snprintf(buf, sizeof buf, fmt.c_str(), arg.ptrdiff);
                else if (arg_type == &typeid(size_t))
                    snprintf(buf, sizeof buf, fmt.c_str(), arg.sz);
                else if (arg_type == &typeid(wint_t))
                    snprintf(buf, sizeof buf, fmt.c_str(), arg.wi);
                else if (arg_type == &typeid(int32_t))
                    snprintf(buf, sizeof buf, fmt.c_str(), arg.i32);
                else if (arg_type == &typeid(int64_t))
                    snprintf(buf, sizeof buf, fmt.c_str(), arg.i64);

                ss << buf;
            }
        }
    }

    return ss.str();
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

// Replace first occurrence of <tofind> with <replacement>
string replace_first(const string &s, const string &tofind, const string &replacement)
{
    string result = s;
    size_t pos = s.find(tofind);
    if (pos != string::npos)
        result.replace(pos, tofind.length(), replacement);
    return result;
}

// Replace last occurrence of <tofind> with <replacement>
string replace_last(const string &s, const string &tofind, const string &replacement)
{
    string result = s;
    size_t pos = s.rfind(tofind);
    if (pos != string::npos)
        result.replace(pos, tofind.length(), replacement);
    return result;
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
    static int nested_calls = 0;

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

        if (!value && isupper(key[0]))
            value = map_find(replacements, lowercase_string(key));

        if (!value)
            return text;

        // capitalise value if key is capitalised
        string value2;
        if (isupper(key[0])) {
            value2 = uppercase_first(*value);
            value = &value2;
        }

        // allow nesting, but avoid infinite loops from circular refs
        if (nested_calls < 5 && value->find("@") != string::npos)
        {
            nested_calls++;
            string new_value = replace_keys(*value, replacements);
            nested_calls--;
            res << new_value;
        }
        else
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

bool contains(const string &text, const string &s)
{
    return !s.empty() && text.find(s) != string::npos;
}

bool contains(const string &text, char c)
{
    return text.find(c) != string::npos;
}

bool contains(const char* text, char c)
{
    return text && strchr(text, c);
}

bool is_all_digits(const string& s)
{
    if (s.empty())
        return false;

    for (char c: s)
    {
        if (!isdigit(c))
            return false;
    }

    return true;
}

bool is_all_alphas(const string& s)
{
    if (s.empty())
        return false;

    for (char c: s)
    {
        if (!isalpha(c))
            return false;
    }

    return true;
}

// also used with macros
string &trim_string(string &str)
{
    str.erase(0, str.find_first_not_of(" \t\n\r"));
    str.erase(str.find_last_not_of(" \t\n\r") + 1);

    return str;
}

string &trim_string_left(string &str)
{
    str.erase(0, str.find_first_not_of(" \t\n\r"));
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

    add_segment(segments, s, trim_segments, accept_empty_segments);

    return segments;
}


// Crude, but functional.
string make_time_string(time_t abs_time, bool /*terse*/)
{
    const int hours = abs_time / 3600;
    const int mins  = (abs_time % 3600) / 60;
    const int secs  = abs_time % 60;

    return make_stringf("%02d:%02d:%02d", hours, mins, secs);
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
