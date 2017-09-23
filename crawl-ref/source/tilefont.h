#pragma once

#include "defines.h"
#include "glwrapper.h"

extern const VColour term_colours[NUM_TERM_COLOURS];

class FontBuffer;
class formatted_string;
class GenericTexture;

class FontWrapper
{
public:
    // Static creation functions.
    // Implementation-specific files (e.g. fontwrapper-ft.cc) should
    // contain their own version of this factory function.
    static FontWrapper *create();

    virtual ~FontWrapper() {}

    // font loading
    virtual bool load_font(const char *font_name, unsigned int font_size,
                           bool outline, int scale_num, int scale_den) = 0;

    // render just text
    virtual void render_textblock(unsigned int x, unsigned int y,
                                  char32_t *chars, uint8_t *colours,
                                  unsigned int width, unsigned int height,
                                  bool drop_shadow = false) = 0;

    // render text + background box
    virtual void render_string(unsigned int x, unsigned int y,
                               const char *text, const coord_def &min_pos,
                               const coord_def &max_pos,
                               unsigned char font_colour,
                               bool drop_shadow = false,
                               unsigned char box_alpha = 0,
                               unsigned char box_colour = 0,
                               unsigned int outline = 0,
                               bool tooltip = false) = 0;

    // FontBuffer helper functions
    virtual void store(FontBuffer &buf, float &x, float &y,
                       const string &s, const VColour &c) = 0;
    virtual void store(FontBuffer &buf, float &x, float &y,
                       const formatted_string &fs) = 0;
    virtual void store(FontBuffer &buf, float &x, float &y, char32_t c,
                       const VColour &col) = 0;
    virtual void store(FontBuffer &buf, float &x, float &y, char32_t c,
                       const VColour &fg_col, const VColour &bg_col) = 0;

    virtual unsigned int char_width() const = 0;
    virtual unsigned int char_height() const = 0;

    virtual unsigned int string_width(const char *text)  = 0;
    virtual unsigned int string_width(const formatted_string &str)  = 0;
    virtual unsigned int string_height(const char *text) const = 0;
    virtual unsigned int string_height(const formatted_string &str) const = 0;

    // Try to split this string to fit in w x h pixel area.
    virtual formatted_string split(const formatted_string &str,
                                   unsigned int max_width,
                                   unsigned int max_height) = 0;

   virtual const GenericTexture *font_tex() const = 0;
};
