/*
 * File:     showsymb.cc
 * Summary:  Rendering of show info to glyph and colour.
 *
 * Eventually, this should only need the information within
 * one object of type show_type (or show_info).
 */

#include "AppHdr.h"

#include "showsymb.h"

#include "colour.h"
#include "env.h"
#include "libutil.h"
#include "map_knowledge.h"
#include "mon-util.h"
#include "monster.h"
#include "options.h"
#include "show.h"
#include "state.h"
#include "stuff.h"
#include "terrain.h"
#include "viewchar.h"
#include "viewgeom.h"
#include "coord.h"

glyph get_cell_glyph(const map_cell& cell, bool clean_map)
{
    show_type object = cell.object;
    glyph g;
    g.col = object.colour;

    if (object.cls < SH_MONSTER || (clean_map && object.is_cleanable_monster()))
    {
        const feature_def &fdef = get_feature_def(object);
        g.ch = cell.seen() ? fdef.symbol : fdef.magic_symbol;
        if (g.col == BLACK)
            g.col = fdef.colour;
        if(!cell.visible() && object.cls == SH_FEATURE)
        {
            if(object.cls == SH_ITEM && cell.flags & MAP_DETECTED_ITEM)
                g.col = Options.detected_item_colour;
            else if(g.col == fdef.em_colour)
                g.col = fdef.seen_em_colour;
            else
                g.col = fdef.seen_colour;
        }
    }
    else
    {
        ASSERT(object.cls == SH_MONSTER);
        g.ch = mons_char(object.mons);
        if(!cell.visible())
        {
            if(cell.flags & MAP_DETECTED_MONSTER)
                g.col = Options.detected_monster_colour;
            else
                g.col = DARKGREY;
        }
    }

    if (g.col)
        g.col = real_colour(g.col);
    return (g);
}

static int _get_mons_colour(const monster_info& mi)
{
    int col = mi.colour;

    if (mi.type == MONS_SLIME_CREATURE && mi.number > 1)
        col = mons_class_colour(MONS_MERGED_SLIME_CREATURE);

    if (mi.is(MB_BERSERK))
        col = RED;

    if (mi.attitude == ATT_FRIENDLY)
    {
        col |= COLFLAG_FRIENDLY_MONSTER;
    }
    else if (mi.attitude != ATT_HOSTILE)
    {
        col |= COLFLAG_NEUTRAL_MONSTER;
    }
    else if (Options.stab_brand != CHATTR_NORMAL
             && mi.is(MB_STABBABLE))
    {
        col |= COLFLAG_WILLSTAB;
    }
    else if (Options.may_stab_brand != CHATTR_NORMAL
             && mi.is(MB_DISTRACTED))
    {
        col |= COLFLAG_MAYSTAB;
    }
    else if (mons_class_is_stationary(mi.type))
    {
        if (Options.feature_item_brand != CHATTR_NORMAL
            && is_critical_feature(grd(player2grid(mi.pos)))
            && feat_stair_direction(grd(player2grid(mi.pos))) != CMD_NO_CMD)
        {
            col |= COLFLAG_FEATURE_ITEM;
        }
        else if (Options.heap_brand != CHATTR_NORMAL
                 && you.visible_igrd(player2grid(mi.pos)) != NON_ITEM
                 && !crawl_state.game_is_arena())
        {
            col |= COLFLAG_ITEM_HEAP;
        }
    }

    // Backlit monsters are fuzzy and override brands.
    if (!crawl_state.game_is_arena() && 
        !you.can_see_invisible() && mi.is(MB_INVISIBLE))
    {
        col = DARKGREY;
    }

    return (col);
}

unsigned get_feat_symbol(dungeon_feature_type feat)
{
    return (get_feature_def(feat).symbol);
}

unsigned get_item_symbol(show_item_type it)
{
    return (get_feature_def(show_type(it)).symbol);
}

glyph get_item_glyph(const item_def *item)
{
    glyph g;
    g.ch = get_feature_def(show_type(*item)).symbol;
    g.col = item->colour;
    return (g);
}

glyph get_mons_glyph(const monster_info& mi, bool realcol)
{
    glyph g;
    if (mi.type == MONS_SLIME_CREATURE && mi.number > 1)
        g.ch = mons_char(MONS_MERGED_SLIME_CREATURE);
    else
        g.ch = mons_char(mi.type);
    g.col = _get_mons_colour(mi);
    if (realcol)
        g.col = real_colour(g.col);
    return (g);
}

std::string glyph_to_tagstr(const glyph& g)
{
    std::string col = colour_to_str(g.col);
    std::string ch = stringize_glyph(g.ch);
    if (g.ch == '<')
        ch += "<";
    return make_stringf("<%s>%s</%s>", col.c_str(), ch.c_str(), col.c_str());
}
