/**
 * @file
 * @brief Terrain related functions.
**/

#include "AppHdr.h"

#include "terrain.h"

#include <algorithm>
#include <functional>
#include <sstream>

#include "areas.h"
#include "attack.h"
#include "branch.h"
#include "cloud.h"
#include "coord.h"
#include "coordit.h"
#include "dgn-event.h"
#include "dgn-overview.h"
#include "directn.h"
#include "dungeon.h"
#include "env.h"
#include "tile-env.h"
#include "fight.h"
#include "feature.h"
#include "fprop.h"
#include "god-abil.h"
#include "item-prop.h"
#include "items.h"
#include "level-state-type.h"
#include "libutil.h"
#include "losglobal.h"
#include "map-knowledge.h" // set_terrain_visible
#include "mapmark.h"
#include "message.h"
#include "mon-behv.h"
#include "mon-place.h"
#include "mon-poly.h"
#include "mon-util.h"
#include "ouch.h"
#include "player.h"
#include "random.h"
#include "religion.h"
#include "species.h"
#include "spl-damage.h" // ramparts_damage
#include "spl-transloc.h"
#include "state.h"
#include "stringutil.h"
#include "tag-version.h"
#include "tileview.h"
#include "transform.h"
#include "traps.h"
#include "travel.h"
#include "viewchar.h"
#include "view.h"

static bool _revert_terrain_to(coord_def pos, dungeon_feature_type feat);

actor* actor_at(const coord_def& c)
{
    if (!in_bounds(c))
        return nullptr;
    if (c == you.pos())
        return &you;
    return monster_at(c);
}

/** Can a malign gateway be placed on this feature?
 */
bool feat_is_malign_gateway_suitable(dungeon_feature_type feat)
{
    return feat == DNGN_FLOOR || feat == DNGN_SHALLOW_WATER;
}

/** Is this feature a type of wall?
 */
bool feat_is_wall(dungeon_feature_type feat)
{
    return get_feature_def(feat).flags & FFT_WALL;
}

/** Is this feature one of the main stone downstairs of a level?
 */
bool feat_is_stone_stair_down(dungeon_feature_type feat)
{
     return feat == DNGN_STONE_STAIRS_DOWN_I
            || feat == DNGN_STONE_STAIRS_DOWN_II
            || feat == DNGN_STONE_STAIRS_DOWN_III;
}

/** Is this feature one of the main stone upstairs of a level?
 */
bool feat_is_stone_stair_up(dungeon_feature_type feat)
{
    return feat == DNGN_STONE_STAIRS_UP_I
           || feat == DNGN_STONE_STAIRS_UP_II
           || feat == DNGN_STONE_STAIRS_UP_III;
}

/** Is this feature one of the main stone stairs of a level?
 */
bool feat_is_stone_stair(dungeon_feature_type feat)
{
    return feat_is_stone_stair_up(feat) || feat_is_stone_stair_down(feat);
}

/** Is it possible to call this feature a staircase? (purely cosmetic)
 */
bool feat_is_staircase(dungeon_feature_type feat)
{
    if (feat_is_stone_stair(feat))
        return true;

    // All branch entries/exits are staircases, except for Zot, Hell & Vaults.
    if (feat == DNGN_ENTER_VAULTS
        || feat == DNGN_EXIT_VAULTS
        || feat == DNGN_ENTER_HELL || feat == DNGN_EXIT_HELL
        || feat_is_hell_subbranch_exit(feat)
        || feat == DNGN_ENTER_ZOT
        || feat == DNGN_EXIT_ZOT)
    {
        return false;
    }

    return feat_is_branch_entrance(feat)
           || feat_is_branch_exit(feat)
           || feat == DNGN_ABYSSAL_STAIR;
}

/** Is this the exit from a sub-branch of Hell?
 */
bool feat_is_hell_subbranch_exit(dungeon_feature_type feat)
{
    static bool init, cached[NUM_FEATURES+1];
    if (!init)
    {
        for (branch_iterator it; it; ++it)
        {
            if (is_hell_subbranch(it->id))
                cached[it->exit_stairs] = true;
        }
        init = true;
    }
    return cached[feat];
}

/**
 * Define a memoized function from dungeon_feature_type to bool.
 * This macro should be followed by the non-memoized version of the
 * function body: see feat_is_branch_entrance below for an example.
 *
 * @param funcname The name of the function to define.
 * @param paramname The name under which the function's single parameter,
 *        of type dungeon_feature_type, is visible in the function body.
 */
#define FEATFN_MEMOIZED(funcname, paramname) \
    static bool _raw_ ## funcname (dungeon_feature_type); \
    bool funcname (dungeon_feature_type feat) \
    { \
        static int cached[NUM_FEATURES+1] = { 0 }; \
        if (!cached[feat]) cached[feat] = _raw_ ## funcname (feat) ? 1 : -1; \
        return cached[feat] > 0; \
    } \
    static bool _raw_ ## funcname (dungeon_feature_type paramname)

/** Is this feature a branch entrance that should show up on ^O?
 */
FEATFN_MEMOIZED(feat_is_branch_entrance, feat)
{
    for (branch_iterator it; it; ++it)
    {
        if (it->entry_stairs == feat
            && is_connected_branch(it->id))
        {
            return true;
        }
    }

    return false;
}

/** Counterpart to feat_is_branch_entrance.
 */
FEATFN_MEMOIZED(feat_is_branch_exit, feat)
{
    for (branch_iterator it; it; ++it)
    {
        if (it->exit_stairs == feat
            && is_connected_branch(it->id))
        {
            return true;
        }
    }

    return false;
}

/** Is this feature an entrance to a portal branch?
 */
FEATFN_MEMOIZED(feat_is_portal_entrance, feat)
{
    // These are have different rules from normal connected branches, but they
    // also have different rules from "portal vaults," and are more similar to
    // real branches in some respects.
    if (feat == DNGN_ENTER_ABYSS || feat == DNGN_ENTER_PANDEMONIUM)
        return false;

    for (branch_iterator it; it; ++it)
    {
        if (it->entry_stairs == feat
            && !is_connected_branch(it->id))
        {
            return true;
        }
    }
#if TAG_MAJOR_VERSION == 34
    if (feat == DNGN_ENTER_PORTAL_VAULT)
        return true;
#endif

    return false;
}

/** Counterpart to feat_is_portal_entrance.
 */
FEATFN_MEMOIZED(feat_is_portal_exit, feat)
{
    if (feat == DNGN_EXIT_ABYSS || feat == DNGN_EXIT_PANDEMONIUM)
        return false;

    for (branch_iterator it; it; ++it)
    {
        if (it->exit_stairs == feat
            && !is_connected_branch(it->id))
        {
            return true;
        }
    }
#if TAG_MAJOR_VERSION == 34
    if (feat == DNGN_EXIT_PORTAL_VAULT)
        return true;
#endif

    return false;
}

/** Is this feature a kind of portal?
 */
bool feat_is_portal(dungeon_feature_type feat)
{
    return feat == DNGN_MALIGN_GATEWAY
        || feat_is_portal_entrance(feat)
        || feat_is_portal_exit(feat);
}

/** Is this feature a kind of level exit?
 */
bool feat_is_stair(dungeon_feature_type gridc)
{
    return feat_is_travelable_stair(gridc) || feat_is_gate(gridc);
}

/** Is this feature a level exit stair with a consistent endpoint?
 */
bool feat_is_travelable_stair(dungeon_feature_type feat)
{
    return feat_is_stone_stair(feat)
           || feat_is_escape_hatch(feat)
           || feat_is_branch_entrance(feat)
           || feat_is_branch_exit(feat);
}

/** Is this feature an escape hatch?
 */
bool feat_is_escape_hatch(dungeon_feature_type feat)
{
    return feat == DNGN_ESCAPE_HATCH_DOWN
           || feat == DNGN_ESCAPE_HATCH_UP;
}

/** Is this feature a gate?
  * XXX: Why does this matter??
 */
bool feat_is_gate(dungeon_feature_type feat)
{
    if (feat_is_portal_entrance(feat)
        || feat_is_portal_exit(feat)
        || feat_is_hell_subbranch_exit(feat))
    {
        return true;
    }

    switch (feat)
    {
    case DNGN_ENTER_ABYSS:
    case DNGN_EXIT_THROUGH_ABYSS:
    case DNGN_EXIT_ABYSS:
    case DNGN_ABYSSAL_STAIR:
    case DNGN_ENTER_PANDEMONIUM:
    case DNGN_EXIT_PANDEMONIUM:
    case DNGN_TRANSIT_PANDEMONIUM:
    case DNGN_ENTER_VAULTS:
    case DNGN_EXIT_VAULTS:
    case DNGN_ENTER_ZOT:
    case DNGN_EXIT_ZOT:
    case DNGN_ENTER_HELL:
    case DNGN_EXIT_HELL:
    case DNGN_ENTER_DIS:
    case DNGN_ENTER_GEHENNA:
    case DNGN_ENTER_COCYTUS:
    case DNGN_ENTER_TARTARUS:
        return true;
    default:
        return false;
    }
}

/** What command do you use to traverse this feature?
 *
 *  @param feat the feature.
 *  @returns CMD_GO_UPSTAIRS if it's a stair up, CMD_GO_DOWNSTAIRS if it's a
 *           stair down, and CMD_NO_CMD if it can't be used to move.
 */
