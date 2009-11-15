#ifndef SHOWSYMB_H
#define SHOWSYMB_H

struct show_type;

struct glyph
{
    unsigned ch;
    unsigned short col;
};

glyph get_item_glyph(const item_def *item);
glyph get_mons_glyph(const monsters *mons);

unsigned get_screen_glyph( int x, int y );
unsigned get_screen_glyph( const coord_def &p );

void get_symbol(const coord_def& where,
                show_type object, unsigned *ch,
                unsigned short *colour);
void get_show_symbol(show_type object, unsigned *ch, unsigned short *colour);

#endif

