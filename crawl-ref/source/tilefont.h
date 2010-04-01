/*
 *  File:       tilefont.h
 *  Created by: ennewalker on Sat Jan 5 01:33:53 2008 UTC
 */

#ifndef TILEFONT_H
#define TILEFONT_H

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

extern const VColour term_colours[MAX_TERM_COLOUR];

class FontBuffer;
class VColour;
class formatted_string;

class FontWrapper
{
public:
    // static creation functions
    static FontWrapper *create();
    
    virtual ~FontWrapper() {};

    // font loading
    virtual bool load_font( const char *font_name, unsigned int font_size,
                    bool outline) = 0;

    // render just text
    virtual void render_textblock( unsigned int x, unsigned int y,
                                   unsigned char *chars, unsigned char *colours,
                                   unsigned int width, unsigned int height,
                                   bool drop_shadow = false) = 0;

    // render text + background box
    virtual void render_string( unsigned int x, unsigned int y,
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
               const std::string &s, const VColour &c) = 0;
    virtual void store(FontBuffer &buf, float &x, float &y,
                const formatted_string &fs) = 0;
    virtual void store(FontBuffer &buf, float &x, float &y, unsigned char c,
                const VColour &col) = 0;

    virtual unsigned int char_width() const = 0;
    virtual unsigned int char_height() const = 0;

    virtual unsigned int string_width(const char *text) const = 0;
    virtual unsigned int string_width(const formatted_string &str) const = 0;
    virtual unsigned int string_height(const char *text) const = 0;
    virtual unsigned int string_height(const formatted_string &str) const = 0;

    // Try to split this string to fit in w x h pixel area.
    virtual formatted_string split( const formatted_string &str,
                                    unsigned int max_width,
                                    unsigned int max_height) = 0;

   virtual const GenericTexture *font_tex() const = 0;
};

#endif