command_type feat_stair_direction(dungeon_feature_type feat)
{
    if (feat_is_portal_entrance(feat)
        || feat_is_branch_entrance(feat))
    {
        return CMD_GO_DOWNSTAIRS;
    }
    if (feat_is_portal_exit(feat)
        || feat_is_branch_exit(feat))
    {
        return CMD_GO_UPSTAIRS;
    }

    if (feat_is_altar(feat))
        return CMD_GO_DOWNSTAIRS; // arbitrary; consistent with shops

    switch (feat)
    {
    case DNGN_STONE_STAIRS_UP_I:
    case DNGN_STONE_STAIRS_UP_II:
    case DNGN_STONE_STAIRS_UP_III:
    case DNGN_ESCAPE_HATCH_UP:
    case DNGN_EXIT_HELL:
        return CMD_GO_UPSTAIRS;

    case DNGN_STONE_STAIRS_DOWN_I:
    case DNGN_STONE_STAIRS_DOWN_II:
    case DNGN_STONE_STAIRS_DOWN_III:
    case DNGN_ESCAPE_HATCH_DOWN:
    case DNGN_ENTER_ABYSS:
    case DNGN_EXIT_THROUGH_ABYSS:
    case DNGN_EXIT_ABYSS:
    case DNGN_ABYSSAL_STAIR:
    case DNGN_ENTER_PANDEMONIUM:
    case DNGN_EXIT_PANDEMONIUM:
    case DNGN_TRANSIT_PANDEMONIUM:
    case DNGN_TRANSPORTER:
    case DNGN_ENTER_SHOP:
        return CMD_GO_DOWNSTAIRS;

    default:
        return CMD_NO_CMD;
    }
}

/** Can you normally see through this feature?
 */
bool feat_is_opaque(dungeon_feature_type feat)
{
    return get_feature_def(feat).flags & FFT_OPAQUE;
}

/** Can you move into this feature in normal play?
 */
bool feat_is_solid(dungeon_feature_type feat)
{
    return get_feature_def(feat).flags & FFT_SOLID;
}

/** Can you wall jump against this feature? (Wu Jian)?
 */
bool feat_can_wall_jump_against(dungeon_feature_type feat)
{
    return feat_is_wall(feat)
           || feat == DNGN_GRATE
           || feat_is_closed_door(feat)
           || feat_is_tree(feat)
           || feat_is_statuelike(feat);
}

/** Can you move into this cell in normal play?
 */
bool cell_is_solid(const coord_def &c)
{
    return feat_is_solid(env.grid(c));
}

/** Can a human stand on this feature without flying?
 */
bool feat_has_solid_floor(dungeon_feature_type feat)
{
    return !feat_is_solid(feat) && feat != DNGN_DEEP_WATER
           && feat != DNGN_LAVA;
}

/** Is there enough dry floor on this feature to stand without penalty?
 */
bool feat_has_dry_floor(dungeon_feature_type feat)
{
    return feat_has_solid_floor(feat) && !feat_is_water(feat) && feat != DNGN_MUD;
}

/** Is this feature a variety of door?
 */
bool feat_is_door(dungeon_feature_type feat)
{
    return feat_is_closed_door(feat) || feat_is_open_door(feat);
}

/** Is this feature a variety of closed door?
 */
bool feat_is_closed_door(dungeon_feature_type feat)
{
    return feat == DNGN_CLOSED_DOOR
           || feat == DNGN_CLOSED_CLEAR_DOOR
           || feat_is_runed(feat)
           || feat == DNGN_SEALED_DOOR
           || feat == DNGN_SEALED_CLEAR_DOOR;
}

/** Is this feature a variety of open door?
 */
bool feat_is_open_door(dungeon_feature_type feat)
{
    return feat == DNGN_OPEN_DOOR
        || feat == DNGN_OPEN_CLEAR_DOOR
        || feat == DNGN_BROKEN_DOOR
        || feat == DNGN_BROKEN_CLEAR_DOOR;
}

/** Has this feature been sealed by a vault warden?
 */
bool feat_is_sealed(dungeon_feature_type feat)
{
    return feat == DNGN_SEALED_STAIRS_DOWN
        || feat == DNGN_SEALED_STAIRS_UP
        || feat == DNGN_SEALED_DOOR
        || feat == DNGN_SEALED_CLEAR_DOOR;
}

/** Is this feature a type of runed door?
 */
bool feat_is_runed(dungeon_feature_type feat)
{
    return feat == DNGN_RUNED_DOOR || feat == DNGN_RUNED_CLEAR_DOOR;
}

/** Is the original feature at this position runed, as in a runed door?
 */
bool cell_is_runed(const coord_def &p)
{
    // the orig_terrain call will check the actual terrain if there's no change
    return feat_is_runed(orig_terrain(p));
}

/** Is this feature a type of statue, i.e., granite or an idol?
 */
bool feat_is_statuelike(dungeon_feature_type feat)
{
    return feat == DNGN_ORCISH_IDOL
      || feat == DNGN_GRANITE_STATUE
      || feat == DNGN_METAL_STATUE;
}

/** Is this feature permanent, unalterable rock?
 */
bool feat_is_permarock(dungeon_feature_type feat)
{
    return feat == DNGN_PERMAROCK_WALL || feat == DNGN_CLEAR_PERMAROCK_WALL;
}

/** Is this feature an open expanse used only as a map border?
 */
bool feat_is_endless(dungeon_feature_type feat)
{
    return feat == DNGN_OPEN_SEA || feat == DNGN_LAVA_SEA
           || feat == DNGN_ENDLESS_SALT;
}

/** Can this feature be dug?
 */
bool feat_is_diggable(dungeon_feature_type feat)
{
    return feat == DNGN_ROCK_WALL || feat == DNGN_CLEAR_ROCK_WALL
           || feat == DNGN_SLIMY_WALL || feat == DNGN_GRATE
           || feat == DNGN_ORCISH_IDOL || feat == DNGN_GRANITE_STATUE
           || feat == DNGN_PETRIFIED_TREE;
}

/** Is this feature a type of trap?
 *
 *  @param feat the feature.
 *  @returns true if it's a trap.
 */
bool feat_is_trap(dungeon_feature_type feat)
{
    if (!is_valid_feature_type(feat))
        return false; // ???
    return get_feature_def(feat).flags & FFT_TRAP;
}

/** Is this feature a type of water, with the concomitant dangers/bonuses?
 */
bool feat_is_water(dungeon_feature_type feat)
{
    return feat == DNGN_SHALLOW_WATER
           || feat == DNGN_DEEP_WATER
           || feat == DNGN_OPEN_SEA
           || feat == DNGN_TOXIC_BOG
           || feat == DNGN_MANGROVE;
}

/** Is this feature a kind of lava?
 */
bool feat_is_lava(dungeon_feature_type feat)
{
    return feat == DNGN_LAVA || feat == DNGN_LAVA_SEA;
}

static const pair<god_type, dungeon_feature_type> _god_altars[] =
{
    { GOD_ZIN, DNGN_ALTAR_ZIN },
    { GOD_SHINING_ONE, DNGN_ALTAR_SHINING_ONE },
    { GOD_KIKUBAAQUDGHA, DNGN_ALTAR_KIKUBAAQUDGHA },
    { GOD_YREDELEMNUL, DNGN_ALTAR_YREDELEMNUL },
    { GOD_XOM, DNGN_ALTAR_XOM },
    { GOD_VEHUMET, DNGN_ALTAR_VEHUMET },
    { GOD_OKAWARU, DNGN_ALTAR_OKAWARU },
    { GOD_MAKHLEB, DNGN_ALTAR_MAKHLEB },
    { GOD_SIF_MUNA, DNGN_ALTAR_SIF_MUNA },
    { GOD_TROG, DNGN_ALTAR_TROG },
    { GOD_NEMELEX_XOBEH, DNGN_ALTAR_NEMELEX_XOBEH },
    { GOD_ELYVILON, DNGN_ALTAR_ELYVILON },
    { GOD_LUGONU, DNGN_ALTAR_LUGONU },
    { GOD_BEOGH, DNGN_ALTAR_BEOGH },
    { GOD_JIYVA, DNGN_ALTAR_JIYVA },
    { GOD_FEDHAS, DNGN_ALTAR_FEDHAS },
    { GOD_CHEIBRIADOS, DNGN_ALTAR_CHEIBRIADOS },
    { GOD_ASHENZARI, DNGN_ALTAR_ASHENZARI },
    { GOD_DITHMENOS, DNGN_ALTAR_DITHMENOS },
    { GOD_GOZAG, DNGN_ALTAR_GOZAG },
    { GOD_QAZLAL, DNGN_ALTAR_QAZLAL },
    { GOD_RU, DNGN_ALTAR_RU },
#if TAG_MAJOR_VERSION == 34
    { GOD_PAKELLAS, DNGN_ALTAR_PAKELLAS },
#endif
    { GOD_USKAYAW, DNGN_ALTAR_USKAYAW },
    { GOD_HEPLIAKLQANA, DNGN_ALTAR_HEPLIAKLQANA },
    { GOD_WU_JIAN, DNGN_ALTAR_WU_JIAN },
    { GOD_IGNIS, DNGN_ALTAR_IGNIS },
    { GOD_ECUMENICAL, DNGN_ALTAR_ECUMENICAL },
};

COMPILE_CHECK(ARRAYSZ(_god_altars) == NUM_GODS );

/** Whose altar is this feature?
 *
 *  @param feat the feature.
 *  @returns GOD_NO_GOD if not an altar, otherwise the god_type of the god.
 */
god_type feat_altar_god(dungeon_feature_type feat)
{
    for (const auto &altar : _god_altars)
        if (altar.second == feat)
            return altar.first;

    return GOD_NO_GOD;
}

/** What feature is the altar of this god?
 *
 *  @param god the god.
 *  @returns DNGN_FLOOR for an invalid god, the god's altar otherwise.
 */
dungeon_feature_type altar_for_god(god_type god)
{
    for (const auto &altar : _god_altars)
        if (altar.first == god)
            return altar.second;

    return DNGN_FLOOR;
}

/** Is this feature an altar to any god?
 */
FEATFN_MEMOIZED(feat_is_altar, grid)
{
    return feat_altar_god(grid) != GOD_NO_GOD;
}

/** Is this feature an altar to the player's god?
 *
 *  @param feat the feature.
 *  @returns true if the player has a god and this is its altar.
 */
bool feat_is_player_altar(dungeon_feature_type grid)
{
    return !you_worship(GOD_NO_GOD) && you_worship(feat_altar_god(grid));
}

/** Is this feature a tree?
 */
bool feat_is_tree(dungeon_feature_type feat)
{
    return feat == DNGN_TREE || feat == DNGN_MANGROVE
        || feat == DNGN_PETRIFIED_TREE || feat == DNGN_DEMONIC_TREE;
}

