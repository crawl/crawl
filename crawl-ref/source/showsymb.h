#ifndef SHOWSYMB_H
#define SHOWSYMB_H

#include "show.h"

struct glyph
{
    unsigned ch;
    unsigned short col;
};

unsigned get_feat_symbol(dungeon_feature_type feat);
unsigned get_item_symbol(show_item_type it);
glyph get_item_glyph(const item_def *item);
glyph get_mons_glyph(const monsters *mons);
glyph get_show_glyph(show_type object);

unsigned get_screen_glyph( const coord_def &p );

#endif
