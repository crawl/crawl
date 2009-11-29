#include "AppHdr.h"

#include <stdint.h>

#include "show.h"

#include "areas.h"
#include "cloud.h"
#include "coordit.h"
#include "env.h"
#include "exclude.h"
#include "fprop.h"
#include "itemprop.h"
#include "mon-stuff.h"
#include "mon-util.h"
#include "monster.h"
#include "options.h"
#include "overmap.h"
#include "random.h"
#include "showsymb.h"
#include "state.h"
#include "stuff.h"
#include "areas.h"
#include "terrain.h"
#include "viewgeom.h"
#include "viewmap.h"

show_type::show_type()
    : cls(SH_NOTHING),
      feat(DNGN_UNSEEN),
      item(SHOW_ITEM_NONE),
      mons(MONS_NO_MONSTER),
      colour(0)
{
}

show_type::show_type(monster_type montype)
    : cls(SH_MONSTER),
      feat(DNGN_UNSEEN),
      item(SHOW_ITEM_NONE),
      mons(montype),
      colour(0)
{
}

show_type::show_type(dungeon_feature_type f)
    : cls(SH_FEATURE),
      feat(f),
      item(SHOW_ITEM_NONE),
      mons(MONS_NO_MONSTER),
      colour(0)
{
}

static show_item_type _item_to_show_code(const item_def &item);

show_type::show_type(const item_def &it)
    : cls(SH_ITEM),
      feat(DNGN_UNSEEN),
      item(_item_to_show_code(it)),
      mons(MONS_NO_MONSTER),
      colour(0)
{
}

show_type::show_type(show_item_type t)
    : cls(SH_ITEM),
      feat(DNGN_UNSEEN),
      item(t),
      mons(MONS_NO_MONSTER),
      colour(0)
{
}

bool show_type::operator < (const show_type &other) const
{
    if (cls < other.cls)
        return (false);
    else if (cls > other.cls)
        return (true);
    switch (cls)
    {
    case SH_FEATURE:
        return (feat < other.feat);
    case SH_ITEM:
        return (item < other.item);
    case SH_MONSTER:
        return (mons < other.mons);
    default:
        return (false);
    }
}

