/**
 * @file
 * @brief Conversions between Unicode and local charsets, string
 *        manipulation functions that act on character types.
**/

#include "AppHdr.h"

#include "unicode.h"

#include <climits>
#include <clocale>
#include <cstdio>
#include <cstring>
#include <string>

#include "syscalls.h"

// there must be at least 4 bytes free, NOT CHECKED!
int wctoutf8(char *d, char32_t s)
{
    if (s < 0x80)
    {
        d[0] = s;
        return 1;
    }
    if (s < 0x800)
    {
        d[0] = ( s >>  6)         | 0xc0;
        d[1] = ( s        & 0x3f) | 0x80;
        return 2;
    }
    if (s < 0x10000)
    {
        d[0] = ( s >> 12)         | 0xe0;
        d[1] = ((s >>  6) & 0x3f) | 0x80;
        d[2] = ( s        & 0x3f) | 0x80;
        return 3;
    }
    if (s < 0x110000)
    {
        d[0] = ( s >> 18)         | 0xf0;
        d[1] = ((s >> 12) & 0x3f) | 0x80;
        d[2] = ((s >>  6) & 0x3f) | 0x80;
        d[3] = ( s        & 0x3f) | 0x80;
        return 4;
    }
    // Invalid char marker (U+FFFD). Make sure we handled it above.
    ASSERT(s != 0xFFFD);
    return wctoutf8(d, 0xFFFD);
}

int utf8towc(char32_t *d, const char *s)
{
    if (*s == 0)
    {
        *d = 0;
        return 0;
    }
    if (!(*s & 0x80))
    {
        *d = *s;
        return 1;
    }
    if ((*s & 0xc0) == 0x80)
    {   // bare tail, invalid
        *d = 0xFFFD;
        int bad = 0;
        do bad++; while ((s[bad] & 0xc0) == 0x80);
        return bad;
    }

    int cnt;
    char32_t c;
    if ((*s & 0xe0) == 0xc0)
        cnt=2, c = *s & 0x1f;
    else if ((*s & 0xf0) == 0xe0)
        cnt=3, c = *s & 0x0f;
    else if ((*s & 0xf8) == 0xf0)
        cnt=4, c =*s & 0x07;
    /* valid UTF-8, invalid Unicode
    else if ((*s & 0xfc) == 0xf8)
        cnt=5, c = *s & 0x03;
    else if ((*s & 0xfe) == 0xfc)
        cnt=6, c = *s & 0x01;
    */
    else
    {   // 0xfe or 0xff, invalid
        *d = 0xFFFD;
        return 1;
    }

    for (int i = 1;  i < cnt; i++)
    {
        if ((s[i] & 0xc0) != 0x80)
        {   // only tail characters are allowed here, invalid
            *d = 0xFFFD;
            return i;
        }
        c = (c << 6) | (s[i] & 0x3f);
    }

    if (c < 0xA0                        // illegal characters
        || (c >= 0xD800 && c <= 0xDFFF) // UTF-16 surrogates
        || (cnt == 3 && c < 0x800)      // overlong characters
        || (cnt == 4 && c < 0x10000)    // overlong characters
        || c > 0x10FFFF)                // outside Unicode
    {
        c = 0xFFFD;
    }
    *d = c;
    return cnt;
}

#ifdef TARGET_OS_WINDOWS
// don't pull in wstring templates on other systems
wstring utf8_to_16(const char *s)
{
    wstring d;
    char32_t c;

    while (int l = utf8towc(&c, s))
    {
        s += l;
        if (c >= 0x10000)
        {
            c -= 0x10000;
            d.push_back(0xD800 + (c >> 10));
            d.push_back(0xDC00 + (c & 0x3FF));
        }
        else
            d.push_back(c);
    }
    return d;
}
#endif

#ifndef TARGET_OS_WINDOWS
static
#endif
string utf16_to_8(const utf16_t *s)
{
    string d;
    char32_t c;

    while (*s)
    {
        if (*s >= 0xD800 && *s <= 0xDBFF)
            if (s[1] >= 0xDC00 && s[1] <= 0xDFFF)
            {
                c = (((char32_t)s[0]) << 10) + s[1] - 0x35fdc00;
                s++;
            }
            else
                c = 0xFFFD; // leading surrogate without its tail
        else if (*s >= 0xDC00 && *s <= 0xDFFF)
            c = 0xFFFD;     // unpaired trailing surrogate
        else
            c = *s;
        s++;

        char buf[4];
        int l = wctoutf8(buf, c);
        for (int i = 0; i < l; i++)
            d.push_back(buf[i]);
    }

    return d;
}

