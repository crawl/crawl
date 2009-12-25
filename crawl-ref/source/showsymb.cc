/*
 * File:     showsymb.cc
 * Summary:  Rendering of show info to glyph and colour.
 *
 * Eventually, this should only need the information within
 * one object of type show_type (or show_info).
 */

#include "AppHdr.h"

#include "showsymb.h"

#include <stdint.h> // For uint32_t

#include "areas.h"
#include "colour.h"
#include "env.h"
#include "map_knowledge.h"
#include "fprop.h"
#include "mon-util.h"
#include "monster.h"
#include "options.h"
#include "overmap.h"
#include "random.h"
#include "show.h"
#include "state.h"
#include "stuff.h"
#include "terrain.h"
#include "viewchar.h"
#include "viewgeom.h"
#include "viewmap.h"

glyph get_show_glyph(show_type object)
{
    glyph g;
    g.col = object.colour;

    if (object.cls < SH_MONSTER)
    {
        const feature_def &fdef = get_feature_def(object);
        g.ch = fdef.symbol;
        if (g.col == BLACK)
            g.col = fdef.colour;
    }
    else
    {
        ASSERT(object.cls == SH_MONSTER);
        g.ch = mons_char(object.mons);
    }

    if (g.col)
        g.col = real_colour(g.col);
    return (g);
}

static int _get_mons_colour(const monsters *mons)
{
    int col = mons->colour;
    if (!crawl_state.arena && you.misled())
    {
        const monsterentry* mdat = get_monster_data(mons->get_mislead_type());
        col = mdat->colour;
    }

    if (mons->berserk())
        col = RED;

    if (mons->friendly())
    {
        col |= COLFLAG_FRIENDLY_MONSTER;
    }
    else if (mons->neutral())
    {
        col |= COLFLAG_NEUTRAL_MONSTER;
    }
    else if (Options.stab_brand != CHATTR_NORMAL
             && mons_looks_stabbable(mons))
    {
        col |= COLFLAG_WILLSTAB;
    }
    else if (Options.may_stab_brand != CHATTR_NORMAL
             && mons_looks_distracted(mons))
    {
        col |= COLFLAG_MAYSTAB;
    }
    else if (mons_is_stationary(mons))
    {
        if (Options.feature_item_brand != CHATTR_NORMAL
            && is_critical_feature(grd(mons->pos()))
            && feat_stair_direction(grd(mons->pos())) != CMD_NO_CMD)
        {
            col |= COLFLAG_FEATURE_ITEM;
        }
        else if (Options.heap_brand != CHATTR_NORMAL
                 && igrd(mons->pos()) != NON_ITEM
                 && !crawl_state.arena)
        {
            col |= COLFLAG_ITEM_HEAP;
        }
    }

    // Backlit monsters are fuzzy and override brands.
    if (!you.can_see_invisible() && mons->has_ench(ENCH_INVIS)
        && mons->backlit())
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

glyph get_mons_glyph(const monsters *mons)
{
    glyph g;

    if (!crawl_state.arena && you.misled())
        g.ch = mons_char(mons->get_mislead_type());
    else
        g.ch = mons_char(mons->type);
    g.col = _get_mons_colour(mons);
    return (g);
}

unsigned get_screen_glyph(const coord_def& p)
{
    const coord_def ep = view2show(grid2view(p));
    show_type object = env.show(ep);
    if (!object)
        return get_map_knowledge_char(p.x, p.y);

    return (get_show_glyph(object).ch);
}