/** Is this feature flammable?
 */
bool feat_is_flammable(dungeon_feature_type feat)
{
    return feat == DNGN_TREE || feat == DNGN_MANGROVE
        || feat == DNGN_DEMONIC_TREE;
}


/** Is this feature made of metal?
 */
bool feat_is_metal(dungeon_feature_type feat)
{
    return feat == DNGN_METAL_WALL || feat == DNGN_GRATE;
}

/** Is this feature ambivalent about whether we're going up or down?
 */
bool feat_is_bidirectional_portal(dungeon_feature_type feat)
{
    return get_feature_dchar(feat) == DCHAR_ARCH
           && feat_stair_direction(feat) != CMD_NO_CMD
           && !feat_is_hell_subbranch_exit(feat)
           && feat != DNGN_ENTER_ZOT
           && feat != DNGN_EXIT_ZOT
           && feat != DNGN_ENTER_VAULTS
           && feat != DNGN_EXIT_VAULTS
           && feat != DNGN_EXIT_HELL
           && feat != DNGN_ENTER_HELL;
}
/** Will this stair-like feature stick around after the player stops using it? (In descent mode?)
 */
bool feat_is_descent_exitable(dungeon_feature_type feat)
{
    return feat == DNGN_EXIT_DUNGEON
           || feat == DNGN_EXIT_HELL
           || feat == DNGN_EXIT_COCYTUS
           || feat == DNGN_EXIT_GEHENNA
           || feat == DNGN_EXIT_TARTARUS
           || feat == DNGN_EXIT_DIS;
}

/** Is this feature a type of fountain?
 */
bool feat_is_fountain(dungeon_feature_type feat)
{
    return feat == DNGN_FOUNTAIN_BLUE
           || feat == DNGN_FOUNTAIN_SPARKLING
           || feat == DNGN_FOUNTAIN_BLOOD
           || feat == DNGN_FOUNTAIN_EYES
           || feat == DNGN_DRY_FOUNTAIN;
}

/** Is this feature a pile of food?
 */
bool feat_is_food(dungeon_feature_type feat)
{
    return feat == DNGN_CACHE_OF_FRUIT
           || feat == DNGN_CACHE_OF_MEAT
           || feat == DNGN_CACHE_OF_BAKED_GOODS;
}

/** Is this feature non-solid enough that you can reach past it?
 */
bool feat_is_reachable_past(dungeon_feature_type feat)
{
    return !feat_is_opaque(feat)
        && !feat_is_wall(feat)
        && !feat_is_closed_door(feat)
        && feat != DNGN_GRATE;
}

/** Is this feature important to the game?
 *
 *  @param feat the feature.
 *  @returns true for altars, stairs/portals, and malign gateways (???).
 */
FEATFN_MEMOIZED(feat_is_critical, feat)
{
    return feat_stair_direction(feat) != CMD_NO_CMD
           || feat_altar_god(feat) != GOD_NO_GOD
           || feat == DNGN_TRANSPORTER_LANDING
           || feat == DNGN_MALIGN_GATEWAY
           || feat == DNGN_ORB_DAIS;
}

/** Can you use this feature for a map border?
 */
bool feat_is_valid_border(dungeon_feature_type feat)
{
    return feat_is_wall(feat)
           || feat_is_tree(feat)
           || feat == DNGN_OPEN_SEA
           || feat == DNGN_LAVA_SEA
           || feat == DNGN_ENDLESS_SALT;
}

/** Can this feature be a mimic?
 *
 *  @param feat the feature
 *  @param strict if true, disallow features for which being a mimic would be bad in
                  normal generation; vaults can still use such mimics.
 *  @returns whether this could make a valid mimic type.
 */
bool feat_is_mimicable(dungeon_feature_type feat, bool strict)
{
    if (!strict && feat != DNGN_FLOOR && feat != DNGN_SHALLOW_WATER
        && feat != DNGN_DEEP_WATER)
    {
        return true;
    }

    if (feat == DNGN_ENTER_ZIGGURAT)
        return false;

    if (feat == DNGN_ENTER_SHOP)
        return true;

    return false;
}

/** Can creatures on this feature be shafted?
 *
 * @param feat The feature in question.
 * @returns Whether creatures standing on this feature can be shafted (by
 *          magical effects, Formicid digging, etc).
 */
bool feat_is_shaftable(dungeon_feature_type feat)
{
    return feat_has_dry_floor(feat)
           && !feat_is_stair(feat)
           && !feat_is_portal(feat);
}

int count_neighbours_with_func(const coord_def& c, bool (*checker)(dungeon_feature_type))
{
    int count = 0;
    for (adjacent_iterator ai(c); ai; ++ai)
    {
        if (checker(env.grid(*ai)))
            count++;
    }
    return count;
}

// For internal use by find_connected_identical only.
static void _find_connected_identical(const coord_def &d,
                                      dungeon_feature_type ft,
                                      set<coord_def>& out,
                                      bool known_only)
{
    if (env.grid(d) != ft || (known_only && !env.map_knowledge(d).known()))
        return;

    string prop = env.markers.property_at(d, MAT_ANY, "connected_exclude");

    if (!prop.empty())
    {
        // Don't treat this square as connected to anything. Ignore it.
        // Continue the search in other directions.
        return;
    }

    if (out.insert(d).second)
    {
        _find_connected_identical(coord_def(d.x+1, d.y), ft, out, known_only);
        _find_connected_identical(coord_def(d.x-1, d.y), ft, out, known_only);
        _find_connected_identical(coord_def(d.x, d.y+1), ft, out, known_only);
        _find_connected_identical(coord_def(d.x, d.y-1), ft, out, known_only);
    }
}

// Find all connected cells containing ft, starting at d.
void find_connected_identical(const coord_def &d, set<coord_def>& out, bool known_only)
{
    string prop = env.markers.property_at(d, MAT_ANY, "connected_exclude");

    if (!prop.empty())
        out.insert(d);
    else
        _find_connected_identical(d, env.grid(d), out, known_only);
}

void get_door_description(int door_size, const char** adjective, const char** noun)
{
    const char* descriptions[] =
    {
        "miniscule " , "buggy door",
        ""           , "door",
        "large "     , "door",
        ""           , "gate",
        "huge "      , "gate",
    };

    int max_idx = static_cast<int>(ARRAYSZ(descriptions) - 2);
    const unsigned int idx = min(door_size*2, max_idx);

    *adjective = descriptions[idx];
    *noun = descriptions[idx+1];
}

static unique_ptr<map_mask_boolean> _slime_wall_precomputed_neighbour_mask;

static void _precompute_slime_wall_neighbours()
{
    map_mask_boolean &mask(*_slime_wall_precomputed_neighbour_mask);
    for (rectangle_iterator ri(1); ri; ++ri)
    {
        if (env.grid(*ri) == DNGN_SLIMY_WALL)
        {
            for (adjacent_iterator ai(*ri); ai; ++ai)
                mask(*ai) = true;
        }
    }
}

unwind_slime_wall_precomputer::unwind_slime_wall_precomputer(bool docompute)
    : did_compute_mask(false)
{
    if (!(env.level_state & LSTATE_SLIMY_WALL))
        return;

    if (docompute && !_slime_wall_precomputed_neighbour_mask)
    {
        did_compute_mask = true;
        _slime_wall_precomputed_neighbour_mask.reset(
            new map_mask_boolean(false));
        _precompute_slime_wall_neighbours();
    }
}

unwind_slime_wall_precomputer::~unwind_slime_wall_precomputer()
{
    if (did_compute_mask)
        _slime_wall_precomputed_neighbour_mask.reset(nullptr);
}

bool slime_wall_neighbour(const coord_def& c)
{
    if (!(env.level_state & LSTATE_SLIMY_WALL))
        return false;

    if (_slime_wall_precomputed_neighbour_mask)
        return (*_slime_wall_precomputed_neighbour_mask)(c);

    // Not using count_adjacent_slime_walls because the early return might
    // be relevant for performance here. TODO: profile it and find out.
    for (adjacent_iterator ai(c); ai; ++ai)
        if (env.grid(*ai) == DNGN_SLIMY_WALL)
            return true;
    return false;
}

int count_adjacent_slime_walls(const coord_def &pos)
{
    int count = 0;
    for (adjacent_iterator ai(pos); ai; ++ai)
        if (env.grid(*ai) == DNGN_SLIMY_WALL)
            count++;

    return count;
}

int slime_wall_corrosion(actor* act)
{
    ASSERT(act);

    if (actor_slime_wall_immune(act))
        return 0;

    return count_adjacent_slime_walls(act->pos()) * 4;
}

// slime wall damage under Jiyva's oozemancy; this should only affect monsters
void slime_wall_damage(actor* act, int delay)
{
    ASSERT(act);

    if (actor_slime_wall_immune(act))
        return;

    const int walls = count_adjacent_slime_walls(act->pos());
    if (!walls)
        return;

    const int strength = div_rand_round(3 * walls * delay, BASELINE_DELAY);
    const int base_dam = roll_dice(2, strength);
    const int dam = resist_adjust_damage(act, BEAM_ACID, base_dam);
    if (dam > 0 && you.see_cell_no_trans(act->pos()))
    {
        const char *verb = act->is_icy() ? "melt" : "burn";
        mprf((walls > 1) ? "The walls %s %s!" : "The wall %ss %s!",
              verb, act->name(DESC_THE).c_str());
        act->hurt(&you, dam, BEAM_ACID);
        if (act->alive())
            behaviour_event(act->as_monster(), ME_WHACK, &you, you.pos());
    }
}

int count_adjacent_icy_walls(const coord_def &pos)
{
    int count = 0;
    for (adjacent_iterator ai(pos); ai; ++ai)
        if (is_icecovered(*ai))
            count++;

    return count;
}

// Could an actor standing at a given position see a wall next to a given cell?
bool near_visible_wall(coord_def observer_pos, coord_def cell)
{
    if (!cell_see_cell(observer_pos, cell, LOS_NO_TRANS))
        return false;
    for (adjacent_iterator ai(cell); ai; ++ai)
        if (feat_is_wall(env.grid(*ai))
            && cell_see_cell(observer_pos, *ai, LOS_NO_TRANS))
        {
            return true;
        }
    return false;
}