string utf8_to_mb(const char *s)
{
#ifdef __ANDROID__
    return s;
#else
    string d;
    char32_t c;
    int l;
    mbstate_t ps;

    memset(&ps, 0, sizeof(ps));
    while ((l = utf8towc(&c, s)))
    {
        s += l;

        char buf[MB_LEN_MAX];
        int r = wcrtomb(buf, c, &ps);
        if (r != -1)
        {
            for (int i = 0; i < r; i++)
                d.push_back(buf[i]);
        }
        else
            d.push_back('?'); // TODO: try to transliterate
    }
    return d;
#endif
}

static string utf8_validate(const char *s)
{
    string d;
    char32_t c;
    int l;

    while ((l = utf8towc(&c, s)))
    {
        s += l;

        char buf[4];
        int r = wctoutf8(buf, c);
        for (int i = 0; i < r; i++)
            d.push_back(buf[i]);
    }
    return d;
}

string mb_to_utf8(const char *s)
{
#ifdef __ANDROID__
    // Paranoia; all consumers already use the same code so this won't do
    // anything new.
    return utf8_validate(s);
#else
    string d;
    wchar_t c;
    int l;
    mbstate_t ps;

    memset(&ps, 0, sizeof(ps));
    // the input is zero-terminated, so third argument doesn't matter
    while ((l = mbrtowc(&c, s, MB_LEN_MAX, &ps)))
    {
        if (l > 0)
            s += l;
        else
        {   // invalid input, mark it and try to recover
            s++;
            c = 0xFFFD;
        }

        char buf[4];
        int r = wctoutf8(buf, c);
        for (int i = 0; i < r; i++)
            d.push_back(buf[i]);
    }
    return d;
#endif
}

static bool _check_trail(FILE *f, const char* bytes, int len)
{
    while (len--)
    {
        if (fgetc(f) != (unsigned char)*bytes++)
        {
            rewind(f);
            return false;
        }
    }
    return true;
}

FileLineInput::FileLineInput(const char *name)
{
    f = fopen_u(name, "r");
    if (!f)
    {
        seen_eof = true;
        return;
    }
    seen_eof = false;

    bom = BOM_NORMAL;
    int ch = fgetc(f);
    switch (ch)
    {
    case 0xEF:
        if (_check_trail(f, "\xBB\xBF", 2))
            bom = BOM_UTF8;
        break;
    case 0xFE:
        if (_check_trail(f, "\xFF", 1))
            bom = BOM_UTF16BE;
        break;
    case 0xFF:
        if (_check_trail(f, "\xFE\x00\x00", 3))
            bom = BOM_UTF32LE;
        else if (_check_trail(f, "\xFF\xFE", 2)) // rewound
            bom = BOM_UTF16LE;
        break;
    case 0x00:
        if (_check_trail(f, "\x00\xFE\xFF", 3))
            bom = BOM_UTF32BE;
        break;
    default:
        ungetc(ch, f);
    }
}

FileLineInput::~FileLineInput()
{
    if (f)
        fclose(f);
}

string FileLineInput::get_line()
{
    ASSERT(f);
    vector<utf16_t> win;
    string out;
    char buf[512];
    char32_t c;
    int len;

    switch (bom)
    {
    case BOM_NORMAL:
        do
        {
            if (!fgets(buf, sizeof buf, f))
            {
                seen_eof = true;
                break;
            }
            out += buf;
            if (out[out.length() - 1] == '\n')
            {
                out.erase(out.length() - 1);
                break;
            }
        } while (!seen_eof);
        return mb_to_utf8(out.c_str());

    case BOM_UTF8:
        do
        {
            if (!fgets(buf, sizeof buf, f))
            {
                seen_eof = true;
                break;
            }
            out += buf;
            if (out[out.length() - 1] == '\n')
            {
                out.erase(out.length() - 1);
                break;
            }
        } while (!seen_eof);
        return utf8_validate(out.c_str());

    case BOM_UTF16LE:
        do
        {
            if (fread(buf, 2, 1, f) != 1)
            {
                seen_eof = true;
                break;
            }
            c = ((uint32_t)((unsigned char)buf[0]))
              | ((uint32_t)((unsigned char)buf[1])) << 8;
            if (c == '\n')
                break;
            win.push_back(c);
        }
        while (!seen_eof);
        win.push_back(0);
        return utf16_to_8(&win[0]);

    case BOM_UTF16BE:
        do
        {
            if (fread(buf, 2, 1, f) != 1)
            {
                seen_eof = true;
                break;
            }
            c = ((uint32_t)((unsigned char)buf[1]))
              | ((uint32_t)((unsigned char)buf[0])) << 8;
            if (c == '\n')
                break;
            win.push_back(c);
        }
        while (!seen_eof);
        win.push_back(0);
        return utf16_to_8(&win[0]);

    case BOM_UTF32LE:
        do
        {
            if (fread(buf, 4, 1, f) != 1)
            {
                seen_eof = true;
                break;
            }
            c = ((uint32_t)((unsigned char)buf[0]))
              | ((uint32_t)((unsigned char)buf[1])) << 8
              | ((uint32_t)((unsigned char)buf[2])) << 16
              | ((uint32_t)((unsigned char)buf[3])) << 24;
            if (c == '\n')
                break;
            len = wctoutf8(buf, c);
            for (int i = 0; i < len; i++)
                out.push_back(buf[i]);
        }
        while (!seen_eof);
        return out;

    case BOM_UTF32BE:
        do
        {
            if (fread(buf, 4, 1, f) != 1)
            {
                seen_eof = true;
                break;
            }
            c = ((uint32_t)((unsigned char)buf[0])) << 24
              | ((uint32_t)((unsigned char)buf[1])) << 16
              | ((uint32_t)((unsigned char)buf[2])) << 8
              | ((uint32_t)((unsigned char)buf[3]));
            if (c == '\n')
                break;
            len = wctoutf8(buf, c);
            for (int i = 0; i < len; i++)
                out.push_back(buf[i]);
        }
        while (!seen_eof);
        return out;
    }

    die("FileLineInput had a bad bom_type (%d)", bom);
}