void show_def::_set_backup(const coord_def &ep)
{
    backup(ep) = grid(ep);
}

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
                                   const dungeon_feature_type feat)
{
    unsigned short colour = BLACK;
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
        if (testbits(env.pgrid(where), FPROP_SANCTUARY_1))
            colour = YELLOW;
        else if (testbits(env.pgrid(where), FPROP_SANCTUARY_2))
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
        colour = fdef.colour;
        if (colour == BLACK && feat == DNGN_TREES)
            colour = _tree_colour(where);

        if (fdef.em_colour && fdef.em_colour != fdef.colour
            && emphasise(where))
        {
            colour = fdef.em_colour;
        }
    }

    // TODO: should be a feat_is_whatever(feat)
    if (feat >= DNGN_FLOOR_MIN && feat <= DNGN_FLOOR_MAX
        || feat == DNGN_UNDISCOVERED_TRAP)
    {
        if (!haloers(where).empty())
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

void show_def::_update_feat_at(const coord_def &gp, const coord_def &ep)
{
    grid(ep).cls = SH_FEATURE;
    grid(ep).feat = grid_appearance(gp);
    grid(ep).colour = _feat_colour(gp, grid(ep).feat);

    if (get_feature_def(grid(ep)).is_notable())
        seen_notable_thing(grid(ep).feat, gp);
}

static show_item_type _item_to_show_code(const item_def &item)
{
    switch (item.base_type)
    {
    case OBJ_ORBS:       return (SHOW_ITEM_ORB);
    case OBJ_WEAPONS:    return (SHOW_ITEM_WEAPON);
    case OBJ_MISSILES:   return (SHOW_ITEM_MISSILE);
    case OBJ_ARMOUR:     return (SHOW_ITEM_ARMOUR);
    case OBJ_WANDS:      return (SHOW_ITEM_WAND);
    case OBJ_FOOD:       return (SHOW_ITEM_FOOD);
    case OBJ_SCROLLS:    return (SHOW_ITEM_SCROLL);
    case OBJ_JEWELLERY:
        return (jewellery_is_amulet(item)? SHOW_ITEM_AMULET : SHOW_ITEM_RING);
    case OBJ_POTIONS:    return (SHOW_ITEM_POTION);
    case OBJ_BOOKS:      return (SHOW_ITEM_BOOK);
    case OBJ_STAVES:     return (SHOW_ITEM_STAVE);
    case OBJ_MISCELLANY: return (SHOW_ITEM_MISCELLANY);
    case OBJ_CORPSES:    return (SHOW_ITEM_CORPSE);
    case OBJ_GOLD:       return (SHOW_ITEM_GOLD);
    default:             return (SHOW_ITEM_ORB); // bad item character
   }
}

void show_def::_update_item_at(const coord_def &gp, const coord_def &ep)
{
    item_def eitem;
    // Check for mimics.
    const monsters* m = monster_at(gp);
    if (m && mons_is_unknown_mimic(m))
        get_mimic_item(m, eitem);
    else if (igrd(gp) != NON_ITEM)
        eitem = mitm[igrd(gp)];
    else
        return;

    unsigned short &ecol  = grid(ep).colour;

    glyph g = get_item_glyph(&eitem);

    const dungeon_feature_type feat = grd(gp);
    if (Options.feature_item_brand && is_critical_feature(feat))
        ecol |= COLFLAG_FEATURE_ITEM;
    else if (Options.trap_item_brand && feat_is_trap(feat))
        ecol |= COLFLAG_TRAP_ITEM;
    else
    {
        const unsigned short gcol = env.grid_colours(gp);
        ecol = (feat == DNGN_SHALLOW_WATER) ?
               (gcol != BLACK ? gcol : CYAN) : g.col;
        if (eitem.link != NON_ITEM && !crawl_state.arena)
            ecol |= COLFLAG_ITEM_HEAP;
        grid(ep).cls = SH_ITEM;
        grid(ep).item = _item_to_show_code(eitem);
    }

#ifdef USE_TILE
    int idx = igrd(gp);
    if (feat_is_stair(feat))
        tile_place_item_marker(ep.x, ep.y, idx);
    else
        tile_place_item(ep.x, ep.y, idx);
#endif
}

void show_def::_update_cloud(int cloudno)
{
    const coord_def e = grid2show(env.cloud[cloudno].pos);
    int which_colour = get_cloud_colour(cloudno);
    _set_backup(e);
    grid(e).cls = SH_CLOUD;
    grid(e).colour = which_colour;

#ifdef USE_TILE
    tile_place_cloud(e.x, e.y, env.cloud[cloudno].type,
                     env.cloud[cloudno].decay);
#endif
}

static void _check_monster_pos(const monsters* monster)
{
    int s = monster->mindex();
    ASSERT(mgrd(monster->pos()) == s);

    // [rob] The following in case asserts aren't enabled.
    // [enne] - It's possible that mgrd and monster->x/y are out of
    // sync because they are updated separately.  If we can see this
    // monster, then make sure that the mgrd is set correctly.
    if (mgrd(monster->pos()) != s)
    {
        // If this mprf triggers for you, please note any special
        // circumstances so we can track down where this is coming
        // from.
        mprf(MSGCH_ERROR, "monster %s (%d) at (%d, %d) was "
             "improperly placed.  Updating mgrd.",
             monster->name(DESC_PLAIN, true).c_str(), s,
             monster->pos().x, monster->pos().y);
        mgrd(monster->pos()) = s;
    }
}

void show_def::_update_monster(const monsters* mons)
{
    _check_monster_pos(mons);

    const coord_def pos = mons->pos();
    const coord_def e = grid2show(pos);

    if (mons_is_unknown_mimic(mons))
        return;

    if (!mons->visible_to(&you))
    {
        // ripple effect?
        if (grd(pos) == DNGN_SHALLOW_WATER
            && !mons_flies(mons)
            && env.cgrid(pos) == EMPTY_CLOUD)
        {
            _set_backup(e);
            grid(e).cls = SH_INVIS_EXPOSED;

            // Translates between colours used for shallow and deep water,
            // if not using the normal LIGHTCYAN / BLUE. The ripple uses
            // the deep water colour.
            unsigned short base_colour = env.grid_colours(pos);

            static const unsigned short ripple_table[] =
                {BLUE,          // BLACK        => BLUE (default)
                 BLUE,          // BLUE         => BLUE
                 GREEN,         // GREEN        => GREEN
                 CYAN,          // CYAN         => CYAN
                 RED,           // RED          => RED
                 MAGENTA,       // MAGENTA      => MAGENTA
                 BROWN,         // BROWN        => BROWN
                 DARKGREY,      // LIGHTGREY    => DARKGREY
                 DARKGREY,      // DARKGREY     => DARKGREY
                 BLUE,          // LIGHTBLUE    => BLUE
                 GREEN,         // LIGHTGREEN   => GREEN
                 BLUE,          // LIGHTCYAN    => BLUE
                 RED,           // LIGHTRED     => RED
                 MAGENTA,       // LIGHTMAGENTA => MAGENTA
                 BROWN,         // YELLOW       => BROWN
                 LIGHTGREY};    // WHITE        => LIGHTGREY

            grid(e).colour = ripple_table[base_colour & 0x0f];
        }
        else if (is_opaque_cloud(env.cgrid(pos))
                 && !mons->submerged()
                 && !mons_is_insubstantial(mons->type))
        {
            grid(e).cls = SH_INVIS_EXPOSED;
            grid(e).colour = get_cloud_colour(env.cgrid(pos));
        }
        return;
    }

    // Mimics are always left on map.
    if (!mons_is_mimic(mons->type))
        _set_backup(e);

    grid(e).cls = SH_MONSTER;
    grid(e).mons = mons->type;
    grid(e).colour = get_mons_glyph(mons).col;

#ifdef USE_TILE
    tile_place_monster(mons->pos().x, mons->pos().y, mons->mindex(), true);
#endif
}

void show_def::update_at(const coord_def &gp, const coord_def &ep)
{
    grid(ep).cls = SH_NOTHING;

    // The sequence is grid, items, clouds, monsters.
    _update_feat_at(gp, ep);

    _update_item_at(gp, ep);

    const int cloud = env.cgrid(gp);
    if (cloud != EMPTY_CLOUD && env.cloud[cloud].type != CLOUD_NONE
        && env.cloud[cloud].pos == gp)
    {
        _update_cloud(cloud);
    }

    const monsters *mons = monster_at(gp);
    if (mons && mons->alive())
        _update_monster(mons);
}

void show_def::init()
{
    grid.init(show_type());
    backup.init(show_type());

    for (radius_iterator ri(&you.get_los()); ri; ++ri)
        update_at(*ri, grid2show(*ri));
}