void feat_splash_noise(dungeon_feature_type feat)
{
    if (crawl_state.generating_level)
        return;

    switch (feat)
    {
    case DNGN_SHALLOW_WATER:
    case DNGN_DEEP_WATER:
        mprf(MSGCH_SOUND, "You hear a splash.");
        return;

    case DNGN_LAVA:
        mprf(MSGCH_SOUND, "You hear a sizzling splash.");
        return;

    default:
        return;
    }
}

FEATFN_MEMOIZED(feat_suppress_blood, feat)
{
    if (feat_is_tree(feat))
        return true;

    if (feat == DNGN_DRY_FOUNTAIN || feat == DNGN_RUNELIGHT)
        return true;

    // covers shops and altars
    if (feat_stair_direction(feat) != CMD_NO_CMD)
        return true;

    if (feat == DNGN_MALIGN_GATEWAY)
        return true;

    if (feat == DNGN_TRAP_SHAFT)
        return true;

    return false;

}

/** Does this feature destroy any items that fall into it?
 */
bool feat_destroys_items(dungeon_feature_type feat)
{
    return feat == DNGN_LAVA;
}

/** Does this feature make items that fall into it permanently inaccessible?
 */
bool feat_eliminates_items(dungeon_feature_type feat)
{
    return feat_destroys_items(feat)
            // intentionally use the species version rather than the player
            // version: switching to an amphibious form doesn't give you access
            // to the items.
            || feat == DNGN_DEEP_WATER && !species::likes_water(you.species);
}

static coord_def _dgn_find_nearest_square(
    const coord_def &pos,
    function<bool (const coord_def &)> acceptable,
    function<bool (const coord_def &)> traversable = nullptr)
{
    bool visited[GXM][GYM];
    memset(&visited, 0, sizeof(visited));

    vector<coord_def> points[2];
    int iter = 0;
    points[iter].push_back(pos);

    // TODO: Deduplicate this BFS code (see commit for other two instances).
    while (!points[iter].empty())
    {
        // Iterate each layer of BFS in random order to avoid bias.
        shuffle_array(points[iter]);
        for (const auto &p : points[iter])
        {
            if (p != pos && acceptable(p))
                return p;

            visited[p.x][p.y] = true;
            for (adjacent_iterator ai(p); ai; ++ai)
            {
                const coord_def np = coord_def(ai->x, ai->y);
                if (!in_bounds(np) || visited[np.x][np.y])
                    continue;

                if (traversable && !traversable(np))
                    continue;

                points[!iter].push_back(np);
            }
        }

        points[iter].clear();
        iter = !iter;
    }

    return coord_def(0, 0); // Not found.
}

static bool _item_safe_square(const coord_def &pos)
{
    const dungeon_feature_type feat = env.grid(pos);
    return feat_is_traversable(feat)
            && !feat_is_closed_door(feat)
            && !feat_destroys_items(feat);
}

static bool _item_traversable_square(const coord_def &pos)
{
    return !cell_is_solid(pos);
}

// Moves an item on the floor to the nearest adjacent floor-space.
static bool _dgn_shift_item(const coord_def &pos, item_def &item)
{
    // First try to avoid pushing things through solid features...
    coord_def np = _dgn_find_nearest_square(pos, _item_safe_square,
                                            _item_traversable_square);
    // ... but if we have to, so be it.
    if (!in_bounds(np) || np == pos)
        np = _dgn_find_nearest_square(pos, _item_safe_square);

    if (in_bounds(np) && np != pos)
    {
        int index = item.index();
        move_item_to_grid(&index, np);

        return true;
    }
    return false;
}

static bool _is_feature_shift_target(const coord_def &pos)
{
    return env.grid(pos) == DNGN_FLOOR && !dungeon_events.has_listeners_at(pos)
           && !actor_at(pos);
}

// Moves everything at src to dst. This is not a swap operation: src will be
// left with the same feature it started with, and should be overwritten with
// something new. Assumes there are no actors in the destination square.
//
// Things that are moved:
// 1. Dungeon terrain (set to DNGN_UNSEEN)
// 2. Actors (including the player)
// 3. Items
// 4. Clouds
// 5. Terrain properties
// 6. Terrain colours
// 7. Vault (map) mask
// 8. Vault id mask
// 9. Map markers, dungeon listeners, shopping list
//10. Player's knowledge
void dgn_move_entities_at(coord_def src, coord_def dst,
                          bool move_player,
                          bool move_monster,
                          bool move_items)
{
    if (!in_bounds(dst) || !in_bounds(src) || src == dst)
        return;

    move_notable_thing(src, dst);

    dungeon_feature_type dfeat = env.grid(src);
    if (dfeat == DNGN_ENTER_SHOP)
    {
        ASSERT(shop_at(src));
        env.shop[dst] = env.shop[src];
        env.shop[dst].pos = dst;
        env.shop.erase(src);
        env.grid(src) = DNGN_FLOOR;
    }
    else if (feat_is_trap(dfeat))
    {
        ASSERT(trap_at(src));
        env.trap[dst] = env.trap[src];
        env.trap[dst].pos = dst;
        env.trap.erase(src);
        env.grid(src) = DNGN_FLOOR;
    }

    env.grid(dst) = dfeat;

    if (move_monster || move_player)
        ASSERT(!actor_at(dst));

    if (move_monster)
    {
        if (monster* mon = monster_at(src))
        {
            mon->moveto(dst);
            if (mon->type == MONS_ELDRITCH_TENTACLE)
            {
                if (mon->props.exists(BASE_POSITION_KEY))
                {
                    coord_def delta = dst - src;
                    coord_def base_pos = mon->props[BASE_POSITION_KEY].get_coord();
                    base_pos += delta;
                    mon->props[BASE_POSITION_KEY].get_coord() = base_pos;
                }

            }
            env.mgrid(dst) = env.mgrid(src);
            env.mgrid(src) = NON_MONSTER;
        }
    }

    if (move_player && you.pos() == src)
        you.shiftto(dst);

    if (move_items)
        move_item_stack_to_grid(src, dst);

    if (cell_is_solid(dst))
    {
        delete_cloud(src);
        delete_cloud(dst); // in case there was already a clear there
    }
    else
        move_cloud(src, dst);

    // Move terrain colours and properties.
    env.pgrid(dst) = env.pgrid(src);
    env.grid_colours(dst) = env.grid_colours(src);
#ifdef USE_TILE
    tile_env.bk_fg(dst) = tile_env.bk_fg(src);
    tile_env.bk_bg(dst) = tile_env.bk_bg(src);
    tile_env.bk_cloud(dst) = tile_env.bk_cloud(src);
#endif
    tile_env.flv(dst) = tile_env.flv(src);

    // Move vault masks.
    env.level_map_mask(dst) = env.level_map_mask(src);
    env.level_map_ids(dst) = env.level_map_ids(src);

    // Move markers, dungeon listeners and shopping list.
    env.markers.move(src, dst);
    dungeon_events.move_listeners(src, dst);
    shopping_list.move_things(src, dst);

    // Move player's knowledge.
    env.map_knowledge(dst) = env.map_knowledge(src);
    env.map_seen.set(dst, env.map_seen(src));
    StashTrack.move_stash(src, dst);
}

static bool _dgn_shift_feature(const coord_def &pos)
{
    const dungeon_feature_type dfeat = env.grid(pos);
    if (!feat_is_critical(dfeat) && !env.markers.find(pos, MAT_ANY))
        return false;

    const coord_def dest =
        _dgn_find_nearest_square(pos, _is_feature_shift_target);

    dgn_move_entities_at(pos, dest, false, false, false);
    return true;
}

static void _dgn_check_terrain_items(const coord_def &pos, bool preserve_items)
{
    const dungeon_feature_type feat = env.grid(pos);

    int item = env.igrid(pos);
    while (item != NON_ITEM)
    {
        const int curr = item;
        item = env.item[item].link;

        if (!feat_is_solid(feat) && !feat_destroys_items(feat))
            continue;

        // Game-critical item.
        if (preserve_items || env.item[curr].is_critical())
            _dgn_shift_item(pos, env.item[curr]);
        else
        {
            feat_splash_noise(feat);
            item_was_destroyed(env.item[curr]);
            destroy_item(curr);
        }
    }
}

static void _dgn_check_terrain_monsters(const coord_def &pos)
{
    if (monster* m = monster_at(pos))
        m->apply_location_effects(pos);
}

// Clear blood or off of terrain that shouldn't have it. Also clear of blood if
// a bloody wall has been dug out and replaced by a floor, or if a bloody floor
// has been replaced by a wall.
static void _dgn_check_terrain_covering(const coord_def &pos,
                                     dungeon_feature_type old_feat,
                                     dungeon_feature_type new_feat)
{
    if (!testbits(env.pgrid(pos), FPROP_BLOODY))
        return;

    if (new_feat == DNGN_UNSEEN)
    {
        // Caller has already changed the grid, and old_feat is actually
        // the new feat.
        if (old_feat != DNGN_FLOOR && !feat_is_solid(old_feat))
            env.pgrid(pos) &= ~(FPROP_BLOODY);
    }
    else
    {
        if (feat_is_solid(old_feat) != feat_is_solid(new_feat)
            || feat_is_water(new_feat) || new_feat == DNGN_LAVA
            || feat_is_critical(new_feat))
        {
            env.pgrid(pos) &= ~(FPROP_BLOODY);
        }
    }
}

static void _dgn_check_terrain_player(const coord_def pos)
{
    if (crawl_state.generating_level || !crawl_state.need_save)
        return; // don't reference player if they don't currently exist

    if (pos != you.pos())
        return;

    if (you.can_pass_through(pos))
        move_player_to_grid(pos, false);
    else
        push_or_teleport_actor_from(pos);
}

