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

#include "colour.h"
#include "env.h"
#include "envmap.h"
#include "mon-util.h"
#include "monster.h"
#include "options.h"
#include "overmap.h"
#include "random.h"
#include "show.h"
#include "spells3.h"
#include "state.h"
#include "stuff.h"
#include "terrain.h"
#include "viewchar.h"
#include "viewgeom.h"
#include "viewmap.h"

static bool _show_bloodcovered(const coord_def& where)
{
    if (!is_bloodcovered(where))
        return (false);

    dungeon_feature_type feat = env.grid(where);

    // Altars, stairs (of any kind) and traps should not be coloured red.
    return (!is_critical_feature(feat) && !feat_is_trap(feat));
}

static unsigned short _tree_colour(const coord_def& where)
{
    uint32_t h = where.x;
    h+=h<<10; h^=h>>6;
    h += where.y;
    h+=h<<10; h^=h>>6;
    h+=h<<3; h^=h>>11; h+=h<<15;
    return (h>>30) ? GREEN : LIGHTGREEN;
}

static unsigned short _feat_colour(const coord_def &where,
                                   const dungeon_feature_type feat,
                                   unsigned short colour)
{
    const feature_def &fdef = get_feature_def(feat);
    // TODO: consolidate with feat_is_stair etc.
    bool excluded_stairs = (feat >= DNGN_STONE_STAIRS_DOWN_I
                            && feat <= DNGN_ESCAPE_HATCH_UP
                            && is_exclude_root(where));

    if (excluded_stairs)
        colour = Options.tc_excluded;
    else if (feat >= DNGN_MINMOVE && you.get_beholder(where))
    {
        // Colour grids that cannot be reached due to beholders
        // dark grey.
        colour = DARKGREY;
    }
    else if (feat >= DNGN_MINMOVE && is_sanctuary(where))
    {
        if (testbits(env.map(where).property, FPROP_SANCTUARY_1))
            colour = YELLOW;
        else if (testbits(env.map(where).property, FPROP_SANCTUARY_2))
        {
            if (!one_chance_in(4))
                colour = WHITE;     // 3/4
            else if (!one_chance_in(3))
                colour = LIGHTCYAN; // 1/6
            else
                colour = LIGHTGREY; // 1/12
        }
    }
    else if (_show_bloodcovered(where))
        colour = RED;
    else if (env.grid_colours(where))
        colour = env.grid_colours(where);
    else
    {
        // Don't clobber with BLACK, because the colour should be
        // already set.
        if (fdef.colour != BLACK)
            colour = fdef.colour;
        else if (feat == DNGN_TREES)
            colour = _tree_colour(where);

        if (fdef.em_colour && fdef.em_colour != fdef.colour &&
            emphasise(where, feat))
        {
            colour = fdef.em_colour;
        }
    }

    // TODO: should be a feat_is_whatever(feat)
    if (feat >= DNGN_FLOOR_MIN && feat <= DNGN_FLOOR_MAX
        || feat == DNGN_UNDISCOVERED_TRAP)
    {
        if (you.inside_halo(where))
        {
            if (silenced(where))
                colour = LIGHTCYAN;
            else
                colour = YELLOW;
        }
        else if (silenced(where))
            colour = CYAN;
    }
    return (colour);
}

void get_symbol(const coord_def& where,
                show_type object, unsigned *ch,
                unsigned short *colour,
                bool magic_mapped)
{
    ASSERT(ch != NULL);

    if (object.cls < SH_MONSTER)
    {

        // Don't recolor items
        if (colour && object.cls == SH_FEATURE)
        {
            const int colmask = *colour & COLFLAG_MASK;
            *colour = _feat_colour(where, object.feat, *colour) | colmask;
        }

        const feature_def &fdef = get_feature_def(object);
        *ch = magic_mapped ? fdef.magic_symbol
                           : fdef.symbol;

        // Note anything we see that's notable
        if (!where.origin() && fdef.is_notable())
        {
            if (object.cls == SH_FEATURE)
                seen_notable_thing(object.feat, where);
        }
    }
    else
    {
        ASSERT(object.cls == SH_MONSTER);
        *ch = mons_char(object.mons);
    }

    if (colour)
        *colour = real_colour(*colour);
}

unsigned get_symbol(show_type object, unsigned short *colour,
                    bool magic_mapped)
{
    unsigned ch;
    get_symbol(coord_def(0,0), object, &ch, NULL, magic_mapped);
    return (ch);
}

void get_show_symbol(show_type object, unsigned *ch,
                     unsigned short *colour)
{
    if (object.cls < SH_MONSTER)
    {
        *ch = get_feature_def(object).symbol;

        // Don't clobber with BLACK, because the colour should be already set.
        if (get_feature_def(object).colour != BLACK)
            *colour = get_feature_def(object).colour;
    }
    *colour = real_colour(*colour);
}

unsigned grid_character_at(const coord_def &c)
{
    unsigned glych;
    unsigned short glycol = 0;

    get_symbol(c, grd(c), &glych, &glycol);
    return glych;
}

dungeon_char_type get_feature_dchar(dungeon_feature_type feat)
{
    return (get_feature_def(feat).dchar);
}

int get_mons_colour(const monsters *mons)
{
    int col = mons->colour;

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

void get_item_glyph(const item_def *item, unsigned *glych,
                    unsigned short *glycol)
{
    *glycol = item->colour;
    get_symbol(coord_def(0,0), show_type(*item), glych, glycol);
}

void get_mons_glyph(const monsters *mons, unsigned *glych,
                    unsigned short *glycol)
{
    *glycol = get_mons_colour(mons);
    get_symbol(coord_def(0,0), show_type(mons), glych, glycol);
}

unsigned get_screen_glyph( int x, int y )
{
    return get_screen_glyph(coord_def(x,y));
}

unsigned get_screen_glyph(const coord_def& p)
{
    const coord_def ep = view2show(grid2view(p));

    show_type object = env.show(ep);
    unsigned short  colour = object.colour;
    unsigned        ch;

    if (!object)
        return get_envmap_char(p.x, p.y);

    get_symbol(p, object, &ch, &colour);
    return (ch);
}
