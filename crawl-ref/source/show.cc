/**
 * @file
 * @brief Updates the screen via map_knowledge.
**/

#include "AppHdr.h"

#include "mpr.h"
#include "show.h"

#include "areas.h"
#include "cloud.h"
#include "coord.h"
#include "coordit.h"
#include "dgn-event.h"
#include "dgn-overview.h"
#include "dungeon.h"
#include "god-companions.h" // Beogh Blood for Blood markers
#include "item-prop.h"
#include "level-state-type.h"
#include "libutil.h"
#include "map-knowledge.h"
#include "mon-place.h"
#include "state.h"
#include "tag-version.h"
#include "terrain.h"
#include "rltiles/tiledef-main.h"
#ifdef USE_TILE
 #include "tileview.h"
#endif
#include "tiles-build-specific.h"
#include "traps.h"
#include "travel.h"
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
        return false;
    else if (cls > other.cls)
        return true;
    switch (cls)
    {
    case SH_FEATURE:
        return feat < other.feat;
    case SH_ITEM:
        return item < other.item;
    case SH_MONSTER:
        return mons < other.mons;
    default:
        return false;
    }
}

// Returns true if this is a monster that can be hidden for ^C
// purposes. All non-immobile monsters are removed from the map
// when the clear-map command (^C) is used.
bool show_type::is_cleanable_monster() const
{
    return cls == SH_MONSTER && !mons_class_is_stationary(mons);
}

static void _update_feat_at(const coord_def &gp)
{
    dungeon_feature_type feat = env.grid(gp);
    unsigned colour = env.grid_colours(gp);
    trap_type trap = TRAP_UNASSIGNED;
    if (feat_is_trap(feat))
        trap = get_trap_type(gp);

    env.map_knowledge(gp).set_feature(feat, colour, trap);

    if (haloed(gp))
        env.map_knowledge(gp).flags |= MAP_HALOED;

    if (umbraed(gp))
        env.map_knowledge(gp).flags |= MAP_UMBRAED;

    if (silenced(gp))
        env.map_knowledge(gp).flags |= MAP_SILENCED;

    if (liquefied(gp, true))
        env.map_knowledge(gp).flags |= MAP_LIQUEFIED;

    if (orb_haloed(gp))
        env.map_knowledge(gp).flags |= MAP_ORB_HALOED;

    if (quad_haloed(gp))
        env.map_knowledge(gp).flags |= MAP_QUAD_HALOED;

    if (disjunction_haloed(gp))
        env.map_knowledge(gp).flags |= MAP_DISJUNCT;

    if (is_sanctuary(gp))
    {
        if (testbits(env.pgrid(gp), FPROP_SANCTUARY_1))
            env.map_knowledge(gp).flags |= MAP_SANCTUARY_1;
        else if (testbits(env.pgrid(gp), FPROP_SANCTUARY_2))
            env.map_knowledge(gp).flags |= MAP_SANCTUARY_2;
    }

    if (is_blasphemy(gp))
        env.map_knowledge(gp).flags |= MAP_BLASPHEMY;

    if (you.get_beholder(gp))
        env.map_knowledge(gp).flags |= MAP_WITHHELD;

    if (you.get_fearmonger(gp))
        env.map_knowledge(gp).flags |= MAP_WITHHELD;

    if (you.is_nervous() && you.see_cell(gp)
        && (!monster_at(gp) || !monster_at(gp)->visible_to(&you)))
    {
        env.map_knowledge(gp).flags |= MAP_WITHHELD;
    }

    if ((feat_is_stone_stair(feat)
         || feat_is_escape_hatch(feat))
        && is_exclude_root(gp))
    {
        env.map_knowledge(gp).flags |= MAP_EXCLUDED_STAIRS;
    }

    if (is_bloodcovered(gp))
        env.map_knowledge(gp).flags |= MAP_BLOODY;

    if (env.level_state & LSTATE_SLIMY_WALL && slime_wall_neighbour(gp))
        env.map_knowledge(gp).flags |= MAP_CORRODING;

    // We want to give non-solid terrain and the icy walls themselves MAP_ICY
    // so we can properly recolor both.
    if (env.level_state & LSTATE_ICY_WALL
        && (is_icecovered(gp)
            || !feat_is_wall(feat)
                && count_adjacent_icy_walls(gp)
                && you.see_cell_no_trans(gp)))
    {
        env.map_knowledge(gp).flags |= MAP_ICY;
    }

    // XXX: This feels like is should be more separated somehow...
    if (you.religion == GOD_BEOGH
        && you.props.exists(BEOGH_RES_PIETY_NEEDED_KEY)
        && tile_has_valid_bfb_corpse(gp))
    {
        env.map_knowledge(gp).flags |= MAP_BFB_CORPSE;
    }

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
    case OBJ_ORBS:       return SHOW_ITEM_ORB;
    case OBJ_WEAPONS:    return SHOW_ITEM_WEAPON;
    case OBJ_MISSILES:   return SHOW_ITEM_MISSILE;
    case OBJ_ARMOUR:     return SHOW_ITEM_ARMOUR;
    case OBJ_WANDS:      return SHOW_ITEM_WAND;
#if TAG_MAJOR_VERSION == 34
    case OBJ_FOOD:       return SHOW_ITEM_FOOD;
#endif
    case OBJ_SCROLLS:    return SHOW_ITEM_SCROLL;
    case OBJ_JEWELLERY:
        return jewellery_is_amulet(item) ? SHOW_ITEM_AMULET : SHOW_ITEM_RING;
    case OBJ_POTIONS:    return SHOW_ITEM_POTION;
    case OBJ_BOOKS:      return SHOW_ITEM_BOOK;
    case OBJ_STAVES:     return SHOW_ITEM_STAFF;
#if TAG_MAJOR_VERSION == 34
    case OBJ_RODS:       return SHOW_ITEM_ROD;
#endif
    case OBJ_MISCELLANY: return SHOW_ITEM_MISCELLANY;
    case OBJ_TALISMANS:  return SHOW_ITEM_TALISMAN;
    case OBJ_CORPSES:
        if (item.sub_type == CORPSE_SKELETON)
            return SHOW_ITEM_SKELETON;
        else
            return SHOW_ITEM_CORPSE;
    case OBJ_GOLD:       return SHOW_ITEM_GOLD;
    case OBJ_GEMS:       return SHOW_ITEM_GEM;
    case OBJ_DETECTED:   return SHOW_ITEM_DETECTED;
    case OBJ_RUNES:      return SHOW_ITEM_RUNE;
    default:             return SHOW_ITEM_ORB; // bad item character
    }
}

