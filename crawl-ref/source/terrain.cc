/**
 * @file
 * @brief Terrain related functions.
**/

#include "AppHdr.h"

#include "externs.h"
#include "terrain.h"

#include <algorithm>
#include <sstream>

#include "areas.h"
#include "cloud.h"
#include "coordit.h"
#include "dgn-overview.h"
#include "dgnevent.h"
#include "directn.h"
#include "dungeon.h"
#include "map_knowledge.h"
#include "feature.h"
#include "fprop.h"
#include "godabil.h"
#include "itemprop.h"
#include "items.h"
#include "libutil.h"
#include "message.h"
#include "misc.h"
#include "mon-place.h"
#include "coord.h"
#include "mon-util.h"
#include "ouch.h"
#include "player.h"
#include "random.h"
#include "religion.h"
#include "species.h"
#include "spl-transloc.h"
#include "env.h"
#include "state.h"
#include "tileview.h"
#include "travel.h"
#include "transform.h"
#include "traps.h"
#include "view.h"
#include "viewchar.h"
#include "mapmark.h"

static bool _revert_terrain_to(coord_def pos, dungeon_feature_type newfeat);

actor* actor_at(const coord_def& c)
{
    if (!in_bounds(c))
        return NULL;
    if (c == you.pos())
        return &you;
    return monster_at(c);
}

int count_neighbours_with_func(const coord_def& c, bool (*checker)(dungeon_feature_type))
{
    int count = 0;
    for (adjacent_iterator ai(c); ai; ++ai)
    {
        if (checker(grd(*ai)))
            count++;
    }
    return count;
}

bool feat_is_malign_gateway_suitable(dungeon_feature_type feat)
{
    return (feat == DNGN_FLOOR || feat == DNGN_SHALLOW_WATER);
}

bool feat_is_wall(dungeon_feature_type feat)
{
    return (feat >= DNGN_MINWALL && feat <= DNGN_MAXWALL);
}

bool feat_is_stone_stair(dungeon_feature_type feat)
{
    switch (feat)
    {
    case DNGN_STONE_STAIRS_UP_I:
    case DNGN_STONE_STAIRS_UP_II:
    case DNGN_STONE_STAIRS_UP_III:
    case DNGN_STONE_STAIRS_DOWN_I:
    case DNGN_STONE_STAIRS_DOWN_II:
    case DNGN_STONE_STAIRS_DOWN_III:
        return true;
    default:
        return false;
    }
}

bool feat_is_staircase(dungeon_feature_type feat)
{
    if (feat_is_stone_stair(feat))
        return true;

    // All branch entries/exits are staircases, except for Zot.
    if (feat == DNGN_ENTER_ZOT || feat == DNGN_RETURN_FROM_ZOT)
        return false;

    if (feat == DNGN_EXIT_DUNGEON)
        return true;

    return (feat >= DNGN_ENTER_FIRST_BRANCH && feat <= DNGN_ENTER_LAST_BRANCH
            || feat >= DNGN_RETURN_FROM_FIRST_BRANCH
               && feat <= DNGN_RETURN_FROM_LAST_BRANCH);
}

bool feat_is_portal(dungeon_feature_type feat)
{
    return (feat == DNGN_ENTER_PORTAL_VAULT || feat == DNGN_EXIT_PORTAL_VAULT
            || feat == DNGN_MALIGN_GATEWAY);
}

// Returns true if the given dungeon feature is a stair, i.e., a level
// exit.
bool feat_is_stair(dungeon_feature_type gridc)
{
    return (feat_is_travelable_stair(gridc) || feat_is_gate(gridc));
}

// Returns true if the given dungeon feature is a travelable stair, i.e.,
// it's a level exit with a consistent endpoint.
bool feat_is_travelable_stair(dungeon_feature_type feat)
{
    switch (feat)
    {
    case DNGN_STONE_STAIRS_DOWN_I:
    case DNGN_STONE_STAIRS_DOWN_II:
    case DNGN_STONE_STAIRS_DOWN_III:
    case DNGN_ESCAPE_HATCH_DOWN:
    case DNGN_STONE_STAIRS_UP_I:
    case DNGN_STONE_STAIRS_UP_II:
    case DNGN_STONE_STAIRS_UP_III:
    case DNGN_ESCAPE_HATCH_UP:
    case DNGN_EXIT_DUNGEON:
    case DNGN_ENTER_HELL:
    case DNGN_EXIT_HELL:
    case DNGN_ENTER_DIS:
    case DNGN_ENTER_GEHENNA:
    case DNGN_ENTER_COCYTUS:
    case DNGN_ENTER_TARTARUS:
#if TAG_MAJOR_VERSION == 34
    case DNGN_ENTER_DWARVEN_HALL:
#endif
    case DNGN_ENTER_ORCISH_MINES:
    case DNGN_ENTER_LAIR:
    case DNGN_ENTER_SLIME_PITS:
    case DNGN_ENTER_VAULTS:
    case DNGN_ENTER_CRYPT:
    case DNGN_ENTER_HALL_OF_BLADES:
    case DNGN_ENTER_ZOT:
    case DNGN_ENTER_TEMPLE:
    case DNGN_ENTER_SNAKE_PIT:
    case DNGN_ENTER_ELVEN_HALLS:
    case DNGN_ENTER_TOMB:
    case DNGN_ENTER_SWAMP:
    case DNGN_ENTER_SHOALS:
    case DNGN_ENTER_SPIDER_NEST:
    case DNGN_ENTER_FOREST:
#if TAG_MAJOR_VERSION == 34
    case DNGN_RETURN_FROM_DWARVEN_HALL:
#endif
    case DNGN_RETURN_FROM_ORCISH_MINES:
    case DNGN_RETURN_FROM_LAIR:
    case DNGN_RETURN_FROM_SLIME_PITS:
    case DNGN_RETURN_FROM_VAULTS:
    case DNGN_RETURN_FROM_CRYPT:
    case DNGN_RETURN_FROM_HALL_OF_BLADES:
    case DNGN_RETURN_FROM_ZOT:
    case DNGN_RETURN_FROM_TEMPLE:
    case DNGN_RETURN_FROM_SNAKE_PIT:
    case DNGN_RETURN_FROM_ELVEN_HALLS:
    case DNGN_RETURN_FROM_TOMB:
    case DNGN_RETURN_FROM_SWAMP:
    case DNGN_RETURN_FROM_SHOALS:
    case DNGN_RETURN_FROM_SPIDER_NEST:
    case DNGN_RETURN_FROM_FOREST:
        return true;
    default:
        return false;
    }
}