/**
 * Change a given feature to a new type, cleaning up associated issues
 * (monsters/items in walls, blood on water, etc) in the process.
 *
 * @param pos               The location to be changed.
 * @param nfeat             The feature to be changed to.
 * @param preserve_features Whether to shunt the old feature to a nearby loc.
 * @param preserve_items    Whether to shunt items to a nearby loc, if they
 *                          can't stay in this one.
 * @param temporary         Whether the terrain change is only temporary & so
 *                          shouldn't affect branch/travel knowledge.
 * @param wizmode           Whether this is a wizmode terrain change,
 *                          & shouldn't check whether the player can actually
 *                          exist in the new feature.
 */
void dungeon_terrain_changed(const coord_def &pos,
                             dungeon_feature_type nfeat,
                             bool preserve_features,
                             bool preserve_items,
                             bool temporary,
                             bool wizmode,
                             unsigned short flv_nfeat,
                             unsigned short flv_nfeat_idx)
{
    if (env.grid(pos) == nfeat)
        return;

    // Cannot change the terrain beneath the orb's starting location by any means.
    if (env.grid(pos) == DNGN_ORB_DAIS)
        return;

    // If we're trying to place a wall on top of a monster, push it out of the
    // way first.
    if (feat_is_wall(nfeat) && monster_at(pos))
        push_or_teleport_actor_from(pos);
    if (feat_is_trap(nfeat) && env.trap.find(pos) == env.trap.end())
        place_specific_trap(pos, trap_type_from_feature(nfeat), 1);


    _dgn_check_terrain_covering(pos, env.grid(pos), nfeat);

    if (nfeat != DNGN_UNSEEN)
    {
        if (preserve_features)
            _dgn_shift_feature(pos);

        if (!temporary)
            unnotice_feature(level_pos(level_id::current(), pos));

        env.grid(pos) = nfeat;
        tile_env.flv(pos).feat = flv_nfeat;
        tile_env.flv(pos).feat_idx = flv_nfeat_idx;

        if (is_notable_terrain(nfeat) && you.see_cell(pos))
            seen_notable_thing(nfeat, pos);

        // Don't destroy a trap which was just placed.
        if (!feat_is_trap(nfeat))
            destroy_trap(pos);
    }

    _dgn_check_terrain_items(pos, preserve_items);
    _dgn_check_terrain_monsters(pos);
    if (!wizmode)
        _dgn_check_terrain_player(pos);
    if (!temporary && feature_mimic_at(pos))
        env.level_map_mask(pos) &= ~MMT_MIMIC;

    set_terrain_changed(pos);

    // Deal with doors being created by changing features.
    tile_init_flavour(pos);
}

static void _announce_swap_real(coord_def orig_pos, coord_def dest_pos)
{
    const dungeon_feature_type orig_feat = env.grid(dest_pos);

    const string orig_name =
        feature_description_at(dest_pos, false,
                            you.see_cell(orig_pos) ? DESC_THE : DESC_A);

    string prep = feat_preposition(orig_feat, false);

    string orig_actor, dest_actor;
    if (orig_pos == you.pos())
        orig_actor = "you";
    else if (const monster* m = monster_at(orig_pos))
    {
        if (you.can_see(*m))
            orig_actor = m->name(DESC_THE);
    }

    if (dest_pos == you.pos())
        dest_actor = "you";
    else if (const monster* m = monster_at(dest_pos))
    {
        if (you.can_see(*m))
            dest_actor = m->name(DESC_THE);
    }

    ostringstream str;
    str << orig_name << " ";
    if (you.see_cell(orig_pos) && !you.see_cell(dest_pos))
    {
        str << "suddenly disappears";
        if (!orig_actor.empty())
            str << " from " << prep << " " << orig_actor;
    }
    else if (!you.see_cell(orig_pos) && you.see_cell(dest_pos))
    {
        str << "suddenly appears";
        if (!dest_actor.empty())
            str << " " << prep << " " << dest_actor;
    }
    else
    {
        str << "moves";
        if (!orig_actor.empty())
            str << " from " << prep << " " << orig_actor;
        if (!dest_actor.empty())
            str << " to " << prep << " " << dest_actor;
    }
    str << "!";
    mpr(str.str());
}

static void _announce_swap(coord_def pos1, coord_def pos2)
{
    if (!you.see_cell(pos1) && !you.see_cell(pos2))
        return;

    const dungeon_feature_type feat1 = env.grid(pos1);
    const dungeon_feature_type feat2 = env.grid(pos2);

    if (feat1 == feat2)
        return;

    const bool notable_seen1 = is_notable_terrain(feat1) && you.see_cell(pos1);
    const bool notable_seen2 = is_notable_terrain(feat2) && you.see_cell(pos2);

    if (notable_seen1 && notable_seen2)
    {
        _announce_swap_real(pos1, pos2);
        _announce_swap_real(pos2, pos1);
    }
    else if (notable_seen1)
        _announce_swap_real(pos2, pos1);
    else if (notable_seen2)
        _announce_swap_real(pos1, pos2);
    else if (you.see_cell(pos2))
        _announce_swap_real(pos1, pos2);
    else
        _announce_swap_real(pos2, pos1);
}

bool swap_features(const coord_def &pos1, const coord_def &pos2,
                   bool swap_everything, bool announce)
{
    ASSERT_IN_BOUNDS(pos1);
    ASSERT_IN_BOUNDS(pos2);
    ASSERT(pos1 != pos2);

    if (is_sanctuary(pos1) || is_sanctuary(pos2))
        return false;

    const dungeon_feature_type feat1 = env.grid(pos1);
    const dungeon_feature_type feat2 = env.grid(pos2);

    if (is_notable_terrain(feat1) && !you.see_cell(pos1)
        && env.map_knowledge(pos1).known())
    {
        return false;
    }

    if (is_notable_terrain(feat2) && !you.see_cell(pos2)
        && env.map_knowledge(pos2).known())
    {
        return false;
    }

    const unsigned short col1 = env.grid_colours(pos1);
    const unsigned short col2 = env.grid_colours(pos2);

    const terrain_property_t prop1 = env.pgrid(pos1);
    const terrain_property_t prop2 = env.pgrid(pos2);

    trap_def* trap1 = trap_at(pos1);
    trap_def* trap2 = trap_at(pos2);

    shop_struct* shop1 = shop_at(pos1);
    shop_struct* shop2 = shop_at(pos2);

    // Find a temporary holding place for pos1 stuff to be moved to
    // before pos2 is moved to pos1.
    coord_def temp(-1, -1);
    for (int x = X_BOUND_1 + 1; x < X_BOUND_2; x++)
    {
        for (int y = Y_BOUND_1 + 1; y < Y_BOUND_2; y++)
        {
            coord_def pos(x, y);
            if (pos == pos1 || pos == pos2)
                continue;

            if (!env.markers.find(pos, MAT_ANY)
                && !is_notable_terrain(env.grid(pos))
                && !cloud_at(pos))
            {
                temp = pos;
                break;
            }
        }
        if (in_bounds(temp))
            break;
    }

    if (!in_bounds(temp))
    {
        mprf(MSGCH_ERROR, "swap_features(): No boring squares on level?");
        return false;
    }

    // OK, now we guarantee the move.

    (void) move_notable_thing(pos1, temp);
    env.markers.move(pos1, temp);
    dungeon_events.move_listeners(pos1, temp);
    env.grid(pos1) = DNGN_UNSEEN;
    env.pgrid(pos1) = terrain_property_t{};

    (void) move_notable_thing(pos2, pos1);
    env.markers.move(pos2, pos1);
    dungeon_events.move_listeners(pos2, pos1);
    env.pgrid(pos1) = prop2;
    env.pgrid(pos2) = prop1;

    (void) move_notable_thing(temp, pos2);
    env.markers.move(temp, pos2);
    dungeon_events.move_listeners(temp, pos2);

    // Swap features and colours.
    env.grid(pos2) = feat1;
    env.grid(pos1) = feat2;

    env.grid_colours(pos1) = col2;
    env.grid_colours(pos2) = col1;

    // Swap traps.
    if (trap1 && !trap2)
    {
        env.trap[pos2] = env.trap[pos1];
        env.trap[pos2].pos = pos2;
        env.trap.erase(pos1);
    }
    else if (!trap1 && trap2)
    {
        env.trap[pos1] = env.trap[pos2];
        env.trap[pos1].pos = pos1;
        env.trap.erase(pos2);
    }
    else if (trap1 && trap2)
    {
        trap_def tmp = env.trap[pos1];
        env.trap[pos1] = env.trap[pos2];
        env.trap[pos2] = tmp;
        env.trap[pos1].pos = pos1;
        env.trap[pos2].pos = pos2;
    }

    // Swap shops.
    if (shop1 && !shop2)
    {
        env.shop[pos2] = env.shop[pos1];
        env.shop[pos2].pos = pos2;
        env.shop.erase(pos1);
    }
    else if (!shop1 && shop2)
    {
        env.shop[pos1] = env.shop[pos2];
        env.shop[pos1].pos = pos1;
        env.shop.erase(pos2);
    }
    else if (shop1 && shop2)
    {
        shop_struct tmp = env.shop[pos1];
        env.shop[pos1] = env.shop[pos2];
        env.shop[pos2] = tmp;
        env.shop[pos1].pos = pos1;
        env.shop[pos2].pos = pos2;
    }

    if (!swap_everything)
    {
        _dgn_check_terrain_items(pos1, false);
        _dgn_check_terrain_monsters(pos1);
        _dgn_check_terrain_player(pos1);
        set_terrain_changed(pos1);

        _dgn_check_terrain_items(pos2, false);
        _dgn_check_terrain_monsters(pos2);
        _dgn_check_terrain_player(pos2);
        set_terrain_changed(pos2);

        if (announce)
            _announce_swap(pos1, pos2);
        return true;
    }

    // Swap items.
    for (stack_iterator si(pos1); si; ++si)
        si->pos = pos1;

    for (stack_iterator si(pos2); si; ++si)
        si->pos = pos2;

    // Swap monsters.
    // Note that trapping nets, etc., move together
    // with the monster/player, so don't clear them.
    const int m1 = env.mgrid(pos1);
    const int m2 = env.mgrid(pos2);

    env.mgrid(pos1) = m2;
    env.mgrid(pos2) = m1;

    if (monster_at(pos1))
    {
        env.mons[env.mgrid(pos1)].set_position(pos1);
        env.mons[env.mgrid(pos1)].clear_invalid_constrictions();
    }
    if (monster_at(pos2))
    {
        env.mons[env.mgrid(pos2)].set_position(pos2);
        env.mons[env.mgrid(pos2)].clear_invalid_constrictions();
    }

    swap_clouds(pos1, pos2);

    if (pos1 == you.pos())
    {
        you.set_position(pos2);
        you.clear_invalid_constrictions();
        viewwindow();
        update_screen();
    }
    else if (pos2 == you.pos())
    {
        you.set_position(pos1);
        you.clear_invalid_constrictions();
        viewwindow();
        update_screen();
    }

    set_terrain_changed(pos1);
    set_terrain_changed(pos2);

    if (announce)
        _announce_swap(pos1, pos2);

    return true;
}