UTF8FileLineInput::UTF8FileLineInput(const char *name)
{
    f = fopen_u(name, "r");
    if (!f)
    {
        seen_eof = true;
        return;
    }
    seen_eof = false;
}

UTF8FileLineInput::~UTF8FileLineInput()
{
    if (f)
        fclose(f);
}

string UTF8FileLineInput::get_line()
{
    ASSERT(f);
    string out;
    char buf[512];

    do
    {
        if (!fgets(buf, sizeof buf, f))
        {
            seen_eof = true;
            break;
        }
        out += buf;
        if (out[out.length() - 1] == '\n')
        {
            out.erase(out.length() - 1);
            break;
        }
    } while (!seen_eof);
    return utf8_validate(out.c_str());
}

int strwidth(const char *s)
{
    char32_t c;
    int w = 0;

    while (int l = utf8towc(&c, s))
    {
        s += l;
        int cw = wcwidth(c);
        if (cw != -1) // shouldn't ever happen
            w += cw;
    }

    return w;
}

int strwidth(const string &s)
{
    return strwidth(s.c_str());
}

int wclen(char32_t c)
{
    char dummy[4];
    return wctoutf8(dummy, c);
}

char *prev_glyph(char *s, char *start)
{
    char32_t c;
    do
    {
        // Find the start of the previous code point.
        do
            if (--s < start)
                return 0;
        while ((*s & 0xc0) == 0x80);
        // If a combining one, continue.
        utf8towc(&c, s);
    } while (!wcwidth(c));
    return s;
}

char *next_glyph(char *s)
{
    char *s_cur;
    char32_t c;
    // Skip at least one character.
    s += utf8towc(&c, s);
    if (!c)
        return 0;
    do
    {
        s += utf8towc(&c, s_cur = s);
        // And any combining ones after it.
    }
    while (c && !wcwidth(c));
    return s_cur;
}

string chop_string(const char *s, int width, bool spaces)
{
    const char *s0 = s;
    char32_t c;

    while (int clen = utf8towc(&c, s))
    {
        int cw = wcwidth(c);
        // Due to combining chars, we can't stop at merely reaching the
        // target width, the next character needs to exceed it.
        if (cw > width) // note: a CJK character might leave one space left
            break;
        if (cw >= 0) // should we assert on control chars instead?
            width -= cw;
        s += clen;
    }

    if (spaces && width)
        return string(s0, s - s0) + string(width, ' ');
    return string(s0, s - s0);;
}

string chop_string(const string &s, int width, bool spaces)
{
    return chop_string(s.c_str(), width, spaces);
}

string chop_tagged_string(const char *s, int width, bool spaces)
{
    const char * const s0 = s;
    bool in_tag = false;    // are we in a tag
    bool tag_first = false; // is this the first character of a tag?
    char32_t c;

    while (int clen = utf8towc(&c, s))
    {
        bool visible = true;

        if (in_tag)
        {
            if (c == '>')
                in_tag = false;

            // << is an escape for <, not a tag. Count the second < as visible.
            if (c == '<' && tag_first)
                in_tag = false;
            else
                visible = false;
            tag_first = false;
        }
        else if (c == '<')
        {
            in_tag = true;
            tag_first = true;
            visible = false;
        }

        if (visible)
        {
            const int cw = wcwidth(c);
            // Due to combining chars, we can't stop at merely reaching the
            // target width, the next character needs to exceed it.
            if (cw > width) // note: a CJK character might leave one space left
                break;
            if (cw >= 0) // should we assert on control chars instead?
                width -= cw;
        }

        s += clen;
    }

    if (spaces && width)
        return string(s0, s - s0) + string(width, ' ');
    return string(s0, s - s0);;
}

string chop_tagged_string(const string &s, int width, bool spaces)
{
    return chop_tagged_string(s.c_str(), width, spaces);
}
