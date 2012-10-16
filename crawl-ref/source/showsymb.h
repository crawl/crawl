#ifndef SHOWSYMB_H
#define SHOWSYMB_H

#include "show.h"
#include "mon-info.h"

struct map_cell;
struct cglyph_t
{
    ucs_t ch;
    unsigned short col; // XXX: real or unreal depending on context...

    cglyph_t(ucs_t _ch = 0, unsigned short _col = LIGHTGREY)
        : ch(_ch), col(_col)
    {
    }
};

string glyph_to_tagstr(const cglyph_t& g);

ucs_t get_feat_symbol(dungeon_feature_type feat);
ucs_t get_item_symbol(show_item_type it);
cglyph_t get_item_glyph(const item_def *item);
cglyph_t get_mons_glyph(const monster_info& mi);

show_class get_cell_show_class(const map_cell& cell, bool only_stationary_monsters = false);
cglyph_t get_cell_glyph(const coord_def& loc, bool only_stationary_monsters = false, int colour_mode = 0);
cglyph_t get_cell_glyph(const map_cell& cell, const coord_def& loc, bool only_stationary_monsters = false, int colour_mode = 0);

#endif
