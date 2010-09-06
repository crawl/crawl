#include "AppHdr.h"

#include <stdint.h>

#include "show.h"

#include "areas.h"
#include "cloud.h"
#include "coord.h"
#include "coordit.h"
#include "dgnevent.h"
#include "dgn-overview.h"
#include "dungeon.h"
#include "env.h"
#include "exclude.h"
#include "fprop.h"
#include "itemprop.h"
#include "mon-stuff.h"
#include "mon-util.h"
#include "monster.h"
#include "options.h"
#include "random.h"
#include "showsymb.h"
#include "state.h"
#include "stuff.h"
#include "areas.h"
#include "terrain.h"
#ifdef USE_TILE
 #include "tileview.h"
#endif
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

// Returns true if this is a monster that can be hidden for clean_map
// purposes. All non-immobile monsters are hidden when out of LOS with
// Options.clean_map, or removed from the map when the clear-map
// command (^C) is used.
bool show_type::is_cleanable_monster() const
{
    return (cls == SH_MONSTER && !mons_class_is_stationary(mons));
}

static unsigned short _tree_colour(const coord_def& where)
{
    uint32_t h = where.x;
    h+=h<<10; h^=h>>6;
    h += where.y;
    h+=h<<10; h^=h>>6;
    h+=h<<3; h^=h>>11; h+=h<<15;
    return (h>>30) ? GREEN :
           (you.where_are_you == BRANCH_SWAMP) ? BROWN
                   : LIGHTGREEN;
}

static void _update_feat_at(const coord_def &gp)
{
    dungeon_feature_type feat = grid_appearance(gp);
    unsigned colour = env.grid_colours(gp);
    if (feat == DNGN_TREE && !colour)
        colour = _tree_colour(gp);
    env.map_knowledge(gp).set_feature(feat, colour);

    if (haloed(gp))
        env.map_knowledge(gp).flags |= MAP_HALOED;

    if (silenced(gp))
        env.map_knowledge(gp).flags |= MAP_SILENCED;

    if (is_sanctuary(gp))
    {
        if (testbits(env.pgrid(gp), FPROP_SANCTUARY_1))
            env.map_knowledge(gp).flags |= MAP_SANCTUARY_1;
        else if (testbits(env.pgrid(gp), FPROP_SANCTUARY_2))
            env.map_knowledge(gp).flags |= MAP_SANCTUARY_2;
    }

    if (you.get_beholder(gp))
        env.map_knowledge(gp).flags |= MAP_WITHHELD;

    if (feat >= DNGN_STONE_STAIRS_DOWN_I
                            && feat <= DNGN_ESCAPE_HATCH_UP
                            && is_exclude_root(gp))
        env.map_knowledge(gp).flags |= MAP_EXCLUDED_STAIRS;

    if (is_bloodcovered(gp))
        env.map_knowledge(gp).flags |= MAP_BLOODY;

    if (is_moldy(gp))
    {
        env.map_knowledge(gp).flags |= MAP_MOLDY;
        if (glowing_mold(gp))
            env.map_knowledge(gp).flags |= MAP_GLOWING_MOLDY;
    }

    if (slime_wall_neighbour(gp))
        env.map_knowledge(gp).flags |= MAP_CORRODING;

    if (emphasise(gp))
        env.map_knowledge(gp).flags |= MAP_EMPHASIZE;

    // Tell the world first.
    dungeon_events.fire_position_event(DET_PLAYER_IN_LOS, gp);

    if (is_notable_terrain(feat))
        seen_notable_thing(feat, gp);

    dgn_seen_vault_at(gp);
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

static void _update_item_at(const coord_def &gp)
{
    const item_def *eitem;
    bool more_items = false;
    // Check for mimics.
    const monster* m = monster_at(gp);
    if (m && mons_is_unknown_mimic(m))
        eitem = &get_mimic_item(m);
    else if (you.visible_igrd(gp) != NON_ITEM)
        eitem = &mitm[you.visible_igrd(gp)];
    else
        return;

    // monster(mimic)-owned items have link = NON_ITEM+1+midx
    if (eitem->link > NON_ITEM && you.visible_igrd(gp) != NON_ITEM)
        more_items = true;
    else if (eitem->link < NON_ITEM && !crawl_state.game_is_arena())
        more_items = true;

    env.map_knowledge(gp).set_item(get_item_info(*eitem), more_items);

#ifdef USE_TILE
    if (feat_is_stair(env.grid(gp)))
        tile_place_item_marker(grid2show(gp), *eitem);
    else
        tile_place_item(grid2show(gp), *eitem);
#endif
}

static void _update_cloud(int cloudno)
{
    const coord_def gp = env.cloud[cloudno].pos;
    cloud_type cloud = env.cloud[cloudno].type;
    env.map_knowledge(gp).set_cloud(cloud, get_cloud_colour(cloudno));

#ifdef USE_TILE
    tile_place_cloud(gp, env.cloud[cloudno]);
#endif
}

static void _check_monster_pos(const monster* mons)
{
    int s = mons->mindex();
    ASSERT(mgrd(mons->pos()) == s);

    // [rob] The following in case asserts aren't enabled.
    // [enne] - It's possible that mgrd and mons->x/y are out of
    // sync because they are updated separately.  If we can see this
    // monster, then make sure that the mgrd is set correctly.
    if (mgrd(mons->pos()) != s)
    {
        // If this mprf triggers for you, please note any special
        // circumstances so we can track down where this is coming
        // from.
        mprf(MSGCH_ERROR, "monster %s (%d) at (%d, %d) was "
             "improperly placed.  Updating mgrd.",
             mons->name(DESC_PLAIN, true).c_str(), s,
             mons->pos().x, mons->pos().y);
        mgrd(mons->pos()) = s;
    }
}

static void _update_monster(const monster* mons)
{
    _check_monster_pos(mons);

    const coord_def gp = mons->pos();

    if (mons_is_unknown_mimic(mons))
    {
#ifdef USE_TILE
        tile_place_monster(mons->pos(), mons);
#endif
        return;
    }

    if (!mons->visible_to(&you))
    {
        // ripple effect?
        if ((grd(gp) == DNGN_SHALLOW_WATER
            && !mons_flies(mons)
            && env.cgrid(gp) == EMPTY_CLOUD)
            ||
            (is_opaque_cloud(env.cgrid(gp))
                 && !mons->submerged()
                 && !mons->is_insubstantial())
           )
            env.map_knowledge(gp).set_invisible_monster();
        return;
    }

    monster_info mi(mons);
    env.map_knowledge(gp).set_monster(mi);

#ifdef USE_TILE
    tile_place_monster(mons->pos(), mons);
#endif
}

void show_update_at(const coord_def &gp, bool terrain_only)
{
    env.map_knowledge(gp).clear_data();

    // The sequence is grid, items, clouds, monsters.
    _update_feat_at(gp);

    if (terrain_only)
        return;

    // If there's items on the boundary (shop inventory),
    // we don't show them.
    if (!in_bounds(gp))
        return;

    _update_item_at(gp);

    const int cloud = env.cgrid(gp);
    if (cloud != EMPTY_CLOUD && env.cloud[cloud].type != CLOUD_NONE
        && env.cloud[cloud].pos == gp)
    {
        _update_cloud(cloud);
    }

    const monster* mons = monster_at(gp);
    if (mons && mons->alive())
        _update_monster(mons);

    set_terrain_visible(gp);
}

void show_init(bool terrain_only)
{
    clear_terrain_visibility();
    for (radius_iterator ri(you.get_los()); ri; ++ri)
        show_update_at(*ri, terrain_only);
}
