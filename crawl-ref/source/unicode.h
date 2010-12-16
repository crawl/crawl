/*
 *  File:       unicode.h
 *  Summary:    Conversions between Unicode and local charsets, string
 *              manipulation functions that act on character types.
 *  Written by: Adam Borowski
 */

int wctoutf8(char *d, ucs_t s);
int utf8towc(ucs_t *d, const char *s);
#ifdef TARGET_OS_WINDOWS
std::wstring utf8_to_16(const char *s);
std::string utf16_to_8(const wchar_t *s);
#endif
std::string utf8_to_mb(const char *s);
std::string mb_to_utf8(const char *s);

static inline std::string utf8_to_mb(const std::string &s)
{
    return utf8_to_mb(s.c_str());
}
static inline std::string mb_to_utf8(const std::string &s)
{
    return mb_to_utf8(s.c_str());
}

#define OUTS(x) utf8_to_mb(x).c_str()

class TextFileReader
{
    enum bom_type
    {
        BOM_NORMAL, // system locale
        BOM_UTF8,
        BOM_UTF16LE,
        BOM_UTF16BE,
        BOM_UTF32LE,
        BOM_UTF32BE,
    };
    FILE *f;
    bom_type bom;
    bool seen_eof;
public:
    TextFileReader(const char *name);
    ~TextFileReader();
    bool eof() { return seen_eof; };
    bool error() { return !f; };
    std::string get_line();
};