void update_item_at(const coord_def &gp, bool wizard)
{
    if (!in_bounds(gp))
        return;

    item_def eitem;
    bool more_items = false;

    if (you.see_cell(gp) || wizard)
    {
        const int item_grid = wizard ? env.igrid(gp) : you.visible_igrd(gp);
        if (item_grid == NON_ITEM)
            return;
        eitem = env.item[item_grid];

        // monster(mimic)-owned items have link = NON_ITEM+1+midx
        if (eitem.link > NON_ITEM)
            more_items = true;
        else if (eitem.link < NON_ITEM && !crawl_state.game_is_arena())
            more_items = true;

        if (wizard)
            StashTrack.add_stash(gp);
    }
    else
    {
        const vector<item_def> stash = item_list_in_stash(gp);
        if (stash.empty())
            return;

        eitem = stash[0];
        if (stash.size() > 1)
            more_items = true;
    }
    env.map_knowledge(gp).set_item(eitem, more_items);
}

static void _update_cloud(cloud_struct& cloud)
{
    const coord_def gp = cloud.pos;

    int dur = cloud.decay/20;
    if (dur < 0)
        dur = 0;
    else if (dur > 3)
        dur = 3;

    cloud_info ci(cloud.type, get_cloud_colour(cloud), dur, 0, gp,
                  cloud.killer);
    env.map_knowledge(gp).set_cloud(ci);
}

static void _check_monster_pos(const monster* mons)
{
    int s = mons->mindex();
    ASSERT(env.mgrid(mons->pos()) == s);

    // [rob] The following in case asserts aren't enabled.
    // [enne] - It's possible that env.mgrid and mons->x/y are out of
    // sync because they are updated separately. If we can see this
    // monster, then make sure that the env.mgrid is set correctly.
    if (env.mgrid(mons->pos()) != s)
    {
        // If this mprf triggers for you, please note any special
        // circumstances so we can track down where this is coming
        // from.
        mprf(MSGCH_ERROR, "monster %s (%d) at (%d, %d) was "
             "improperly placed. Updating env.mgrid.",
             mons->name(DESC_PLAIN, true).c_str(), s,
             mons->pos().x, mons->pos().y);
        env.mgrid(mons->pos()) = s;
    }
}

static int _hashed_rand(const monster* mons, uint32_t id, uint32_t die)
{
    if (die <= 1)
        return 0;

    struct
    {
        uint32_t mid;
        uint32_t id;
        uint32_t seed;
    } data;
    data.mid = mons->mid;
    data.id  = id;
    data.seed = you.attribute[ATTR_SEEN_INVIS_SEED];

    return hash32(&data, sizeof(data)) % die;
}

