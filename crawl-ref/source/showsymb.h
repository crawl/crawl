#ifndef SHOWSYMB_H
#define SHOWSYMB_H

#include "show.h"
#include "mon-info.h"

struct map_cell;
struct glyph
{
    ucs_t ch;
    unsigned short col; // XXX: real or unreal depending on context...

    glyph(ucs_t _ch = ' ', unsigned short _col = LIGHTGREY)
        : ch(_ch), col(_col)
    {
    }
};

std::string glyph_to_tagstr(const glyph& g);

ucs_t get_feat_symbol(dungeon_feature_type feat);
ucs_t get_item_symbol(show_item_type it);
glyph get_item_glyph(const item_def *item);
glyph get_mons_glyph(const monster_info& mi);

show_class get_cell_show_class(const map_cell& cell, bool only_stationary_monsters = false);
glyph get_cell_glyph(const coord_def& loc, bool only_stationary_monsters = false, int color_mode = 0);
glyph get_cell_glyph(const map_cell& cell, const coord_def& loc, bool only_stationary_monsters = false, int color_mode = 0);

#endif