static bool _ok_dest_cell(const actor* orig_actor,
                          const dungeon_feature_type orig_feat,
                          const coord_def dest_pos)
{
    const dungeon_feature_type dest_feat = env.grid(dest_pos);

    if (orig_feat == dest_feat)
        return false;

    if (is_notable_terrain(dest_feat))
        return false;

    if (trap_at(dest_pos))
        return false;

    actor* dest_actor = actor_at(dest_pos);

    if (orig_actor && !orig_actor->is_habitable_feat(dest_feat))
        return false;
    if (dest_actor && !dest_actor->is_habitable_feat(orig_feat))
        return false;

    return true;
}

bool slide_feature_over(const coord_def &src, coord_def preferred_dest,
                        bool announce)
{
    ASSERT_IN_BOUNDS(src);

    const dungeon_feature_type orig_feat = env.grid(src);
    const actor* orig_actor = actor_at(src);

    if (in_bounds(preferred_dest)
        && _ok_dest_cell(orig_actor, orig_feat, preferred_dest))
    {
        ASSERT(preferred_dest != src);
    }
    else
    {
        int squares = 0;
        for (adjacent_iterator ai(src); ai; ++ai)
        {
            if (_ok_dest_cell(orig_actor, orig_feat, *ai)
                && one_chance_in(++squares))
            {
                preferred_dest = *ai;
            }
        }
    }

    if (!in_bounds(preferred_dest))
        return false;

    ASSERT(preferred_dest != src);
    return swap_features(src, preferred_dest, false, announce);
}

/**
 * Apply harmful environmental effects from the current tile terrain to the
 * player.
 *
 * @param entry     The terrain type in question.
 */
void fall_into_a_pool(dungeon_feature_type terrain)
{
    if (terrain == DNGN_DEEP_WATER && (you.can_water_walk()
                                       || form_can_swim()))
    {
        return;
    }

    mprf("You fall into the %s!",
         (terrain == DNGN_LAVA)       ? "lava" :
         (terrain == DNGN_DEEP_WATER) ? "water"
                                      : "programming rift");
    // included in default force_more_message
    enable_emergency_flight();
}

typedef map<string, dungeon_feature_type> feat_desc_map;
static feat_desc_map feat_desc_cache;

void init_feat_desc_cache()
{
    for (int i = 0; i < NUM_FEATURES; i++)
    {
        dungeon_feature_type feat = static_cast<dungeon_feature_type>(i);
        string               desc = feature_description(feat);

        lowercase(desc);
        if (!feat_desc_cache.count(desc))
            feat_desc_cache[desc] = feat;
    }
}

dungeon_feature_type feat_by_desc(string desc)
{
    lowercase(desc);

#if TAG_MAJOR_VERSION == 34
    // hard-coded because all the dry fountain variants match this description,
    // and they have a lower enum value, so the first is incorrectly returned
    if (desc == "a dry fountain")
        return DNGN_DRY_FOUNTAIN;
#endif

    return lookup(feat_desc_cache, desc, DNGN_UNSEEN);
}

// If active is true, the player is just stepping onto the feature, with the
// message: "<feature> slides away as you move <prep> it!"
// Else, the actor is already on the feature:
// "<feature> moves from <prep origin> to <prep destination>!"
string feat_preposition(dungeon_feature_type feat, bool active, const actor* who)
{
    const bool         airborne = !who || who->airborne();
    const command_type dir      = feat_stair_direction(feat);

    if (dir == CMD_NO_CMD)
    {
        if (feat == DNGN_STONE_ARCH)
            return "beside";
        else if (feat_is_solid(feat)) // Passwall?
        {
            if (active)
                return "inside";
            else
                return "around";
        }
        else if (!airborne)
        {
            if (feat == DNGN_LAVA || feat_is_water(feat))
            {
                if (active)
                    return "into";
                else
                    return "around";
            }
            else
            {
                if (active)
                    return "onto";
                else
                    return "under";
            }
        }
    }

    if (dir == CMD_GO_UPSTAIRS && feat_is_escape_hatch(feat))
    {
        if (active)
            return "under";
        else
            return "above";
    }

    if (airborne)
    {
        if (active)
            return "over";
        else
            return "beneath";
    }

    if (dir == CMD_GO_DOWNSTAIRS
        && (feat_is_staircase(feat) || feat_is_escape_hatch(feat)))
    {
        if (active)
            return "onto";
        else
            return "beneath";
    }
    else
        return "beside";
}

string stair_climb_verb(dungeon_feature_type feat)
{
    ASSERT(feat_stair_direction(feat) != CMD_NO_CMD);

    if (feat_is_staircase(feat))
        return "climb";
    else if (feat_is_escape_hatch(feat))
        return "use";
    else
        return "pass through";
}

/** Find the feature with this name.
 *
 *  @param name The name (not the user-visible one) to be matched.
 *  @returns DNGN_UNSEEN if name is "", DNGN_FLOOR if the name is for a
 *           dead/forbidden god, and the first entry in the enum with a
 *           matching name otherwise.
 */
dungeon_feature_type dungeon_feature_by_name(const string &name)
{
    if (name.empty())
        return DNGN_UNSEEN;

    for (unsigned i = 0; i < NUM_FEATURES; ++i)
    {
        dungeon_feature_type feat = static_cast<dungeon_feature_type>(i);

        if (!is_valid_feature_type(feat))
            continue;

        if (get_feature_def(feat).vaultname == name)
        {

            if (feat_is_altar(feat)
                && is_unavailable_god(feat_altar_god(feat)))
            {
                return DNGN_FLOOR;
            }

            return feat;
        }
    }

    return DNGN_UNSEEN;
}

/** Find feature names that contain this name.
 *
 *  @param name The string to be matched.
 *  @returns a list of matching names.
 */
vector<string> dungeon_feature_matches(const string &name)
{
    vector<string> matches;

    if (name.empty())
        return matches;

    for (unsigned i = 0; i < NUM_FEATURES; ++i)
    {
        dungeon_feature_type feat = static_cast<dungeon_feature_type>(i);

        if (!is_valid_feature_type(feat))
            continue;

        const char *featname = get_feature_def(feat).vaultname;
        if (strstr(featname, name.c_str()))
            matches.emplace_back(featname);
    }

    return matches;
}

/** Get the lua/wizmode name for a feature.
 *
 *  @param rfeat The feature type to be found.
 *  @returns nullptr if rfeat is not defined, the vaultname of the corresponding
 *           feature_def otherwise.
 */
const char *dungeon_feature_name(dungeon_feature_type rfeat)
{
    if (!is_valid_feature_type(rfeat))
        return nullptr;

    return get_feature_def(rfeat).vaultname;
}

void destroy_wall(const coord_def& p)
{
    if (!in_bounds(p))
        return;

    // Blood does not transfer onto floor.
    if (is_bloodcovered(p))
        env.pgrid(p) &= ~(FPROP_BLOODY);

    _revert_terrain_to(p,
            env.grid(p) == DNGN_MANGROVE ? DNGN_SHALLOW_WATER : DNGN_FLOOR);
    env.level_map_mask(p) |= MMT_TURNED_TO_FLOOR;
}

const char* feat_type_name(dungeon_feature_type feat)
{
    if (feat_is_door(feat))
        return "door";
    if (feat_is_wall(feat))
        return "wall";
    if (feat == DNGN_GRATE)
        return "grate";
    if (feat_is_tree(feat))
        return "tree";
    if (feat_is_statuelike(feat))
        return "statue";
    if (feat_is_water(feat))
        return "water";
    if (feat_is_lava(feat))
        return "lava";
    if (feat_is_altar(feat))
        return "altar";
    if (feat_is_trap(feat))
        return "trap";
    if (feat_is_escape_hatch(feat))
        return "escape hatch";
    if (feat_is_portal(feat) || feat_is_gate(feat))
        return "portal";
    if (feat_is_travelable_stair(feat))
        return "staircase";
    if (feat == DNGN_ENTER_SHOP || feat == DNGN_ABANDONED_SHOP)
        return "shop";
    if (feat_is_fountain(feat))
        return "fountain";
    if (feat == DNGN_ORB_DAIS)
        return "dais";
    if (feat == DNGN_UNSEEN)
        return "unknown terrain";
    return "floor";
}

void set_terrain_changed(const coord_def p)
{
    if (cell_is_solid(p))
        delete_cloud(p);

    if (env.grid(p) == DNGN_SLIMY_WALL)
        env.level_state |= LSTATE_SLIMY_WALL;
    if (env.grid(p) == DNGN_PASSAGE_OF_GOLUBRIA)
        env.level_state |= LSTATE_GOLUBRIA;
    else if (env.grid(p) == DNGN_OPEN_DOOR)
    {
        // Restore colour from door-change markers
        for (map_marker *marker : env.markers.get_markers_at(p))
        {
            if (marker->get_type() == MAT_TERRAIN_CHANGE)
            {
                map_terrain_change_marker* tmarker =
                    dynamic_cast<map_terrain_change_marker*>(marker);

                if (tmarker->change_type == TERRAIN_CHANGE_DOOR_SEAL
                    && tmarker->colour != BLACK)
                {
                    // Restore the unsealed colour.
                    dgn_set_grid_colour_at(p, tmarker->colour);
                    break;
                }
            }
        }
    }

    env.map_knowledge(p).flags |= MAP_CHANGED_FLAG;

    dungeon_events.fire_position_event(DET_FEAT_CHANGE, p);

    los_terrain_changed(p);
}

