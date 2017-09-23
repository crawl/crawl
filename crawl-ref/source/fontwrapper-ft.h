#pragma once

#ifdef USE_TILE_LOCAL
#ifdef USE_FT

#include <map>
#include <ft2build.h>
#include FT_FREETYPE_H

#include "tilefont.h"

// TODO enne - Fonts could be made better by:
//
// * handling kerning
// * using SDL_font (maybe?)
// * the possibility of streaming this class in and out so that Crawl doesn't
//   have to link against FreeType2 or be forced do as much processing at
//   load time.

class FTFontWrapper : public FontWrapper
{
public:
    FTFontWrapper();
    virtual ~FTFontWrapper();

    // font loading
    virtual bool load_font(const char *font_name, unsigned int font_size,
                           bool outline, int scale_num, int scale_den) override;

    // render just text
    virtual void render_textblock(unsigned int x, unsigned int y,
                                  char32_t *chars, uint8_t *colours,
                                  unsigned int width, unsigned int height,
                                  bool drop_shadow = false) override;

    // render text + background box
    virtual void render_string(unsigned int x, unsigned int y,
                               const char *text, const coord_def &min_pos,
                               const coord_def &max_pos,
                               unsigned char font_colour,
                               bool drop_shadow = false,
                               unsigned char box_alpha = 0,
                               unsigned char box_colour = 0,
                               unsigned int outline = 0,
                               bool tooltip = false) override;

    // FontBuffer helper functions
    virtual void store(FontBuffer &buf, float &x, float &y,
                       const string &s, const VColour &c) override;
    virtual void store(FontBuffer &buf, float &x, float &y,
                       const formatted_string &fs) override;
    virtual void store(FontBuffer &buf, float &x, float &y, char32_t c,
                       const VColour &col) override;
    virtual void store(FontBuffer &buf, float &x, float &y, char32_t c,
                       const VColour &fg_col, const VColour &bg_col) override;

    virtual unsigned int char_width() const override;
    virtual unsigned int char_height() const override;

    virtual unsigned int string_width(const char *text) override;
    virtual unsigned int string_width(const formatted_string &str) override;
    virtual unsigned int string_height(const char *text) const override;
    virtual unsigned int string_height(const formatted_string &str) const override;

    // Try to split this string to fit in w x h pixel area.
    virtual formatted_string split(const formatted_string &str,
                                   unsigned int max_width,
                                   unsigned int max_height) override;

    virtual const GenericTexture *font_tex() const override;

protected:
    // Not overrides! These two have an additional orig_x parameter compared
    // to the virtuals.
    void store(FontBuffer &buf, float &x, float &y,
               const string &s, const VColour &c, float orig_x);
    void store(FontBuffer &buf, float &x, float &y, const formatted_string &fs,
               float orig_x);

    int find_index_before_width(const char *str, int max_width);

    unsigned int map_unicode(char32_t uchar, bool update);
    unsigned int map_unicode(char32_t uchar);
    void load_glyph(unsigned int c, char32_t uchar);
    void draw_m_buf(unsigned int x_pos, unsigned int y_pos, bool drop_shadow);

    struct GlyphInfo
    {
        // offset before drawing glyph; can be negative
#ifdef __ANDROID__
        // signed int in android port
        int offset;
#else
        int8_t offset;
#endif

        // per-glyph horizontal advance
        int8_t advance;
        // per-glyph width
        int8_t width;
        // per-glyph ascender
        int8_t ascender;

        // does glyph have any pixels?
        bool renderable;

        // index of prev/next glyphs in LRU
        unsigned int prev; unsigned int next;
        // charcode of glyph
        char32_t uchar;
    };
    GlyphInfo *m_glyphs;
    map<char32_t, unsigned int> m_glyphmap;
    // index of least recently used glyph
    char32_t m_glyphs_lru;
    // index of most recently used glyph
    char32_t m_glyphs_mru;
    // index of last populated glyph until m_glyphs[] is full
    char32_t m_glyphs_top;

    // count of glyph loads in the current text block
    int n_subst;

    // cached value of the maximum advance from m_advance
    coord_def m_max_advance;

    // minimum offset (likely negative)
    int m_min_offset;

    // size of ascender according to font
    int m_ascender;

    // other font metrics
    coord_def charsz;
    unsigned int m_ft_width;
    unsigned int m_ft_height;
    int m_max_width;
    int m_max_height;

    GenericTexture m_tex;
    GLShapeBuffer *m_buf;

    FT_Face face;
    bool    outl;
    unsigned char *pixels;

    int scale_num;
    int scale_den;
};

#endif // USE_FT
#endif // USE_TILE_LOCAL