/**
 * Update map knowledge for monsters
 *
 * This function updates the map_knowledge grid with a monster_info if relevant.
 * If the monster is not currently visible to the player, the map knowledge will
 * be updated with a disturbance if necessary.
 * @param mons  The monster at the relevant location.
**/
static void _update_monster(monster* mons)
{
    _check_monster_pos(mons);
    const coord_def gp = mons->pos();

    if (mons->visible_to(&you))
    {
        mons->ensure_has_client_id();
        monster_info mi(mons);
        env.map_knowledge(gp).set_monster(mi);
        return;
    }

    // From here on we're handling an invisible monster, possibly leaving an
    // invisible monster indicator.

    // We cannot use regular randomness here, otherwise redrawing the screen
    // would give out the real position. We need to save the seed too -- but it
    // needs to be regenerated every turn.
    if (you.attribute[ATTR_SEEN_INVIS_TURN] != you.num_turns)
    {
        you.attribute[ATTR_SEEN_INVIS_TURN] = you.num_turns;
        you.attribute[ATTR_SEEN_INVIS_SEED] = rng::get_uint32();
    }

    // Show an invis indicator for a revealed monster until it moves from where
    // at was revealed, but make sure to show it for at least one turn.
    if (!you.turn_is_over)
        mons->revealed_this_turn = false;
    else if (!mons->revealed_this_turn && mons->revealed_at_pos != gp)
        mons->revealed_at_pos.reset();

    if (!mons->revealed_at_pos.origin())
    {
        env.map_knowledge(gp).set_invisible_monster();
        mons->revealed_at_pos = gp;
        return;
    }

    // Ripple effect?
    // Should match directn.cc's _mon_exposed
    if (env.grid(gp) == DNGN_SHALLOW_WATER
            && !mons->airborne()
            && !cloud_at(gp)
        || cloud_at(gp) && is_opaque_cloud(cloud_at(gp)->type)
            && !mons->is_insubstantial())
    {
        env.map_knowledge(gp).set_invisible_monster();
        mons->revealed_at_pos = gp;
        return;
    }

    int range = player_monster_detect_radius();
    if (mons->constricted_by == MID_PLAYER
        || (range > 0 && (you.pos() - mons->pos()).rdist() <= range))
    {
        env.map_knowledge(gp).set_invisible_monster();
        mons->revealed_at_pos = gp;
        return;
    }

    // 1/7 chance to leave an invis indicator at the real position.
    if (!_hashed_rand(mons, 0, 7))
    {
        env.map_knowledge(gp).set_invisible_monster();
        mons->revealed_at_pos = gp;
    }
}

/**
 * Updates the map knowledge at a location.
 * @param gp      The location to update.
 * @param layers  The information layers to display.
**/
void show_update_at(const coord_def &gp, layers_type layers)
{
    if (you.see_cell(gp))
        env.map_knowledge(gp).clear_data();
    else if (!env.map_knowledge(gp).known())
        return;
    else
        env.map_knowledge(gp).clear_monster();

    force_show_update_at(gp, layers);
}

void force_show_update_at(const coord_def &gp, layers_type layers)
{
    // The sequence is grid, items, clouds, monsters.
    // XX it actually seems to be grid monsters clouds items??
    _update_feat_at(gp);
    if (!in_bounds(gp))
        return;

    if (layers & Layer::MONSTERS)
    {
        monster* mons = monster_at(gp);
        if (mons && mons->alive())
            _update_monster(mons);
    }

    if (layers & Layer::CLOUDS)
        if (cloud_struct* cloud = cloud_at(gp))
            _update_cloud(*cloud);

    if (layers & Layer::ITEMS)
        update_item_at(gp);
}

void show_init(layers_type layers)
{
    clear_terrain_visibility();
    if (crawl_state.game_is_arena())
    {
        for (rectangle_iterator ri(crawl_view.vgrdc, LOS_MAX_RANGE); ri; ++ri)
            show_update_at(*ri, layers);
        return;
    }

    ASSERT(you.on_current_level);

    for (vision_iterator ri(you); ri; ++ri)
        show_update_at(*ri, layers);
}

// Emphasis may change while off-level. This catches up.
// It should be equivalent to looping over the whole map
// and setting MAP_EMPHASIZE for any coordinate with
// emphasise(p) == true, but we optimise a bit.
void show_update_emphasis()
{
    // The only thing that can change is that previously unknown
    // stairs are now known. (see is_unknown_stair(), emphasise())
    LevelInfo& level_info = travel_cache.get_level_info(level_id::current());
    vector<stair_info> stairs = level_info.get_stairs();
    for (const stair_info &stair : stairs)
        if (stair.destination.is_valid())
            env.map_knowledge(stair.position).flags &= ~MAP_EMPHASIZE;

    vector<transporter_info> transporters = level_info.get_transporters();
    for (const transporter_info &transporter: transporters)
        if (!transporter.destination.origin())
            env.map_knowledge(transporter.position).flags &= ~MAP_EMPHASIZE;
}