/**
 * Does this cell count for exploration piety?
 *
 * Don't count: endless map borders, deep water, lava, and cells explicitly
 * marked. (player_view_update_at in view.cc updates the flags)
 */
bool cell_triggers_conduct(const coord_def p)
{
    return !(feat_is_endless(env.grid(p))
             || env.grid(p) == DNGN_LAVA
             || env.grid(p) == DNGN_DEEP_WATER
             || env.pgrid(p) & FPROP_SEEN_OR_NOEXP);
}

bool is_boring_terrain(dungeon_feature_type feat)
{
    if (!is_notable_terrain(feat))
        return true;

    // Altars in the temple are boring, as are any you can never use.
    if (feat_is_altar(feat) && (player_in_branch(BRANCH_TEMPLE)
        || !player_can_join_god(feat_altar_god(feat), false)))
    {
        return true;
    }

    // These were DNGN_ENTER_HELL, but would never be the first you see.
    if (feat_is_hell_subbranch_exit(feat))
        return true;

    // Only note the first entrance to the Abyss/Pan/Hell
    // which is found.
    if ((feat == DNGN_ENTER_ABYSS || feat == DNGN_ENTER_PANDEMONIUM
         || feat == DNGN_ENTER_HELL)
         && overview_knows_num_portals(feat) > 1)
    {
        return true;
    }

    return false;
}

dungeon_feature_type orig_terrain(coord_def pos)
{
    const map_marker *mark = env.markers.find(pos, MAT_TERRAIN_CHANGE);
    if (!mark)
        return env.grid(pos);

    const map_terrain_change_marker *terch
        = dynamic_cast<const map_terrain_change_marker *>(mark);
    ASSERTM(terch, "%s has incorrect class", mark->debug_describe().c_str());

    return terch->old_feature;
}

void temp_change_terrain(coord_def pos, dungeon_feature_type newfeat, int dur,
                         terrain_change_type type, int mid)
{
    dungeon_feature_type old_feat = env.grid(pos);

    // We can't actually change this, so don't add a map marker that will add
    // a confusing 'summoned' to the feature name when examining it.
    if (old_feat == DNGN_ORB_DAIS)
        return;

    tile_flavour old_flv = tile_env.flv(pos);
    for (map_marker *marker : env.markers.get_markers_at(pos))
    {
        if (marker->get_type() == MAT_TERRAIN_CHANGE)
        {
            map_terrain_change_marker* tmarker =
                    dynamic_cast<map_terrain_change_marker*>(marker);

            // If change type matches, just modify old one; no need to add new one
            if (tmarker->change_type == type)
            {
                if (tmarker->new_feature == newfeat)
                {
                    if (tmarker->duration < dur)
                    {
                        tmarker->duration = dur;
                        tmarker->mon_num = mid;
                    }
                }
                else
                {
                    tmarker->new_feature = newfeat;
                    tmarker->duration = dur;
                    tmarker->mon_num = mid;
                }
                // ensure that terrain change happens. Sometimes a terrain
                // change marker can get stuck; this allows re-doing such
                // cases. Also probably needed by the else case above.
                dungeon_terrain_changed(pos, newfeat, false, true, true);
                return;
            }
            else
            {
                old_feat = tmarker->old_feature;
                old_flv.feat = tmarker->flv_old_feature;
                old_flv.feat_idx = tmarker->flv_old_feature_idx;
            }
        }
    }

    // If we are trying to change terrain into what it already is, don't actually
    // add another marker (unless the current terrain is due to some OTHER marker)
    if (env.grid(pos) == newfeat && newfeat == old_feat)
        return;

    map_terrain_change_marker *marker =
        new map_terrain_change_marker(pos, old_feat, newfeat, old_flv.feat,
                                      old_flv.feat_idx, dur, type, mid,
                                      env.grid_colours(pos));
    env.markers.add(marker);
    env.markers.clear_need_activate();
    dungeon_terrain_changed(pos, newfeat, false, true, true);
}

static bool _revert_terrain_to(coord_def pos, dungeon_feature_type feat)
{
    dungeon_feature_type newfeat = feat;
    for (map_marker *marker : env.markers.get_markers_at(pos))
    {
        if (marker->get_type() == MAT_TERRAIN_CHANGE)
        {
            map_terrain_change_marker* tmarker =
                    dynamic_cast<map_terrain_change_marker*>(marker);

            // Don't revert sealed doors to normal doors if we're trying to
            // remove the door altogether
            // Same for destroyed trees and slime walls
            if ((tmarker->change_type == TERRAIN_CHANGE_DOOR_SEAL
                || tmarker->change_type == TERRAIN_CHANGE_FORESTED
                || tmarker->change_type == TERRAIN_CHANGE_SLIME)
                && newfeat == feat)
            {
                env.markers.remove(tmarker);
            }
            else
            {
                newfeat = tmarker->old_feature;
                if (tmarker->new_feature == env.grid(pos))
                    env.markers.remove(tmarker);
            }
        }
    }

    if (env.grid(pos) == DNGN_RUNED_DOOR && newfeat != DNGN_RUNED_DOOR
        || env.grid(pos) == DNGN_RUNED_CLEAR_DOOR
           && newfeat != DNGN_RUNED_CLEAR_DOOR)
    {
        explored_tracked_feature(env.grid(pos));
    }

    env.grid(pos) = newfeat;
    set_terrain_changed(pos);

    tileidx_t old_floortile = tile_env.flv(pos).floor;
    tileidx_t old_floor_idx = tile_env.flv(pos).floor_idx;

    tile_clear_flavour(pos);
    tile_init_flavour(pos);

    // respect vault FTILE directives
    if (newfeat == DNGN_FLOOR)
    {
        if (old_floortile)
            tile_env.flv(pos).floor = old_floortile;
        if (old_floor_idx)
            tile_env.flv(pos).floor_idx = old_floor_idx;
    }

    return true;
}

// If ctype == NUM_TERRAIN_CHANGE_TYPES, will revert *all* terrain changes on
// the given pos.
bool revert_terrain_change(coord_def pos, terrain_change_type ctype)
{
    dungeon_feature_type newfeat = DNGN_UNSEEN;
    unsigned short newfeat_flv = 0;
    unsigned short newfeat_flv_idx = 0;
    int colour = BLACK;

    for (map_marker *marker : env.markers.get_markers_at(pos))
    {
        if (marker->get_type() == MAT_TERRAIN_CHANGE)
        {
            map_terrain_change_marker* tmarker =
                    dynamic_cast<map_terrain_change_marker*>(marker);

            if (tmarker->change_type == ctype || ctype == NUM_TERRAIN_CHANGE_TYPES)
            {
                if (tmarker->colour != BLACK)
                    colour = tmarker->colour;
                if (!newfeat)
                    newfeat = tmarker->old_feature;
                if (!newfeat_flv)
                    newfeat_flv = tmarker->flv_old_feature;
                if (!newfeat_flv_idx)
                    newfeat_flv_idx = tmarker->flv_old_feature_idx;
                env.markers.remove(tmarker);
            }
            else
            {
                // If we had an old colour, give it to the other marker.
                if (colour != BLACK)
                    tmarker->colour = colour;
                colour = BLACK;
                newfeat = tmarker->new_feature;
            }
        }
    }

    // Don't revert opened sealed doors.
    if (feat_is_door(newfeat) && env.grid(pos) == DNGN_OPEN_DOOR)
        newfeat = DNGN_UNSEEN;

    if (newfeat != DNGN_UNSEEN)
    {
        if (ctype == TERRAIN_CHANGE_BOG)
            env.map_knowledge(pos).set_feature(newfeat, colour);
        dungeon_terrain_changed(pos, newfeat, false, true, false, false,
                                newfeat_flv, newfeat_flv_idx);
        env.grid_colours(pos) = colour;
        return true;
    }
    else
        return false;
}

bool is_temp_terrain(coord_def pos)
{
    for (map_marker *marker : env.markers.get_markers_at(pos))
        if (marker->get_type() == MAT_TERRAIN_CHANGE)
            return true;

    return false;
}

bool plant_forbidden_at(const coord_def &p, bool connectivity_only)
{
    // .... Prevent this arrangement by never placing a plant in a way that
    // #P##  locally disconnects two adjacent cells. We scan clockwise around
    // ##.#  p looking for maximal contiguous sequences of traversable cells.
    // #?##  If we find more than one (and they don't join up cyclically),
    //       reject the configuration so the plant doesn't disconnect floor.
    //
    // ...   We do reject many non-problematic cases, such as this one; dpeg
    // #P#   suggests doing a connectivity check in ruination after placing
    // ...   plants, and removing cut-point plants then.

    // First traversable index, last consecutive traversable index, and
    // the next traversable index after last+1.
    int first = -1, last = -1, next = -1;
    int passable = 0;
    for (int i = 0; i < 8; i++)
    {
        coord_def q = p + Compass[i];

        if (feat_is_traversable(env.grid(q), true))
        {
            ++passable;
            if (first < 0)
                first = i;
            else if (last >= 0 && next < 0)
            {
                // Found a maybe-disconnected traversable cell. This is only
                // acceptable if it might connect up at the end.
                if (first == 0)
                    next = i;
                else
                    return true;
            }
        }
        else
        {
            if (first >= 0 && last < 0)
                last = i - 1;
            else if (next >= 0)
                return true;
        }
    }

    // ?#. Forbid this arrangement when the ? squares are walls.
    // #P#  If multiple plants conspire to do something similar, that's
    // ##?  fine: we just want to avoid the most common occurrences.
    //      This would be an info leak (that at least one ? is not a wall)
    //      were it not for the previous check.

    return passable <= 1 && !connectivity_only;
}

/*
 * Find an adjacent space to displace a stack of items or a creature.
 *
 * @param pos the starting position to displace from.
 * @param push_actor true if the goal is to move an actor, false if items
 * @param excluded any spots to rule out a priori. Used for e.g. imprison and
 *                       for multi-space doors.
 *
 * @return a (possibly empty) vector of positions where displacement is
 *                       possible. If `push_actor` is true but there is no
 *                       actor at the position, will return an empty list.
 */