// Returns true if the given dungeon feature is an escape hatch.
bool feat_is_escape_hatch(dungeon_feature_type feat)
{
    switch (feat)
    {
    case DNGN_ESCAPE_HATCH_DOWN:
    case DNGN_ESCAPE_HATCH_UP:
        return true;
    default:
        return false;
    }
}

// Returns true if the given dungeon feature can be considered a gate.
bool feat_is_gate(dungeon_feature_type feat)
{
    switch (feat)
    {
    case DNGN_ENTER_ABYSS:
    case DNGN_EXIT_THROUGH_ABYSS:
    case DNGN_EXIT_ABYSS:
    case DNGN_ABYSSAL_STAIR:
    case DNGN_ENTER_LABYRINTH:
    case DNGN_ENTER_PANDEMONIUM:
    case DNGN_EXIT_PANDEMONIUM:
    case DNGN_TRANSIT_PANDEMONIUM:
    case DNGN_ENTER_PORTAL_VAULT:
    case DNGN_EXIT_PORTAL_VAULT:
    case DNGN_ENTER_ZOT:
    case DNGN_RETURN_FROM_ZOT:
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

command_type feat_stair_direction(dungeon_feature_type feat)
{
    switch (feat)
    {
    case DNGN_STONE_STAIRS_UP_I:
    case DNGN_STONE_STAIRS_UP_II:
    case DNGN_STONE_STAIRS_UP_III:
    case DNGN_ESCAPE_HATCH_UP:
    case DNGN_EXIT_DUNGEON:
#if TAG_MAJOR_VERSION == 34
    case DNGN_RETURN_FROM_DWARVEN_HALL:
#endif
    case DNGN_RETURN_FROM_ORCISH_MINES:
    case DNGN_RETURN_FROM_LAIR:
    case DNGN_RETURN_FROM_SLIME_PITS:
    case DNGN_RETURN_FROM_VAULTS:
    case DNGN_RETURN_FROM_CRYPT:
    case DNGN_RETURN_FROM_HALL_OF_BLADES:
    case DNGN_RETURN_FROM_ZOT:
    case DNGN_RETURN_FROM_TEMPLE:
    case DNGN_RETURN_FROM_SNAKE_PIT:
    case DNGN_RETURN_FROM_ELVEN_HALLS:
    case DNGN_RETURN_FROM_TOMB:
    case DNGN_RETURN_FROM_SWAMP:
    case DNGN_RETURN_FROM_SHOALS:
    case DNGN_RETURN_FROM_SPIDER_NEST:
    case DNGN_RETURN_FROM_FOREST:
    case DNGN_ENTER_SHOP:
    case DNGN_EXIT_HELL:
    case DNGN_EXIT_PORTAL_VAULT:
        return CMD_GO_UPSTAIRS;

    case DNGN_ENTER_PORTAL_VAULT:
    case DNGN_ENTER_HELL:
    case DNGN_ENTER_LABYRINTH:
    case DNGN_STONE_STAIRS_DOWN_I:
    case DNGN_STONE_STAIRS_DOWN_II:
    case DNGN_STONE_STAIRS_DOWN_III:
    case DNGN_ESCAPE_HATCH_DOWN:
    case DNGN_ENTER_DIS:
    case DNGN_ENTER_GEHENNA:
    case DNGN_ENTER_COCYTUS:
    case DNGN_ENTER_TARTARUS:
    case DNGN_ENTER_ABYSS:
    case DNGN_EXIT_THROUGH_ABYSS:
    case DNGN_EXIT_ABYSS:
    case DNGN_ABYSSAL_STAIR:
    case DNGN_ENTER_PANDEMONIUM:
    case DNGN_EXIT_PANDEMONIUM:
    case DNGN_TRANSIT_PANDEMONIUM:
#if TAG_MAJOR_VERSION == 34
    case DNGN_ENTER_DWARVEN_HALL:
#endif
    case DNGN_ENTER_ORCISH_MINES:
    case DNGN_ENTER_LAIR:
    case DNGN_ENTER_SLIME_PITS:
    case DNGN_ENTER_VAULTS:
    case DNGN_ENTER_CRYPT:
    case DNGN_ENTER_HALL_OF_BLADES:
    case DNGN_ENTER_ZOT:
    case DNGN_ENTER_TEMPLE:
    case DNGN_ENTER_SNAKE_PIT:
    case DNGN_ENTER_ELVEN_HALLS:
    case DNGN_ENTER_TOMB:
    case DNGN_ENTER_SWAMP:
    case DNGN_ENTER_SHOALS:
    case DNGN_ENTER_SPIDER_NEST:
    case DNGN_ENTER_FOREST:
        return CMD_GO_DOWNSTAIRS;

    default:
        return CMD_NO_CMD;
    }
}

bool feat_is_opaque(dungeon_feature_type feat)
{
    return (feat <= DNGN_MAXOPAQUE);
}

bool feat_is_solid(dungeon_feature_type feat)
{
    return (feat <= DNGN_MAXSOLID);
}

bool cell_is_solid(const coord_def &c)
{
    return feat_is_solid(grd(c));
}

bool feat_has_solid_floor(dungeon_feature_type feat)
{
    return (!feat_is_solid(feat) && feat != DNGN_DEEP_WATER
            && feat != DNGN_LAVA);
}

bool feat_has_dry_floor(dungeon_feature_type feat)
{
    return (feat_has_solid_floor(feat) && feat != DNGN_SHALLOW_WATER);
}

bool feat_is_door(dungeon_feature_type feat)
{
    return (feat == DNGN_CLOSED_DOOR || feat == DNGN_RUNED_DOOR
            || feat == DNGN_OPEN_DOOR || feat == DNGN_SEALED_DOOR);
}

bool feat_is_closed_door(dungeon_feature_type feat)
{
    return (feat == DNGN_CLOSED_DOOR || feat == DNGN_RUNED_DOOR
            || feat == DNGN_SEALED_DOOR);
}

bool feat_is_statue_or_idol(dungeon_feature_type feat)
{
    return (feat == DNGN_ORCISH_IDOL || feat == DNGN_GRANITE_STATUE);
}

bool feat_is_rock(dungeon_feature_type feat)
{
    return (feat == DNGN_ORCISH_IDOL
            || feat == DNGN_GRANITE_STATUE
            || feat >= DNGN_ROCK_WALL
               && feat <= DNGN_CLEAR_PERMAROCK_WALL);
}

bool feat_is_permarock(dungeon_feature_type feat)
{
    return (feat == DNGN_PERMAROCK_WALL || feat == DNGN_CLEAR_PERMAROCK_WALL);
}

bool feat_is_trap(dungeon_feature_type feat, bool undiscovered_too)
{
    return (feat == DNGN_TRAP_MECHANICAL || feat == DNGN_TRAP_TELEPORT
            || feat == DNGN_TRAP_ALARM   || feat == DNGN_TRAP_ZOT
            || feat == DNGN_PASSAGE_OF_GOLUBRIA // FIXME
            || feat == DNGN_TRAP_SHAFT || feat == DNGN_TRAP_WEB
            || undiscovered_too && feat == DNGN_UNDISCOVERED_TRAP);
}

bool feat_is_water(dungeon_feature_type feat)
{
    return (feat == DNGN_SHALLOW_WATER
            || feat == DNGN_DEEP_WATER
            || feat == DNGN_OPEN_SEA
            || feat == DNGN_MANGROVE);
}

bool feat_is_watery(dungeon_feature_type feat)
{
    return (feat_is_water(feat) || feat == DNGN_FOUNTAIN_BLUE);
}

bool feat_is_lava(dungeon_feature_type feat)
{
    return (feat == DNGN_LAVA
            || feat == DNGN_LAVA_SEA);
}

// Returns GOD_NO_GOD if feat is not an altar, otherwise returns the
// GOD_* type.
god_type feat_altar_god(dungeon_feature_type feat)
{
    if (feat >= DNGN_ALTAR_FIRST_GOD && feat <= DNGN_ALTAR_LAST_GOD)
        return (static_cast<god_type>(feat - DNGN_ALTAR_FIRST_GOD + 1));

    return GOD_NO_GOD;
}

// Returns DNGN_FLOOR for non-gods, otherwise returns the altar for the
// god.
dungeon_feature_type altar_for_god(god_type god)
{
    if (god == GOD_NO_GOD || god >= NUM_GODS)
        return DNGN_FLOOR;  // Yeah, lame. Tell me about it.

    return (static_cast<dungeon_feature_type>(DNGN_ALTAR_FIRST_GOD + god - 1));
}

// Returns true if the dungeon feature supplied is an altar.
bool feat_is_altar(dungeon_feature_type grid)
{
    return (feat_altar_god(grid) != GOD_NO_GOD);
}

bool feat_is_player_altar(dungeon_feature_type grid)
{
    // An ugly hack, but that's what religion.cc does.
    return (!you_worship(GOD_NO_GOD)
            && feat_altar_god(grid) == you.religion);
}

bool feat_is_branch_stairs(dungeon_feature_type feat)
{
    return ((feat >= DNGN_ENTER_FIRST_BRANCH && feat <= DNGN_ENTER_LAST_BRANCH)
            || (feat >= DNGN_ENTER_DIS && feat <= DNGN_ENTER_TARTARUS));
}

bool feat_is_branchlike(dungeon_feature_type feat)
{
    return (feat_is_branch_stairs(feat)
            || feat == DNGN_ENTER_HELL || feat == DNGN_ENTER_ABYSS
            || feat == DNGN_EXIT_THROUGH_ABYSS
            || feat == DNGN_ENTER_PANDEMONIUM);
}

bool feat_is_tree(dungeon_feature_type feat)
{
    return (feat == DNGN_TREE || feat == DNGN_MANGROVE);
}

bool feat_is_metal(dungeon_feature_type feat)
{
    return (feat == DNGN_METAL_WALL || feat == DNGN_GRATE);
}

bool feat_is_bidirectional_portal(dungeon_feature_type feat)
{
    return (get_feature_dchar(feat) == DCHAR_ARCH
            && feat_stair_direction(feat) != CMD_NO_CMD
            && feat != DNGN_ENTER_ZOT
            && feat != DNGN_RETURN_FROM_ZOT
            && feat != DNGN_EXIT_HELL);
}

bool feat_is_fountain(dungeon_feature_type feat)
{
    return feat >= DNGN_FOUNTAIN_BLUE && feat <= DNGN_DRY_FOUNTAIN;
}

bool feat_is_reachable_past(dungeon_feature_type feat)
{
    return feat > DNGN_MAX_NONREACH;
}

// Find all connected cells containing ft, starting at d.
void find_connected_identical(const coord_def &d, dungeon_feature_type ft,
                              set<coord_def>& out)
{
    if (grd(d) != ft)
        return;

    string prop = env.markers.property_at(d, MAT_ANY, "connected_exclude");

    if (!prop.empty())
    {
        // Even if this square is excluded from being a part of connected
        // cells, add it if it's the starting square.
        if (out.empty())
            out.insert(d);
        return;
    }

    if (out.insert(d).second)
    {
        find_connected_identical(coord_def(d.x+1, d.y), ft, out);
        find_connected_identical(coord_def(d.x-1, d.y), ft, out);
        find_connected_identical(coord_def(d.x, d.y+1), ft, out);
        find_connected_identical(coord_def(d.x, d.y-1), ft, out);
    }
}

set<coord_def> connected_doors(const coord_def& d)
{
    set<coord_def> doors;
    find_connected_identical(d, grd(d), doors);
    return doors;
}

void get_door_description(int door_size, const char** adjective, const char** noun)
{
    const char* descriptions[] = {
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

dungeon_feature_type grid_appearance(const coord_def &gc)
{
    dungeon_feature_type feat = env.grid(gc);
    switch (feat)
    {
    case DNGN_UNDISCOVERED_TRAP:
        return DNGN_FLOOR;
    default:
        return feat;
    }
}

coord_def get_random_stair()
{
    vector<coord_def> st;
    for (rectangle_iterator ri(1); ri; ++ri)
    {
        const dungeon_feature_type feat = grd(*ri);
        if (feat_is_travelable_stair(feat) && !feat_is_escape_hatch(feat)
            && (crawl_state.game_is_zotdef() || feat != DNGN_EXIT_DUNGEON)
            && feat != DNGN_EXIT_HELL)
        {
            st.push_back(*ri);
        }
    }
    if (st.empty())
        return coord_def();        // sanity check: shouldn't happen
    return st[random2(st.size())];
}


static unique_ptr<map_mask_boolean> _slime_wall_precomputed_neighbour_mask;

static void _precompute_slime_wall_neighbours()
{
    map_mask_boolean &mask(*_slime_wall_precomputed_neighbour_mask.get());
    for (rectangle_iterator ri(1); ri; ++ri)
    {
        if (grd(*ri) == DNGN_SLIMY_WALL)
        {
            for (adjacent_iterator ai(*ri); ai; ++ai)
                mask(*ai) = true;
        }
    }
}

unwind_slime_wall_precomputer::unwind_slime_wall_precomputer(bool docompute)
    : did_compute_mask(false)
{
    if (docompute && !_slime_wall_precomputed_neighbour_mask.get())
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
        _slime_wall_precomputed_neighbour_mask.reset(NULL);
}

bool slime_wall_neighbour(const coord_def& c)
{
    if (_slime_wall_precomputed_neighbour_mask.get())
        return (*_slime_wall_precomputed_neighbour_mask)(c);

    for (adjacent_iterator ai(c); ai; ++ai)
        if (env.grid(*ai) == DNGN_SLIMY_WALL)
            return true;
    return false;
}

bool feat_destroys_item(dungeon_feature_type feat, const item_def &item,
                        bool noisy)
{
    switch (feat)
    {
    case DNGN_SHALLOW_WATER:
    case DNGN_DEEP_WATER:
        if (noisy)
            mprf(MSGCH_SOUND, "You hear a splash.");
        return false;

    case DNGN_LAVA:
        if (noisy)
            mprf(MSGCH_SOUND, "You hear a sizzling splash.");
        return true;

    default:
        return false;
    }
}

// For checking whether items would be inaccessible when they wouldn't technically be
// destroyed - ignores Merfolk/Fedhas ability to access items in deep water.
bool feat_virtually_destroys_item(dungeon_feature_type feat, const item_def &item,
                                  bool noisy)
{
    switch (feat)
    {
    case DNGN_SHALLOW_WATER:
        if (noisy)
            mprf(MSGCH_SOUND, "You hear a splash.");
        return false;

    case DNGN_DEEP_WATER:
        if (noisy)
            mprf(MSGCH_SOUND, "You hear a splash.");
        return true;

    case DNGN_LAVA:
        if (noisy)
            mprf(MSGCH_SOUND, "You hear a sizzling splash.");
        return true;

    default:
        return false;
    }
}

static coord_def _dgn_find_nearest_square(
    const coord_def &pos,
    void *thing,
    bool (*acceptable)(const coord_def &, void *thing),
    bool (*traversable)(const coord_def &) = NULL)
{
    memset(travel_point_distance, 0, sizeof(travel_distance_grid_t));

    list<coord_def> points[2];
    int iter = 0;
    points[iter].push_back(pos);

    while (!points[iter].empty())
    {
        for (list<coord_def>::iterator i = points[iter].begin();
             i != points[iter].end(); ++i)
        {
            const coord_def &p = *i;

            if (p != pos && acceptable(p, thing))
                return p;

            travel_point_distance[p.x][p.y] = 1;
            for (int yi = -1; yi <= 1; ++yi)
                for (int xi = -1; xi <= 1; ++xi)
                {
                    if (!xi && !yi)
                        continue;

                    const coord_def np = p + coord_def(xi, yi);
                    if (!in_bounds(np) || travel_point_distance[np.x][np.y])
                        continue;

                    if (traversable && !traversable(np))
                        continue;

                    points[!iter].push_back(np);
                }
        }

        points[iter].clear();
        iter = !iter;
    }

    coord_def unfound;
    return unfound;
}

static bool _item_safe_square(const coord_def &pos, void *item)
{
    const dungeon_feature_type feat = grd(pos);
    return (feat_is_traversable(feat) &&
            !feat_destroys_item(feat, *static_cast<item_def *>(item)));
}

// Moves an item on the floor to the nearest adjacent floor-space.
static bool _dgn_shift_item(const coord_def &pos, item_def &item)
{
    const coord_def np = _dgn_find_nearest_square(pos, &item, _item_safe_square);
    if (in_bounds(np) && np != pos)
    {
        int index = item.index();
        move_item_to_grid(&index, np);
        return true;
    }
    return false;
}

bool is_critical_feature(dungeon_feature_type feat)
{
    return (feat_stair_direction(feat) != CMD_NO_CMD
            || feat_altar_god(feat) != GOD_NO_GOD
            || feat == DNGN_MALIGN_GATEWAY);
}

bool is_valid_border_feat(dungeon_feature_type feat)
{
    return (feat <= DNGN_MAXWALL && feat >= DNGN_MINWALL)
            || (feat_is_tree(feat)
                || feat == DNGN_OPEN_SEA
                || feat == DNGN_LAVA_SEA);
}

// This is for randomly generated mimics.
// Other features can be defined as mimic in vaults.
bool is_valid_mimic_feat(dungeon_feature_type feat)
{
    // Don't risk trapping the player inside a portal vault, don't destroy
    // runed doors either.
    if (feat == DNGN_EXIT_PORTAL_VAULT || feat == DNGN_RUNED_DOOR)
        return false;

    if (feat_is_portal(feat) || feat_is_gate(feat))
        return true;

    if (feat_is_stone_stair(feat) || feat_is_escape_hatch(feat)
        || feat_is_branch_stairs(feat))
    {
        return true;
    }

    if (feat_is_fountain(feat))
        return true;

    if (feat_is_door(feat))
        return true;

    if (feat == DNGN_ENTER_SHOP)
        return true;

    if (feat_is_statue_or_idol(feat))
        return true;

    return false;
}

// Those can never be mimiced.
bool feat_cannot_be_mimic(dungeon_feature_type feat)
{
    if (feat == DNGN_FLOOR || feat == DNGN_SHALLOW_WATER
        || feat == DNGN_DEEP_WATER)
    {
        return true;
    }
    return false;
}

static bool _is_feature_shift_target(const coord_def &pos, void*)
{
    return (grd(pos) == DNGN_FLOOR && !dungeon_events.has_listeners_at(pos));
}

// Moves everything at src to dst. This is not a swap operation: src
// will be left with the same feature it started with, and should be
// overwritten with something new.
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

    dungeon_feature_type dfeat = grd(src);
    if (dfeat == DNGN_ENTER_SHOP)
    {
        if (shop_struct *s = get_shop(src))
        {
            env.tgrid(dst)    = env.tgrid(s->pos);
            env.tgrid(s->pos) = NON_ENTITY;
            // Can't leave the source square as a shop now that all
            // the bookkeeping data has moved.
            grd(src)          = DNGN_FLOOR;
            s->pos = dst;
        }
        else // Destroy invalid shops.
            dfeat = DNGN_FLOOR;
    }
    else if (feat_is_trap(dfeat, true))
    {
        if (trap_def *trap = find_trap(src))
        {
            env.tgrid(dst) = env.tgrid(trap->pos);
            env.tgrid(trap->pos) = NON_ENTITY;
            // Can't leave the source square as a trap now that all
            // the bookkeeping data has moved.
            grd(src)          = DNGN_FLOOR;
            trap->pos = dst;
        }
        else // Destroy invalid traps.
            dfeat = DNGN_FLOOR;
    }

    grd(dst) = dfeat;

    if (move_monster)
    {
        if (monster* mon = monster_at(src))
        {
            mon->moveto(dst);
            if (mon->type == MONS_ELDRITCH_TENTACLE)
            {
                if (mon->props.exists("base_position"))
                {
                    coord_def delta = dst - src;
                    coord_def base_pos = mon->props["base_position"].get_coord();
                    base_pos += delta;
                    mon->props["base_position"].get_coord() = base_pos;
                }

            }
            mgrd(dst) = mgrd(src);
            mgrd(src) = NON_MONSTER;
        }
    }

    if (move_player && you.pos() == src)
        you.shiftto(dst);

    if (move_items)
        move_item_stack_to_grid(src, dst);

    move_cloud_to(src, dst);

    // Move terrain colours and properties.
    env.pgrid(dst) = env.pgrid(src);
    env.grid_colours(dst) = env.grid_colours(src);
#ifdef USE_TILE
    env.tile_bk_fg(dst) = env.tile_bk_fg(src);
    env.tile_bk_bg(dst) = env.tile_bk_bg(src);
    env.tile_bk_cloud(dst) = env.tile_bk_cloud(src);
#endif
    env.tile_flv(dst) = env.tile_flv(src);

    // Move vault masks.
    env.level_map_mask(dst) = env.level_map_mask(src);
    env.level_map_ids(dst) = env.level_map_ids(src);

    // Move markers, dungeon listeners and shopping list.
    env.markers.move(src, dst);
    dungeon_events.move_listeners(src, dst);
    shopping_list.move_things(src, dst);

    // Move player's knowledge.
    env.map_knowledge(dst) = env.map_knowledge(src);
    StashTrack.move_stash(src, dst);
}

static bool _dgn_shift_feature(const coord_def &pos)
{
    const dungeon_feature_type dfeat = grd(pos);
    if (!is_critical_feature(dfeat) && !env.markers.find(pos, MAT_ANY))
        return false;

    const coord_def dest =
        _dgn_find_nearest_square(pos, NULL, _is_feature_shift_target);

    dgn_move_entities_at(pos, dest, false, false, false);
    return true;
}

static void _dgn_check_terrain_items(const coord_def &pos, bool preserve_items)
{
    const dungeon_feature_type feat = grd(pos);

    int item = igrd(pos);
    while (item != NON_ITEM)
    {
        const int curr = item;
        item = mitm[item].link;

        if (!feat_is_solid(feat) && !feat_destroys_item(feat, mitm[curr]))
            continue;

        // Game-critical item.
        if (preserve_items || mitm[curr].is_critical())
            _dgn_shift_item(pos, mitm[curr]);
        else
        {
            feat_destroys_item(feat, mitm[curr], true);
            item_was_destroyed(mitm[curr]);
            destroy_item(curr);
        }
    }
}

static void _dgn_check_terrain_monsters(const coord_def &pos)
{
    if (monster* m = monster_at(pos))
        m->apply_location_effects(pos);
}

// Clear blood or mold off of terrain that shouldn't have it.  Also clear
// of blood if a bloody wall has been dug out and replaced by a floor,
// or if a bloody floor has been replaced by a wall.
static void _dgn_check_terrain_covering(const coord_def &pos,
                                     dungeon_feature_type old_feat,
                                     dungeon_feature_type new_feat)
{
    if (!testbits(env.pgrid(pos), FPROP_BLOODY)
        && !is_moldy(pos))
    {
        return;
    }

    if (new_feat == DNGN_UNSEEN)
    {
        // Caller has already changed the grid, and old_feat is actually
        // the new feat.
        if (old_feat != DNGN_FLOOR && !feat_is_solid(old_feat))
        {
            env.pgrid(pos) &= ~(FPROP_BLOODY);
            remove_mold(pos);
        }
    }
    else
    {
        if (feat_is_solid(old_feat) != feat_is_solid(new_feat)
            || feat_is_water(new_feat) || new_feat == DNGN_LAVA
            || is_critical_feature(new_feat))
        {
            env.pgrid(pos) &= ~(FPROP_BLOODY);
            remove_mold(pos);
        }
    }
}

static void _dgn_check_terrain_player(const coord_def pos)
{
    if (pos != you.pos())
        return;

    if (you.can_pass_through(pos))
        move_player_to_grid(pos, false, true);
    else
        you_teleport_now(true, false);
}

void dungeon_terrain_changed(const coord_def &pos,
                             dungeon_feature_type nfeat,
                             bool affect_player,
                             bool preserve_features,
                             bool preserve_items)
{
    if (grd(pos) == nfeat)
        return;

    _dgn_check_terrain_covering(pos, grd(pos), nfeat);

    if (nfeat != DNGN_UNSEEN)
    {
        if (preserve_features)
            _dgn_shift_feature(pos);

        unnotice_feature(level_pos(level_id::current(), pos));

        grd(pos) = nfeat;
        env.grid_colours(pos) = BLACK;
        // Reset feature tile
        env.tile_flv(pos).feat = 0;
        env.tile_flv(pos).feat_idx = 0;

        if (is_notable_terrain(nfeat) && you.see_cell(pos))
            seen_notable_thing(nfeat, pos);

        // Don't destroy a trap which was just placed.
        if (nfeat < DNGN_TRAP_MECHANICAL || nfeat > DNGN_UNDISCOVERED_TRAP)
            destroy_trap(pos);
    }

    _dgn_check_terrain_items(pos, preserve_items);
    _dgn_check_terrain_monsters(pos);

    if (affect_player)
        _dgn_check_terrain_player(pos);

    set_terrain_changed(pos);

    // Deal with doors being created by changing features.
    tile_init_flavour(pos);
}

static void _announce_swap_real(coord_def orig_pos, coord_def dest_pos)
{
    const dungeon_feature_type orig_feat = grd(dest_pos);

    const string orig_name =
        feature_description_at(dest_pos, false,
                            you.see_cell(orig_pos) ? DESC_THE : DESC_A,
                            false);

    string prep = feat_preposition(orig_feat, false);

    string orig_actor, dest_actor;
    if (orig_pos == you.pos())
        orig_actor = "you";
    else if (const monster* m = monster_at(orig_pos))
    {
        if (you.can_see(m))
            orig_actor = m->name(DESC_THE);
    }

    if (dest_pos == you.pos())
        dest_actor = "you";
    else if (const monster* m = monster_at(dest_pos))
    {
        if (you.can_see(m))
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
    mpr(str.str().c_str());
}

static void _announce_swap(coord_def pos1, coord_def pos2)
{
    if (!you.see_cell(pos1) && !you.see_cell(pos2))
        return;

    const dungeon_feature_type feat1 = grd(pos1);
    const dungeon_feature_type feat2 = grd(pos2);

    if (feat1 == feat2)
        return;

    const bool notable_seen1 = is_notable_terrain(feat1) && you.see_cell(pos1);
    const bool notable_seen2 = is_notable_terrain(feat2) && you.see_cell(pos2);
    coord_def orig_pos, dest_pos;

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

    const dungeon_feature_type feat1 = grd(pos1);
    const dungeon_feature_type feat2 = grd(pos2);

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

    trap_def* trap1 = find_trap(pos1);
    trap_def* trap2 = find_trap(pos2);

    shop_struct* shop1 = get_shop(pos1);
    shop_struct* shop2 = get_shop(pos2);

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
                && !is_notable_terrain(grd(pos))
                && env.cgrid(pos) == EMPTY_CLOUD)
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
        mpr("swap_features(): No boring squares on level?", MSGCH_ERROR);
        return false;
    }

    // OK, now we guarantee the move.

    (void) move_notable_thing(pos1, temp);
    env.markers.move(pos1, temp);
    dungeon_events.move_listeners(pos1, temp);
    grd(pos1) = DNGN_UNSEEN;
    env.pgrid(pos1) = 0;

    (void) move_notable_thing(pos2, pos1);
    env.markers.move(pos2, pos1);
    dungeon_events.move_listeners(pos2, pos1);
    env.pgrid(pos1) = prop2;
    env.pgrid(pos2) = prop1;

    (void) move_notable_thing(temp, pos2);
    env.markers.move(temp, pos2);
    dungeon_events.move_listeners(temp, pos2);

    // Swap features and colours.
    grd(pos2) = feat1;
    grd(pos1) = feat2;

    env.grid_colours(pos1) = col2;
    env.grid_colours(pos2) = col1;

    // Swap traps.
    if (trap1)
        trap1->pos = pos2;
    if (trap2)
        trap2->pos = pos1;

    // Swap shops.
    if (shop1)
        shop1->pos = pos2;
    if (shop2)
        shop2->pos = pos1;

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
    const int m1 = mgrd(pos1);
    const int m2 = mgrd(pos2);

    mgrd(pos1) = m2;
    mgrd(pos2) = m1;

    if (monster_at(pos1))
    {
        menv[mgrd(pos1)].set_position(pos1);
        menv[mgrd(pos1)].clear_far_constrictions();
    }
    if (monster_at(pos2))
    {
        menv[mgrd(pos2)].set_position(pos2);
        menv[mgrd(pos2)].clear_far_constrictions();
    }

    // Swap clouds.
    move_cloud(env.cgrid(pos1), temp);
    move_cloud(env.cgrid(pos2), pos1);
    move_cloud(env.cgrid(temp), pos2);

    if (pos1 == you.pos())
    {
        you.set_position(pos2);
        you.clear_far_constrictions();
        viewwindow();
    }
    else if (pos2 == you.pos())
    {
        you.set_position(pos1);
        you.clear_far_constrictions();
        viewwindow();
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
    const dungeon_feature_type dest_feat = grd(dest_pos);

    if (orig_feat == dest_feat)
        return false;

    if (is_notable_terrain(dest_feat))
        return false;

    if (find_trap(dest_pos))
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

    const dungeon_feature_type orig_feat = grd(src);
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

// Returns true if we manage to scramble free.
bool fall_into_a_pool(const coord_def& entry, bool allow_shift,
                      dungeon_feature_type terrain)
{
    bool escape = false;
    bool clinging = false;
    coord_def empty;

    if (terrain == DNGN_DEEP_WATER)
    {
        if (beogh_water_walk() || form_likes_water())
            return false;

        if (species_likes_water(you.species) && !you.transform_uncancellable)
        {
            emergency_untransform();
            return false;
        }
    }

    mprf("You fall into the %s!",
         (terrain == DNGN_LAVA)       ? "lava" :
         (terrain == DNGN_DEEP_WATER) ? "water"
                                      : "programming rift");

    more();
    mesclr();

    if (terrain == DNGN_LAVA)
    {
        const int resist = player_res_fire();

        if (resist <= 0)
        {
            mpr("The lava burns you to a cinder!");
            ouch(INSTANT_DEATH, NON_MONSTER, KILLED_BY_LAVA);

            if (you.dead) // felids
                return false;
        }
        else
        {
            int damage = 10 + roll_dice(2, 50) / resist;

            if (damage > 100)
                mpr("The lava roasts you!!");
            else if (damage > 70)
                mpr("The lava burns you!!");
            else if (damage > 40)
                mpr("The lava sears you!!");
            else if (damage > 20)
                mpr("The lava scorches you!");
            else
                mpr("The lava scalds you!");

            ouch(damage, NON_MONSTER, KILLED_BY_LAVA);

            if (you.dead) // felids
                return false;
        }

        expose_player_to_element(BEAM_LAVA, 14);
    }

    // A distinction between stepping and falling from
    // you.duration[DUR_FLIGHT] prevents stepping into a thin stream
    // of lava to get to the other side.
    if (scramble())
    {
        if (allow_shift)
        {
            escape = find_habitable_spot_near(you.pos(), MONS_HUMAN, 1, false, empty)
                     || you.check_clinging(false);
            clinging = you.is_wall_clinging();
        }
        else
        {
            // Back out the way we came in, if possible.
            if (grid_distance(you.pos(), entry) == 1
                && !monster_at(entry))
            {
                escape = true;
                empty = entry;
            }
            else  // Zero or two or more squares away, with no way back.
                escape = false;
        }
    }
    else
    {
        if (you.form == TRAN_STATUE)
            mpr("You sink like a stone!");
        else
            mpr("You try to escape, but your burden drags you down!");
    }

    if (escape)
    {
        if (in_bounds(empty) && !is_feat_dangerous(grd(empty)) || clinging)
        {
            mpr("You manage to scramble free!");
            if (!clinging)
                move_player_to_grid(empty, false, false);

            if (terrain == DNGN_LAVA)
                expose_player_to_element(BEAM_LAVA, 14);

            return true;
        }
    }

    if (you.species == SP_MUMMY)
    {
        if (terrain == DNGN_LAVA)
            mpr("You burn to ash...");
        else if (terrain == DNGN_DEEP_WATER)
            mpr("You fall apart...");
    }
    else
        mpr("You drown...");

    if (terrain == DNGN_LAVA)
        ouch(INSTANT_DEATH, NON_MONSTER, KILLED_BY_LAVA);
    else if (terrain == DNGN_DEEP_WATER)
        ouch(INSTANT_DEATH, NON_MONSTER, KILLED_BY_WATER);

    return false;
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
        if (feat_desc_cache.find(desc) == feat_desc_cache.end())
            feat_desc_cache[desc] = feat;
    }
}

dungeon_feature_type feat_by_desc(string desc)
{
    lowercase(desc);

    if (desc[desc.size() - 1] != '.')
        desc += ".";

    feat_desc_map::iterator i = feat_desc_cache.find(desc);

    if (i != feat_desc_cache.end())
        return i->second;

    return DNGN_UNSEEN;
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

static const char *dngn_feature_names[] =
{
"unseen", "closed_door", "runed_door", "sealed_door",
"mangrove", "metal_wall", "green_crystal_wall", "rock_wall",
"slimy_wall", "stone_wall", "permarock_wall",
"clear_rock_wall", "clear_stone_wall", "clear_permarock_wall", "iron_grate",
"tree", "open_sea", "endless_lava", "orcish_idol",
"granite_statue", "malign_gateway", "", "", "", "", "", "", "", "", "",

// DNGN_MINMOVE
"lava", "deep_water",

// DNGN_MINWALK
"shallow_water", "floor", "open_door",
"trap_mechanical", "trap_teleport", "shaft", "trap_web",
"undiscovered_trap", "enter_shop", "abandoned_shop",

"stone_stairs_down_i", "stone_stairs_down_ii",
"stone_stairs_down_iii", "escape_hatch_down", "stone_stairs_up_i",
"stone_stairs_up_ii", "stone_stairs_up_iii", "escape_hatch_up",

"enter_dis", "enter_gehenna", "enter_cocytus",
"enter_tartarus", "enter_abyss", "exit_abyss",
#if TAG_MAJOR_VERSION > 34
"abyssal_stair",
#endif
"stone_arch", "enter_pandemonium", "exit_pandemonium",
"transit_pandemonium", "exit_dungeon", "exit_through_abyss",
"exit_hell", "enter_hell", "enter_labyrinth",
"teleporter", "enter_portal_vault", "exit_portal_vault",
"expired_portal",

#if TAG_MAJOR_VERSION == 34
"enter_dwarven_hall",
#endif
"enter_orcish_mines", "enter_lair",
"enter_slime_pits", "enter_vaults", "enter_crypt",
"enter_hall_of_blades", "enter_zot", "enter_temple",
"enter_snake_pit", "enter_elven_halls", "enter_tomb",
"enter_swamp", "enter_shoals", "enter_spider_nest",
"enter_forest", "",

#if TAG_MAJOR_VERSION == 34
"return_from_dwarven_hall",
#endif
"return_from_orcish_mines",
"return_from_lair", "return_from_slime_pits",
"return_from_vaults", "return_from_crypt",
"return_from_hall_of_blades", "return_from_zot",
"return_from_temple", "return_from_snake_pit",
"return_from_elven_halls", "return_from_tomb",
"return_from_swamp", "return_from_shoals", "return_from_spider_nest",
"return_from_forest", "",

"altar_zin", "altar_the_shining_one", "altar_kikubaaqudgha",
"altar_yredelemnul", "altar_xom", "altar_vehumet",
"altar_okawaru", "altar_makhleb", "altar_sif_muna", "altar_trog",
"altar_nemelex_xobeh", "altar_elyvilon", "altar_lugonu",
"altar_beogh", "altar_jiyva", "altar_fedhas", "altar_cheibriados",
"altar_ashenzari", "",

"fountain_blue", "fountain_sparkling", "fountain_blood",
#if TAG_MAJOR_VERSION == 34
"dry_fountain_blue", "dry_fountain_sparkling", "dry_fountain_blood",
#endif
"dry_fountain",

"explore_horizon",
"unknown_altar", "unknown_portal",

#if TAG_MAJOR_VERSION == 34
"abyssal_stair",
"badly_sealed_door",
#endif

"sealed_stair_up",
"sealed_stair_down",

"trap_alarm",
"trap_zot",
"passage_of_golubria",
};

dungeon_feature_type dungeon_feature_by_name(const string &name)
{
    COMPILE_CHECK(ARRAYSZ(dngn_feature_names) == NUM_FEATURES);

    if (name.empty())
        return DNGN_UNSEEN;

    for (unsigned i = 0; i < ARRAYSZ(dngn_feature_names); ++i)
    {
        if (dngn_feature_names[i] == name)
        {
            dungeon_feature_type feat = static_cast<dungeon_feature_type>(i);

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

vector<string> dungeon_feature_matches(const string &name)
{
    vector<string> matches;

    COMPILE_CHECK(ARRAYSZ(dngn_feature_names) == NUM_FEATURES);
    if (name.empty())
        return matches;

    for (unsigned i = 0; i < ARRAYSZ(dngn_feature_names); ++i)
        if (strstr(dngn_feature_names[i], name.c_str()))
            matches.push_back(dngn_feature_names[i]);

    return matches;
}

const char *dungeon_feature_name(dungeon_feature_type rfeat)
{
    const unsigned feat = rfeat;

    if (feat >= ARRAYSZ(dngn_feature_names))
        return NULL;

    return dngn_feature_names[feat];
}

void nuke_wall(const coord_def& p)
{
    if (!in_bounds(p))
        return;

    // Blood does not transfer onto floor.
    if (is_bloodcovered(p))
        env.pgrid(p) &= ~(FPROP_BLOODY);

    remove_mold(p);

    _revert_terrain_to(p, ((grd(p) == DNGN_MANGROVE) ? DNGN_SHALLOW_WATER
                                                     : DNGN_FLOOR));
    env.level_map_mask(p) |= MMT_NUKED;
}

/*
 * Check if an actor can cling to a cell.
 *
 * Wall clinging is done only on orthogonal walls.
 *
 * @param pos The coordinates of the cell.
 *
 * @return Whether the cell is clingable.
 */
bool cell_is_clingable(const coord_def pos)
{
    for (orth_adjacent_iterator ai(pos); ai; ++ai)
        if (feat_is_wall(env.grid(*ai)) || feat_is_closed_door(env.grid(*ai)))
            return true;

    return false;
}

/*
 * Check if an actor can cling from a cell to another.
 *
 * "clinging" to a wall means being orthogonally (left, right, up, down) next
 * to it. A spider can cling to several squares. A move is allowed if the
 * spider clings to an adjacent wall square or the same wall square before and
 * after moving. Being over floor or shallow water and next to a wall counts as
 * clinging to that wall (no further action needed).
 *
 * Example:
 * ~ = deep water
 * * = deep water the spider can reach
 *
 *  #####
 *  ~~#~~
 *  ~~~*~
 *  **s#*
 *  #####
 *
 * Look at Mantis #2704 for more examples.
 *
 * @param from The coordinates of the starting position.
 * @param to The coordinates of the destination.
 *
 * @return Whether it is possible to cling from one cell to another.
 */
bool cell_can_cling_to(const coord_def& from, const coord_def to)
{
    if (!in_bounds(to))
        return false;

    for (orth_adjacent_iterator ai(from); ai; ++ai)
    {
        if (feat_is_wall(env.grid(*ai)))
        {
            for (orth_adjacent_iterator ai2(to, false); ai2; ++ai2)
                if (feat_is_wall(env.grid(*ai2)) && distance2(*ai, *ai2) <= 1)
                    return true;
        }
    }

        return false;
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
    if (feat_is_statue_or_idol(feat))
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
    if (feat == DNGN_UNSEEN)
        return "unknown terrain";
    return "floor";
}

bool is_boring_terrain(dungeon_feature_type feat)
{
    if (!is_notable_terrain(feat))
        return true;

    // A portal deeper into the Ziggurat is boring.
    if (feat == DNGN_ENTER_PORTAL_VAULT && player_in_branch(BRANCH_ZIGGURAT))
        return true;

    // Altars in the temple are boring.
    if (feat_is_altar(feat) && player_in_branch(BRANCH_ECUMENICAL_TEMPLE))
        return true;

    // Only note the first entrance to the Abyss/Pan/Hell
    // which is found.
    if ((feat == DNGN_ENTER_ABYSS || feat == DNGN_ENTER_PANDEMONIUM
         || feat == DNGN_ENTER_HELL)
         && overview_knows_num_portals(feat) > 1)
    {
        return true;
    }

    // There are at least three Zot entrances, and they're always
    // on D:27, so ignore them.
    if (feat == DNGN_ENTER_ZOT)
        return true;

    return false;
}

void temp_change_terrain(coord_def pos, dungeon_feature_type newfeat, int dur,
                         terrain_change_type type, const monster* mon)
{
    dungeon_feature_type old_feat = grd(pos);
    vector<map_marker*> markers = env.markers.get_markers_at(pos);
    for (int i = 0, size = markers.size(); i < size; ++i)
    {
        if (markers[i]->get_type() == MAT_TERRAIN_CHANGE)
        {
            map_terrain_change_marker* marker =
                    dynamic_cast<map_terrain_change_marker*>(markers[i]);

            // If change type matches, just modify old one; no need to add new one
            if (marker->change_type == type)
            {
                if (marker->new_feature == newfeat)
                {
                    if (marker->duration < dur)
                    {
                        marker->duration = dur;
                        if (mon)
                            marker->mon_num = mon->mid;
                    }
                }
                else
                {
                    marker->new_feature = newfeat;
                    marker->duration = dur;
                    if (mon)
                        marker->mon_num = mon->mid;
                }
                return;
            }
            else
                old_feat = marker->old_feature;
        }
    }

    map_terrain_change_marker *marker =
        new map_terrain_change_marker(pos, old_feat, newfeat, dur, type);
    if (mon)
        marker->mon_num = mon->mid;
    env.markers.add(marker);
    env.markers.clear_need_activate();
    dungeon_terrain_changed(pos, newfeat, true, false, true);
}

static bool _revert_terrain_to(coord_def pos, dungeon_feature_type newfeat)
{
    vector<map_marker*> markers = env.markers.get_markers_at(pos);

    bool found_marker = false;
    for (int i = 0, size = markers.size(); i < size; ++i)
    {
        if (markers[i]->get_type() == MAT_TERRAIN_CHANGE)
        {
            found_marker = true;
            map_terrain_change_marker* marker =
                    dynamic_cast<map_terrain_change_marker*>(markers[i]);

            // Don't revert sealed doors to normal doors if we're trying to
            // remove the door altogether
            if (marker->change_type == TERRAIN_CHANGE_DOOR_SEAL
                && newfeat == DNGN_FLOOR)
            {
                env.markers.remove(marker);
            }
            else
            {
                newfeat = marker->old_feature;
                if (marker->new_feature == grd(pos))
                    env.markers.remove(marker);
            }
        }
    }

    grd(pos) = newfeat;
    set_terrain_changed(pos);

    if (found_marker)
    {
        tile_clear_flavour(pos);
        tile_init_flavour(pos);
    }

    return true;
}

bool revert_terrain_change(coord_def pos, terrain_change_type ctype)
{
    vector<map_marker*> markers = env.markers.get_markers_at(pos);
    dungeon_feature_type newfeat = DNGN_UNSEEN;

    for (int i = 0, size = markers.size(); i < size; ++i)
    {
        if (markers[i]->get_type() == MAT_TERRAIN_CHANGE)
        {
            map_terrain_change_marker* marker =
                    dynamic_cast<map_terrain_change_marker*>(markers[i]);

            if (marker->change_type == ctype)
            {
                if (!newfeat)
                    newfeat = marker->old_feature;
                env.markers.remove(marker);
            }
            else
                newfeat = marker->new_feature;
        }
    }

    // Don't revert opened sealed doors.
    if (feat_is_door(newfeat) && grd(pos) == DNGN_OPEN_DOOR)
        newfeat = DNGN_UNSEEN;

    if (newfeat != DNGN_UNSEEN)
    {
        dungeon_terrain_changed(pos, newfeat, true, false, true);
        return true;
    }
    else
        return false;
}

bool plant_forbidden_at(const coord_def &p, bool connectivity_only)
{
    // ....  Prevent this arrangement by never placing a plant in a way that
    // #P##  locally disconnects two adjacent cells.  We scan clockwise around
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

        if (feat_is_traversable(grd(q), true))
        {
            ++passable;
            if (first < 0)
                first = i;
            else if (last >= 0 && next < 0)
            {
                // Found a maybe-disconnected traversable cell.  This is only
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

    // ?#.  Forbid this arrangement when the ? squares are walls.
    // #P#  If multiple plants conspire to do something similar, that's
    // ##?  fine: we just want to avoid the most common occurrences.
    //      This would be an info leak (that at least one ? is not a wall)
    //      were it not for the previous check.

    return (passable <= 1 && !connectivity_only);
}
