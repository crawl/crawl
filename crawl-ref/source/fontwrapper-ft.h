#ifndef FT_FONTWRAPPER_H
#define FT_FONTWRAPPER_H

#ifdef USE_TILE
#include "tilefont.h"

#ifdef USE_FT

// This class handles loading FreeType2 fonts and rendering them via OpenGL.

// TODO enne - Fonts could be made better by:
//
// * handling kerning
// * using SDL_font (maybe?)

class FTFontWrapper : public FontWrapper
{
public:
    FTFontWrapper();
    virtual ~FTFontWrapper();

    // font loading
    virtual bool load_font( const char *font_name, unsigned int font_size,
                    bool outline);

    // render just text
    virtual void render_textblock( unsigned int x, unsigned int y,
                                   unsigned char *chars, unsigned char *colours,
                                   unsigned int width, unsigned int height,
                                   bool drop_shadow = false);

    // render text + background box
    virtual void render_string( unsigned int x, unsigned int y,
                                const char *text, const coord_def &min_pos,
                                const coord_def &max_pos,
                                unsigned char font_colour,
                                bool drop_shadow = false,
                                unsigned char box_alpha = 0,
                                unsigned char box_colour = 0,
                                unsigned int outline = 0,
                                bool tooltip = false);

    // FontBuffer helper functions
    virtual void store(FontBuffer &buf, float &x, float &y,
               const std::string &s, const VColour &c);
    virtual void store(FontBuffer &buf, float &x, float &y,
                const formatted_string &fs);
    virtual void store(FontBuffer &buf, float &x, float &y, unsigned char c,
                const VColour &col);

    virtual unsigned int char_width() const;
    virtual unsigned int char_height() const;

    virtual unsigned int string_width(const char *text) const;
    virtual unsigned int string_width(const formatted_string &str) const;
    virtual unsigned int string_height(const char *text) const;
    virtual unsigned int string_height(const formatted_string &str) const;

    // Try to split this string to fit in w x h pixel area.
    virtual formatted_string split( const formatted_string &str,
                                    unsigned int max_width,
                                    unsigned int max_height);

    virtual const GenericTexture *font_tex() const;

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

#endif // USE_FT
#endif // USE_TILE
#endif // include guard