vector<coord_def> get_push_spaces(const coord_def& pos, bool push_actor,
                    const vector<coord_def>* excluded)
{
    vector<coord_def> results;
    actor *act = nullptr;
    if (push_actor)
    {
        act = actor_at(pos);
        if (!act || act->is_stationary() || act->resists_dislodge())
            return results;
    }

    dungeon_feature_type starting_feat = env.grid(pos);
    vector<coord_def> bad_spots; // used for items

    for (adjacent_iterator ai(pos); ai; ++ai)
    {
        dungeon_feature_type feat = env.grid(*ai);

        // Make sure the spot wasn't already vetoed. This is used e.g. for
        // imprison, to pre-exclude all the spots where a wall will be.
        if (excluded && find(begin(*excluded), end(*excluded), *ai)
                            != end(*excluded))
        {
            continue;
        }

        // can never push to a solid space
        if (feat_is_solid(feat))
            continue;

        // Extra checks if we're moving a monster instead of an item
        if (push_actor)
        {
            // these should get deep water and lava for cases where they matter
            if (actor_at(*ai)
                || !act->can_pass_through(*ai)
                || !act->is_habitable(*ai))
            {
                continue;
            }
            results.push_back(*ai);
        }
        else
        {
            if (feat_has_solid_floor(feat))
                results.push_back(*ai);
            else if (starting_feat == DNGN_DEEP_WATER
                && feat == DNGN_DEEP_WATER)
            {
                // Dispreferentially allow pushing items from deep water to
                // deep water. Without this, zin imprison fails over deep
                // water if there are items, even if the player can't see
                // them.
                bad_spots.push_back(*ai);
            }
            // otherwise, can't position an item on this spot
        }
    }
    if (!results.empty())
        return results;
    return bad_spots;
}

bool has_push_spaces(const coord_def& pos, bool push_actor,
                    const vector<coord_def>* excluded)
{
    return !get_push_spaces(pos, push_actor, excluded).empty();
}

/**
 * Push items from `pos`, splashing them around whatever available spaces
 * there are.
 * @param pos the source position.
 * @param excluded positions that are a priori unavailable.
 *
 * @return true if any items moved, false otherwise. (Will return false if there
 *         were no items.)
 */
bool push_items_from(const coord_def& pos, const vector<coord_def>* excluded)
{
    vector<coord_def> targets = get_push_spaces(pos, false, excluded);
    bool result = false;
    if (targets.empty())
        return false;
    // TODO: splashing is flavorful, but how annoying is it in practice?
    while (env.igrid(pos) != NON_ITEM)
        result |= move_top_item(pos, targets[random2(targets.size())]);
    return result;
}

/**
 * Push an actor from `pos` to some available space, if possible.
 *
 * @param pos the source position.
 * @param excluded excluded positions that are a priori unavailable.
 * @param random whether to chose the position randomly, or deterministically.
 *        (Useful for systematically moving a bunch of actors at once, when you
 *        need to worry about domino effects.)
 *
 * @return the new coordinates for the actor.
 */
coord_def push_actor_from(const coord_def& pos,
                          const vector<coord_def>* excluded, bool random)
{
    actor* act = actor_at(pos);
    if (!act)
        return coord_def(0,0);
    vector<coord_def> targets = get_push_spaces(pos, true, excluded);
    if (targets.empty())
        return coord_def(0,0);
    const coord_def newpos = random ? targets[random2(targets.size())]
                                    : targets.front();
    ASSERT(!newpos.origin());
    act->move_to_pos(newpos);
    // The new position of the monster is now an additional veto spot for
    // monsters.
    return newpos;
}

/**
 * Move an actor from 'pos' to some available space, by increasingly forceful
 * means. Will first try to push into an adjacent moveable space. If none is
 * available, will instead move into the closest habitable space without LOS_RADIUS.
 * (Note: there is no guarantee that this isn't on the other side of a wall, but
 * since this is usually used to push things OUT of walls, that is acceptable).
 * Finally, if this fails, simply teleport the actor at random.
 *
 * @return the new coordinates for the actor.
 */
coord_def push_or_teleport_actor_from(const coord_def& pos)
{
    actor* act = actor_at(pos);
    if (!act)
        return coord_def(0,0);

    if (push_actor_from(pos, nullptr, true).origin())
    {
        for (distance_iterator di(pos, false, true, LOS_RADIUS); di; ++di)
        {
            if (!actor_at(*di)
                && ((act->is_player() && you.can_pass_through(*di))
                    || act->is_monster() && monster_habitable_grid(act->as_monster(), *di)))
            {
                if (act->is_player())
                    move_player_to_grid(*di, false);
                else
                    act->move_to_pos(*di);
                return act->pos();
            }
        }

        // Failed to find anywhere in LOS_RADIUS that was valid to put this actor,
        // so just teleport them instead
        act->teleport(true);
    }

    return act->pos();
}

/** Close any door at the given position. Handles the grid change, but does not
 * mark terrain or do any event handling.
 *
 * @param dest The location of the door.
 */
void dgn_close_door(const coord_def &dest)
{
    if (!feat_is_open_door(env.grid(dest)))
        return;

    // Yes, this fixes broken doors.
    const auto feat = env.grid(dest);
    if (feat == DNGN_OPEN_CLEAR_DOOR || feat == DNGN_BROKEN_CLEAR_DOOR)
        env.grid(dest) = DNGN_CLOSED_CLEAR_DOOR;
    else
        env.grid(dest) = DNGN_CLOSED_DOOR;
}

/** Open any door at the given position. Handles the grid change, but does not
 * mark terrain or do any event handling.
 *
 * @param dest The location of the door.
 */
void dgn_open_door(const coord_def &dest)
{
    if (!feat_is_closed_door(env.grid(dest)))
        return;

    if (env.grid(dest) == DNGN_CLOSED_CLEAR_DOOR
        || env.grid(dest) == DNGN_RUNED_CLEAR_DOOR)
    {
        env.grid(dest) = DNGN_OPEN_CLEAR_DOOR;
    }
    else
        env.grid(dest) = DNGN_OPEN_DOOR;
}

/** Breaks any door at the given position. Handles the grid change, but does not
 * mark terrain or do any event handling.
 *
 * @param dest The location of the door.
 */
void dgn_break_door(const coord_def &dest)
{
    if (!feat_is_closed_door(env.grid(dest)))
        return;

    if (env.grid(dest) == DNGN_CLOSED_CLEAR_DOOR
        || env.grid(dest) == DNGN_RUNED_CLEAR_DOOR)
    {
        env.grid(dest) = DNGN_BROKEN_CLEAR_DOOR;
    }
    else
        env.grid(dest) = DNGN_BROKEN_DOOR;
}


void ice_wall_damage(monster &mons, int delay)
{
    if (!you.duration[DUR_FROZEN_RAMPARTS]
        || !you.see_cell_no_trans(mons.pos())
        || mons_aligned(&you, &mons))
    {
        return;
    }

    const int walls = count_adjacent_icy_walls(mons.pos());
    if (!walls)
        return;

    const int pow = you.props[FROZEN_RAMPARTS_POWER_KEY].get_int();
    const int undelayed_dam = ramparts_damage(pow).roll();
    const int post_ac_dam = mons.apply_ac(undelayed_dam);
    const int orig_dam = div_rand_round(delay * post_ac_dam, BASELINE_DELAY);

    bolt beam;
    beam.flavour = BEAM_COLD;
    beam.thrower = KILL_YOU;
    int dam = mons_adjust_flavoured(&mons, beam, orig_dam);
    mprf("The wall freezes %s%s%s",
         you.can_see(mons) ? mons.name(DESC_THE).c_str() : "something",
         dam ? "" : " but does no damage",
         attack_strength_punctuation(dam).c_str());

    if (dam > 0)
    {
        mons.hurt(&you, dam, BEAM_COLD);

        if (mons.alive())
        {
            behaviour_event(&mons, ME_WHACK, &you);
            mons.expose_to_element(BEAM_COLD, orig_dam, &you);
        }
    }
}

static bool _feat_is_descent_upstairs(dungeon_feature_type feat)
{
    if (feat_is_descent_exitable(feat))
        return false;

    return feat_is_stone_stair_up(feat)
        || feat_is_branch_exit(feat)
        || feat == DNGN_EXIT_VAULTS
        || feat == DNGN_EXIT_ZOT
        || feat == DNGN_ESCAPE_HATCH_UP;
}

void descent_crumble_stairs()
{
    if (!crawl_state.game_is_descent() || env.properties.exists(DESCENT_STAIRS_KEY))
        return;

    for (rectangle_iterator ri(0); ri; ++ri)
    {
        dungeon_feature_type feat = env.grid(*ri);
        if (_feat_is_descent_upstairs(feat))
        {
            dungeon_terrain_changed(*ri, DNGN_FLOOR);
            if (you.see_cell(*ri))
                mpr("The exit collapses.");
            if (env.map_knowledge(*ri).feat())
            {
                env.map_knowledge(*ri).set_feature(DNGN_FLOOR);
                set_terrain_mapped(*ri);
            }
            force_show_update_at(*ri);
            view_update_at(*ri);
        }
    }

    env.properties[DESCENT_STAIRS_KEY] = true;
}

static void _descent_reveal_around(coord_def p)
{
    force_show_update_at(p);
    view_update_at(p);

    for (radius_iterator ri(p, you.current_vision, C_SQUARE); ri; ++ri)
    {
        if (cell_see_cell_nocache(p, *ri))
        {
            force_show_update_at(*ri);
            update_item_at(*ri, true);
            set_terrain_visible(*ri);
            view_update_at(*ri);
        }
    }
}

void descent_reveal_stairs()
{
    // possible this should be in another file.
    for (rectangle_iterator ri(0); ri; ++ri)
    {
        dungeon_feature_type feat = env.grid(*ri);
        if (_feat_is_descent_upstairs(feat))
            _descent_reveal_around(*ri);
    }
}
