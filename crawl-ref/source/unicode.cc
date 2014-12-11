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
        do bad++; while ((s[bad] & 0xc0) == 0x80);
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

#ifdef TARGET_OS_WINDOWS
// don't pull in wstring templates on other systems
wstring utf8_to_16(const char *s)
{
    wstring d;
    ucs_t c;

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
    ucs_t c;

    while (*s)
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

string utf8_to_mb(const char *s)
{
#ifdef __ANDROID__
    return s;
#else
    string d;
    ucs_t c;
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
    ucs_t c;
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
    ucs_t c;
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

    die("memory got trampled");
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
    ucs_t c;
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

int wclen(ucs_t c)
{
    char dummy[4];
    return wctoutf8(dummy, c);
}

char *prev_glyph(char *s, char *start)
{
    ucs_t c;
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
    ucs_t c;
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
    ucs_t c;

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

unsigned short charset_vt100[128] =
{
    0x0000, 0x0001, 0x0002, 0x0003, 0x0004, 0x0005, 0x0006, 0x0007,
    0x0008, 0x0009, 0x000a, 0x000b, 0x000c, 0x000d, 0x000e, 0x000f,
    0x0010, 0x0011, 0x0012, 0x0013, 0x0014, 0x0015, 0x0016, 0x0017,
    0x0018, 0x0019, 0x001a, 0x001b, 0x001c, 0x001d, 0x001e, 0x001f,
    0x0020, 0x0021, 0x0022, 0x0023, 0x0024, 0x0025, 0x0026, 0x0027,
    0x0028, 0x0029, 0x002a, 0x2192, 0x2190, 0x2191, 0x2193, 0x002f,
    0x2588, 0x0031, 0x0032, 0x0033, 0x0034, 0x0035, 0x0036, 0x0037,
    0x0038, 0x0039, 0x003a, 0x003b, 0x003c, 0x003d, 0x003e, 0x003f,
    0x0040, 0x0041, 0x0042, 0x0043, 0x0044, 0x0045, 0x0046, 0x0047,
    0x0048, 0x0049, 0x004a, 0x004b, 0x004c, 0x004d, 0x004e, 0x004f,
    0x0050, 0x0051, 0x0052, 0x0053, 0x0054, 0x0055, 0x0056, 0x0057,
    0x0058, 0x0059, 0x005a, 0x005b, 0x005c, 0x005d, 0x005e, 0x00a0,
#if 0
    0x25c6, 0x2592, 0x2409, 0x240c, 0x240d, 0x240a, 0x00b0, 0x00b1,
    0x2591, 0x240b, 0x2518, 0x2510, 0x250c, 0x2514, 0x253c, 0xf800,
    0xf801, 0x2500, 0xf803, 0xf804, 0x251c, 0x2524, 0x2534, 0x252c,
    0x2502, 0x2264, 0x2265, 0x03c0, 0x2260, 0x00a3, 0x00b7, 0x007f,
#endif
    0x2666, 0x2592, 0x2409, 0x240c, 0x240d, 0x240a, 0x00b0, 0x00b1,
    0x2424, 0x240b, 0x2518, 0x2510, 0x250c, 0x2514, 0x253c, 0x23ba,
    0x23bb, 0x2500, 0x23bc, 0x23bd, 0x251c, 0x2524, 0x2534, 0x252c,
    0x2502, 0x2264, 0x2265, 0x03c0, 0x2260, 0x00a3, 0x00b7, 0x0020,
};
unsigned short charset_cp437[256] =
{
    0x0000, 0x263a, 0x263b, 0x2665, 0x2666, 0x2663, 0x2660, 0x2022,
    0x25d8, 0x25cb, 0x25d9, 0x2642, 0x2640, 0x266a, 0x266b, 0x263c,
    0x25b6, 0x25c0, 0x2195, 0x203c, 0x00b6, 0x00a7, 0x25ac, 0x21a8,
    0x2191, 0x2193, 0x2192, 0x2190, 0x221f, 0x2194, 0x25b2, 0x25bc,
    0x0020, 0x0021, 0x0022, 0x0023, 0x0024, 0x0025, 0x0026, 0x0027,
    0x0028, 0x0029, 0x002a, 0x002b, 0x002c, 0x002d, 0x002e, 0x002f,
    0x0030, 0x0031, 0x0032, 0x0033, 0x0034, 0x0035, 0x0036, 0x0037,
    0x0038, 0x0039, 0x003a, 0x003b, 0x003c, 0x003d, 0x003e, 0x003f,
    0x0040, 0x0041, 0x0042, 0x0043, 0x0044, 0x0045, 0x0046, 0x0047,
    0x0048, 0x0049, 0x004a, 0x004b, 0x004c, 0x004d, 0x004e, 0x004f,
    0x0050, 0x0051, 0x0052, 0x0053, 0x0054, 0x0055, 0x0056, 0x0057,
    0x0058, 0x0059, 0x005a, 0x005b, 0x005c, 0x005d, 0x005e, 0x005f,
    0x0060, 0x0061, 0x0062, 0x0063, 0x0064, 0x0065, 0x0066, 0x0067,
    0x0068, 0x0069, 0x006a, 0x006b, 0x006c, 0x006d, 0x006e, 0x006f,
    0x0070, 0x0071, 0x0072, 0x0073, 0x0074, 0x0075, 0x0076, 0x0077,
    0x0078, 0x0079, 0x007a, 0x007b, 0x007c, 0x007d, 0x007e, 0x2302,
    0x00c7, 0x00fc, 0x00e9, 0x00e2, 0x00e4, 0x00e0, 0x00e5, 0x00e7,
    0x00ea, 0x00eb, 0x00e8, 0x00ef, 0x00ee, 0x00ec, 0x00c4, 0x00c5,
    0x00c9, 0x00e6, 0x00c6, 0x00f4, 0x00f6, 0x00f2, 0x00fb, 0x00f9,
    0x00ff, 0x00d6, 0x00dc, 0x00a2, 0x00a3, 0x00a5, 0x20a7, 0x0192,
    0x00e1, 0x00ed, 0x00f3, 0x00fa, 0x00f1, 0x00d1, 0x00aa, 0x00ba,
    0x00bf, 0x2310, 0x00ac, 0x00bd, 0x00bc, 0x00a1, 0x00ab, 0x00bb,
    0x2591, 0x2592, 0x2593, 0x2502, 0x2524, 0x2561, 0x2562, 0x2556,
    0x2555, 0x2563, 0x2551, 0x2557, 0x255d, 0x255c, 0x255b, 0x2510,
    0x2514, 0x2534, 0x252c, 0x251c, 0x2500, 0x253c, 0x255e, 0x255f,
    0x255a, 0x2554, 0x2569, 0x2566, 0x2560, 0x2550, 0x256c, 0x2567,
    0x2568, 0x2564, 0x2565, 0x2559, 0x2558, 0x2552, 0x2553, 0x256b,
    0x256a, 0x2518, 0x250c, 0x2588, 0x2584, 0x258c, 0x2590, 0x2580,
    0x03b1, 0x00df, 0x0393, 0x03c0, 0x03a3, 0x03c3, 0x00b5, 0x03c4,
    0x03a6, 0x0398, 0x03a9, 0x03b4, 0x221e, 0x03c6, 0x03b5, 0x2229,
    0x2261, 0x00b1, 0x2265, 0x2264, 0x2320, 0x2321, 0x00f7, 0x2248,
    0x00b0, 0x2219, 0x00b7, 0x221a, 0x207f, 0x00b2, 0x25a0, 0x00a0,
};
