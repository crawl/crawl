/*
 *  File:       tilefont.h
 *  Created by: ennewalker on Sat Jan 5 01:33:53 2008 UTC
 *
 *  Modified for Crawl Reference by $Author: ennewalker $ on $Date: 2008-03-07 $
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
//
// * the possibility of streaming this class in and out so that Crawl doesn't
//   have to link against FreeType2 or be forced do as much processing at 
//   load time.

class FTFont
{
public:
    FTFont();
    virtual ~FTFont();

    bool load_font(const char *font_name, unsigned int font_size);

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

    unsigned int char_width() const { return m_max_advance.x; }
    unsigned int char_height() const { return m_max_advance.y; }

protected:
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
