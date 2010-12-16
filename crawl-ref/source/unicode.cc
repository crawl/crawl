/*
 *  File:       unicode.cc
 *  Summary:    Conversions between Unicode and local charsets, string
 *              manipulation functions that act on character types.
 *  Written by: Adam Borowski
 */

#include "AppHdr.h"

#include <locale.h>
#include <string>
#include <string.h>
#include <limits.h>

#include "syscalls.h"
#include "unicode.h"

// there must be at least 4 bytes free, NOT CHECKED!
int wctoutf8(char *d, ucs_t s)
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
    // Invalid char marker (U+FFFD).
    d[0] = 0xef;
    d[1] = 0xbf;
    d[2] = 0xbd;
    return 3;
}

int utf8towc(ucs_t *d, const char *s)
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
        do bad++; while((s[bad] & 0xc0) == 0x80);
        return bad;
    }

    int cnt;
    ucs_t c;
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

std::wstring utf8_to_16(const char *s)
{
    std::wstring d;
    ucs_t c;

    while(int l = utf8towc(&c, s))
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

std::string utf16_to_8(const wchar_t *s)
{
    std::string d;
    ucs_t c;

    while(*s)
    {
        if (*s >= 0xD800 && *s <= 0xDBFF)
            if (s[1] >= 0xDC00 && s[1] <= 0xDFFF)
            {
                c = (((ucs_t)s[0]) << 10) + s[1] - 0x35fdc00;
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

std::string utf8_to_mb(const char *s)
{
    std::string d;
    ucs_t c;
    int l;
    mbstate_t ps;

    memset(&ps, 0, sizeof(ps));
    while((l = utf8towc(&c, s)))
    {
        s += l;

        char buf[MB_LEN_MAX];
        int r = wcrtomb(buf, c, &ps);
        if (r != -1)
        {
            for (int i = 0; i < l; i++)
                d.push_back(buf[i]);
        }
        else
            d.push_back('?'); // TODO: try to transliterate
    }
    return d;
}

std::string mb_to_utf8(const char *s)
{
    std::string d;
    wchar_t c;
    int l;
    mbstate_t ps;

    memset(&ps, 0, sizeof(ps));
    // the input is zero-terminated, so third argument doesn't matter
    while((l = mbrtowc(&c, s, MB_LEN_MAX, &ps)))
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
}

static std::string utf8_validate(const char *s)
{
    std::string d;
    ucs_t c;
    int l;

    while((l = utf8towc(&c, s)))
    {
        s += l;

        char buf[4];
        int r = wctoutf8(buf, c);
        for (int i = 0; i < r; i++)
            d.push_back(buf[i]);
    }
    return d;
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

TextFileReader::TextFileReader(const char *name)
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
    switch(ch)
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

TextFileReader::~TextFileReader()
{
    if (f)
        fclose(f);
}

std::string TextFileReader::get_line()
{
    ASSERT(f);
    std::wstring win; // actually, these are more of a lose
    std::string out;
    char buf[512];
    ucs_t c;
    int len;

    switch(bom)
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
        return utf16_to_8(win.c_str());

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
        return utf16_to_8(win.c_str());

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

    ASSERT(!"our memory got trampled!");
    return "говно";
}
