#include "AppHdr.h"

#include "show.h"

#include "cloud.h"
#include "colour.h"
#include "coordit.h"
#include "directn.h"
#include "feature.h"
#include "mon-util.h"
#include "monster.h"
#include "options.h"
#include "state.h"
#include "terrain.h"
#include "view.h"
#include "viewgeom.h"

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

show_type::show_type()
    : cls(SH_NOTHING), colour(0)
{
    // To quiet Valgrind warnings.
    feat = (dungeon_feature_type) -1;
}

show_type::show_type(const monsters* m)
    : cls(SH_MONSTER), mons(m->type), colour(0) {}

show_type::show_type(dungeon_feature_type f)
    : cls(SH_FEATURE), feat(f), colour(0) {}

static show_item_type _item_to_show_code(const item_def &item);

show_type::show_type(const item_def &it)
    : cls(SH_ITEM), item(_item_to_show_code(it)), colour(0) {}

show_type::show_type(show_item_type t)
    : cls(SH_ITEM), item(t), colour(0) {}

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

void show_def::_update_feat_at(const coord_def &gp, const coord_def &ep)
{
    grid(ep).cls = SH_FEATURE;
    grid(ep).feat = grid_appearance(gp);
    grid(ep).colour = 0;
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
    const item_def &eitem = mitm[igrd(gp)];
    unsigned short &ecol  = grid(ep).colour;

    const dungeon_feature_type feat = grd(gp);
    if (Options.feature_item_brand && is_critical_feature(feat))
        ecol |= COLFLAG_FEATURE_ITEM;
    else if (Options.trap_item_brand && feat_is_trap(feat))
        ecol |= COLFLAG_TRAP_ITEM;
    else
    {
        const unsigned short gcol = env.grid_colours(gp);
        ecol = (feat == DNGN_SHALLOW_WATER) ?
               (gcol != BLACK ? gcol : CYAN) : eitem.colour;
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

bool show_def::update_monster(const monsters* mons)
{
    const coord_def e = grid2show(mons->pos());

    if (!mons->visible_to(&you))
    {
        // ripple effect?
        if (grd(mons->pos()) == DNGN_SHALLOW_WATER
            && !mons_flies(mons)
            && env.cgrid(mons->pos()) == EMPTY_CLOUD)
        {
            _set_backup(e);
            grid(e).cls = SH_INVIS_EXPOSED;

            // Translates between colours used for shallow and deep water,
            // if not using the normal LIGHTCYAN / BLUE. The ripple uses
            // the deep water colour.
            unsigned short base_colour = env.grid_colours(mons->pos());

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
        return (false);
    }

    // Mimics are always left on map.
    if (!mons_is_mimic(mons->type))
        _set_backup(e);

    grid(e).cls = SH_MONSTER;
    grid(e).mons = mons->type;
    grid(e).colour = get_mons_colour(mons);

    return (true);
}

void show_def::update_at(const coord_def &gp, const coord_def &ep)
{
    grid(ep).cls = SH_NOTHING;

    // The sequence is grid, items, clouds, monsters.
    _update_feat_at(gp, ep);

    if (igrd(gp) != NON_ITEM)
        _update_item_at(gp, ep);

    const int cloud = env.cgrid(gp);
    if (cloud != EMPTY_CLOUD && env.cloud[cloud].type != CLOUD_NONE
        && env.cloud[cloud].pos == gp)
    {
        _update_cloud(cloud);
    }

    const monsters *mons = monster_at(gp);
    if (mons && mons->alive())
        update_monster(mons);
}

void show_def::init()
{
    grid.init(show_type());
    backup.init(show_type());

    for (radius_iterator ri(you.pos(), LOS_RADIUS); ri; ++ri)
        update_at(*ri, grid2show(*ri));
}
