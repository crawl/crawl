/*
 *  File:       tilefont.h
 *  Created by: ennewalker on Sat Jan 5 01:33:53 2008 UTC
 */

#ifndef TILEFONT_H
#define TILEFONT_H

#include "AppHdr.h"
#include "externs.h"
#include "tiletex.h"

// This class handles loading FreeType2 fonts and rendering them via OpenGL.

// TODO enne - Fonts could be made better by:
//
// * handling kerning
// * using SDL_font (maybe?)
// * the possibility of streaming this class in and out so that Crawl doesn't
//   have to link against FreeType2 or be forced do as much processing at
//   load time.

class FontBuffer;
class VColour;
class formatted_string;

class FTFont
{
public:
    FTFont();
    virtual ~FTFont();

    bool load_font(const char *font_name, unsigned int font_size, bool outline);

    // render just text
    void render_textblock(unsigned int x, unsigned int y,
                          unsigned char *chars, unsigned char *colours,
                          unsigned int width, unsigned int height,
                          bool drop_shadow = false);

    // render text + background box
    void render_string(unsigned int x, unsigned int y, const char *text,
                       const coord_def &min_pos, const coord_def &max_pos,
                       unsigned char font_colour, bool drop_shadow = false,
                       unsigned char box_alpha = 0,
                       unsigned char box_colour = 0, unsigned int outline = 0);

    // FontBuffer helper functions
    void store(FontBuffer &buf, float &x, float &y,
               const std::string &s, const VColour &c);
    void store(FontBuffer &buf, float &x, float &y, const formatted_string &fs);
    void store(FontBuffer &buf, float &x, float &y, unsigned char c, const VColour &col);

    unsigned int char_width() const { return m_max_advance.x; }
    unsigned int char_height() const { return m_max_advance.y; }

    unsigned int string_width(const char *text);
    unsigned int string_width(const formatted_string &str);
    unsigned int string_height(const char *text);
    unsigned int string_height(const formatted_string &str);

    // Try to split this string to fit in w x h pixel area.
    formatted_string split(const formatted_string &str, unsigned int max_width,
                           unsigned int max_height);

    const GenericTexture *font_tex() const { return &m_tex; }

protected:
    void store(FontBuffer &buf, float &x, float &y,
               const std::string &s, const VColour &c, float orig_x);
    void store(FontBuffer &buf, float &x, float &y, const formatted_string &fs,
               float orig_x);

    int find_index_before_width(const char *str, int max_width);

    struct GlyphInfo
    {
        // offset before drawing glyph; can be negative
        char offset;

        // per-glyph horizontal advance
        char advance;

        // per-glyph width
        char width;

        bool renderable;
    };
    GlyphInfo *m_glyphs;

    // cached value of the maximum advance from m_advance
    coord_def m_max_advance;

    // minimum offset (likely negative)
    int m_min_offset;

    GenericTexture m_tex;
};

#endif
