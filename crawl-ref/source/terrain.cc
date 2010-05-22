/*
 *  File:       terrain.cc
 *  Summary:    Terrain related functions.
 *  Written by: Linley Henzell
 */

#include "AppHdr.h"

#include "externs.h"
#include "terrain.h"

#include <algorithm>
#include <sstream>

#include "cloud.h"
#include "coordit.h"
#include "dgn-overview.h"
#include "dgnevent.h"
#include "directn.h"
#include "map_knowledge.h"
#include "fprop.h"
#include "godabil.h"
#include "itemprop.h"
#include "items.h"
#include "libutil.h"
#include "message.h"
#include "misc.h"
#include "mon-place.h"
#include "coord.h"
#include "mon-stuff.h"
#include "mon-util.h"
#include "ouch.h"
#include "player.h"
#include "random.h"
#include "religion.h"
#include "spells3.h"
#include "stuff.h"
#include "env.h"
#include "travel.h"
#include "transform.h"
#include "traps.h"
#include "view.h"

actor* actor_at(const coord_def& c)
{
    if (!in_bounds(c))
        return (NULL);
    if (c == you.pos())
        return (&you);
    return (monster_at(c));
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
        return (true);
    default:
        return (false);
    }
}

bool feat_is_staircase(dungeon_feature_type feat)
{
    if (feat_is_stone_stair(feat))
    {
        // Make up staircases in hell appear as gates.
        if (player_in_hell())
        {
            switch (feat)
            {
                case DNGN_STONE_STAIRS_UP_I:
                case DNGN_STONE_STAIRS_UP_II:
                case DNGN_STONE_STAIRS_UP_III:
                    return (false);
                default:
                    return (true);
            }
        }
        return (true);
    }

    // All branch entries/exits are staircases, except for Zot.
    if (feat == DNGN_ENTER_ZOT || feat == DNGN_RETURN_FROM_ZOT)
        return (false);

    return (feat >= DNGN_ENTER_FIRST_BRANCH && feat <= DNGN_ENTER_LAST_BRANCH
            || feat >= DNGN_RETURN_FROM_FIRST_BRANCH
               && feat <= DNGN_RETURN_FROM_LAST_BRANCH);
}

bool feat_sealable_portal(dungeon_feature_type feat)
{
    switch (feat)
    {
    case DNGN_ENTER_COCYTUS:
    case DNGN_ENTER_DIS:
    case DNGN_ENTER_GEHENNA:
    case DNGN_ENTER_TARTARUS:
    case DNGN_ENTER_ABYSS:
    case DNGN_ENTER_PANDEMONIUM:
    case DNGN_ENTER_LABYRINTH:
    case DNGN_ENTER_PORTAL_VAULT:
        return (true);
    default:
        return (false);
    }
}

bool feat_is_portal(dungeon_feature_type feat)
{
    return (feat == DNGN_ENTER_PORTAL_VAULT || feat == DNGN_EXIT_PORTAL_VAULT);
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
    case DNGN_ENTER_HELL:
    case DNGN_EXIT_HELL:
    case DNGN_ENTER_DIS:
    case DNGN_ENTER_GEHENNA:
    case DNGN_ENTER_COCYTUS:
    case DNGN_ENTER_TARTARUS:
    case DNGN_ENTER_ORCISH_MINES:
    case DNGN_ENTER_HIVE:
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
    case DNGN_RETURN_FROM_ORCISH_MINES:
    case DNGN_RETURN_FROM_HIVE:
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
        return (true);
    default:
        return (false);
    }
}

// Returns true if the given dungeon feature is an escape hatch.
bool feat_is_escape_hatch(dungeon_feature_type feat)
{
    switch (feat)
    {
    case DNGN_ESCAPE_HATCH_DOWN:
    case DNGN_ESCAPE_HATCH_UP:
        return (true);
    default:
        return (false);
    }
}

// Returns true if the given dungeon feature can be considered a gate.
bool feat_is_gate(dungeon_feature_type feat)
{
    // Make up staircases in hell appear as gates.
    if (player_in_hell())
    {
        switch (feat)
        {
        case DNGN_STONE_STAIRS_UP_I:
        case DNGN_STONE_STAIRS_UP_II:
        case DNGN_STONE_STAIRS_UP_III:
            return (true);
        default:
            break;
        }
    }

    switch (feat)
    {
    case DNGN_ENTER_ABYSS:
    case DNGN_EXIT_ABYSS:
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
        return (true);
    default:
        return (false);
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
    case DNGN_RETURN_FROM_ORCISH_MINES:
    case DNGN_RETURN_FROM_HIVE:
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
    case DNGN_ENTER_SHOP:
    case DNGN_EXIT_HELL:
    case DNGN_EXIT_PORTAL_VAULT:
        return (CMD_GO_UPSTAIRS);

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
    case DNGN_EXIT_ABYSS:
    case DNGN_ENTER_PANDEMONIUM:
    case DNGN_EXIT_PANDEMONIUM:
    case DNGN_TRANSIT_PANDEMONIUM:
    case DNGN_ENTER_ORCISH_MINES:
    case DNGN_ENTER_HIVE:
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
        return (CMD_GO_DOWNSTAIRS);

    default:
        return (CMD_NO_CMD);
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

bool cell_is_solid(int x, int y)
{
    return (feat_is_solid(grd[x][y]));
}

bool cell_is_solid(const coord_def &c)
{
    return (feat_is_solid(grd(c)));
}

bool feat_is_floor(dungeon_feature_type feat)
{
    return (feat >= DNGN_FLOOR_MIN && feat <= DNGN_FLOOR_MAX);
}

bool feat_has_solid_floor(dungeon_feature_type feat)
{
    return (!feat_is_solid(feat) && feat != DNGN_DEEP_WATER &&
            feat != DNGN_LAVA);
}

bool feat_is_door(dungeon_feature_type feat)
{
    return (feat == DNGN_CLOSED_DOOR || feat == DNGN_DETECTED_SECRET_DOOR
            || feat == DNGN_OPEN_DOOR);
}

bool feat_is_closed_door(dungeon_feature_type feat)
{
    return (feat == DNGN_CLOSED_DOOR || feat == DNGN_DETECTED_SECRET_DOOR);
}

bool feat_is_secret_door(dungeon_feature_type feat)
{
    return (feat == DNGN_SECRET_DOOR || feat == DNGN_DETECTED_SECRET_DOOR);
}

bool feat_is_statue_or_idol(dungeon_feature_type feat)
{
    return (feat >= DNGN_ORCISH_IDOL && feat <= DNGN_STATUE_RESERVED);
}

bool feat_is_rock(dungeon_feature_type feat)
{
    return (feat == DNGN_ORCISH_IDOL
            || feat == DNGN_GRANITE_STATUE
            || feat == DNGN_SECRET_DOOR
            || feat >= DNGN_ROCK_WALL
               && feat <= DNGN_CLEAR_PERMAROCK_WALL);
}

bool feat_is_permarock(dungeon_feature_type feat)
{
    return (feat == DNGN_PERMAROCK_WALL || feat == DNGN_CLEAR_PERMAROCK_WALL);
}

bool feat_is_trap(dungeon_feature_type feat, bool undiscovered_too)
{
    return (feat == DNGN_TRAP_MECHANICAL || feat == DNGN_TRAP_MAGICAL
            || feat == DNGN_TRAP_NATURAL
            || undiscovered_too && feat == DNGN_UNDISCOVERED_TRAP);
}

bool feat_is_water(dungeon_feature_type feat)
{
    return (feat == DNGN_SHALLOW_WATER
            || feat == DNGN_DEEP_WATER
            || feat == DNGN_OPEN_SEA
            || feat == DNGN_WATER_RESERVED);
}

bool feat_is_watery(dungeon_feature_type feat)
{
    return (feat_is_water(feat) || feat == DNGN_FOUNTAIN_BLUE);
}

// Returns GOD_NO_GOD if feat is not an altar, otherwise returns the
// GOD_* type.
god_type feat_altar_god(dungeon_feature_type feat)
{
    if (feat >= DNGN_ALTAR_FIRST_GOD && feat <= DNGN_ALTAR_LAST_GOD)
        return (static_cast<god_type>(feat - DNGN_ALTAR_FIRST_GOD + 1));

    return (GOD_NO_GOD);
}

// Returns DNGN_FLOOR for non-gods, otherwise returns the altar for the
// god.
dungeon_feature_type altar_for_god(god_type god)
{
    if (god == GOD_NO_GOD || god >= NUM_GODS)
        return (DNGN_FLOOR);  // Yeah, lame. Tell me about it.

    return static_cast<dungeon_feature_type>(DNGN_ALTAR_FIRST_GOD + god - 1);
}

// Returns true if the dungeon feature supplied is an altar.
bool feat_is_altar(dungeon_feature_type grid)
{
    return feat_altar_god(grid) != GOD_NO_GOD;
}

bool feat_is_player_altar(dungeon_feature_type grid)
{
    // An ugly hack, but that's what religion.cc does.
    return (you.religion != GOD_NO_GOD
            && feat_altar_god(grid) == you.religion);
}

bool feat_is_branch_stairs(dungeon_feature_type feat)
{
    return ((feat >= DNGN_ENTER_FIRST_BRANCH && feat <= DNGN_ENTER_LAST_BRANCH)
            || (feat >= DNGN_ENTER_DIS && feat <= DNGN_ENTER_TARTARUS));
}

// Find all connected cells containing ft, starting at d.
void find_connected_identical(const coord_def &d, dungeon_feature_type ft,
                              std::set<coord_def>& out)
{
    if (grd(d) != ft)
        return;

    std::string prop = env.markers.property_at(d, MAT_ANY, "connected_exclude");

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

// Find all connected cells containing ft_min to ft_max, starting at d.
void find_connected_range(const coord_def& d, dungeon_feature_type ft_min,
                          dungeon_feature_type ft_max,
                          std::set<coord_def>& out)
{
    if (grd(d) < ft_min || grd(d) > ft_max)
        return;

    std::string prop = env.markers.property_at(d, MAT_ANY, "connected_exclude");

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
        find_connected_range(coord_def(d.x+1, d.y), ft_min, ft_max, out);
        find_connected_range(coord_def(d.x-1, d.y), ft_min, ft_max, out);
        find_connected_range(coord_def(d.x, d.y+1), ft_min, ft_max, out);
        find_connected_range(coord_def(d.x, d.y-1), ft_min, ft_max, out);
    }
}

std::set<coord_def> connected_doors(const coord_def& d)
{
    std::set<coord_def> doors;
    find_connected_range(d, DNGN_CLOSED_DOOR, DNGN_SECRET_DOOR, doors);
    return (doors);
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
    const unsigned int idx = std::min(door_size*2, max_idx);

    *adjective = descriptions[idx];
    *noun = descriptions[idx+1];
}

dungeon_feature_type grid_appearance(const coord_def &gc)
{
    dungeon_feature_type feat = env.grid(gc);
    switch (feat)
    {
    case DNGN_SECRET_DOOR:
        return grid_secret_door_appearance(gc);
    case DNGN_UNDISCOVERED_TRAP:
        return DNGN_FLOOR;
    default:
        return feat;
    }
}

bool find_secret_door_info(const coord_def &where,
                           dungeon_feature_type *appearance,
                           coord_def *gc)
{
    std::set<coord_def> doors = connected_doors(where);
    std::set<coord_def>::iterator it;

    dungeon_feature_type feat = DNGN_FLOOR;
    coord_def loc = where;

    int orth[][2] = { {0, 1}, {1, 0,}, {-1, 0}, {0, -1} };

    for (it = doors.begin(); it != doors.end(); ++it)
    {
        for (int i = 0; i < 4; i++)
        {
            const int x = it->x + orth[i][0];
            const int y = it->y + orth[i][1];

            if (!in_bounds(x, y))
                continue;

            const dungeon_feature_type targ = grd[x][y];
            if (!feat_is_wall(targ) || feat_is_closed_door(targ))
                continue;

            if (feat == DNGN_FLOOR || targ < feat)
            {
                feat = targ;
                loc = coord_def(x, y);
            }
        }
    }

    if (feat == DNGN_FLOOR)
    {
        if (appearance)
            *appearance = DNGN_ROCK_WALL;
        return (false);
    }
    else
    {
        if (appearance)
            *appearance = feat;
        if (gc)
            *gc = loc;
        return (true);
    }
}

dungeon_feature_type grid_secret_door_appearance(const coord_def &where)
{
    dungeon_feature_type feat;
    find_secret_door_info(where, &feat, NULL);
    return (feat);
}

bool feat_destroys_item(dungeon_feature_type feat, const item_def &item,
                        bool noisy)
{
    switch (feat)
    {
    case DNGN_SHALLOW_WATER:
    case DNGN_DEEP_WATER:
        if (is_rune(item) && item.plus == RUNE_ABYSSAL)
            return (true);

        if (noisy)
            mprf(MSGCH_SOUND, "You hear a splash.");
        return (false);

    case DNGN_LAVA:
        if (noisy)
            mprf(MSGCH_SOUND, "You hear a sizzling splash.");
        return (item.base_type == OBJ_SCROLLS);

    default:
        return (false);
    }
}

static coord_def _dgn_find_nearest_square(
    const coord_def &pos,
    void *thing,
    bool (*acceptable)(const coord_def &, void *thing),
    bool (*traversable)(const coord_def &) = NULL)
{
    memset(travel_point_distance, 0, sizeof(travel_distance_grid_t));

    std::list<coord_def> points[2];
    int iter = 0;
    points[iter].push_back(pos);

    while (!points[iter].empty())
    {
        for (std::list<coord_def>::iterator i = points[iter].begin();
             i != points[iter].end(); ++i)
        {
            const coord_def &p = *i;

            if (p != pos && acceptable(p, thing))
                return (p);

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
    return (unfound);
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
        return (true);
    }
    return (false);
}

bool is_critical_feature(dungeon_feature_type feat)
{
    return (feat_stair_direction(feat) != CMD_NO_CMD
            || feat_altar_god(feat) != GOD_NO_GOD);
}

static bool _is_feature_shift_target(const coord_def &pos, void*)
{
    return (grd(pos) == DNGN_FLOOR && !dungeon_events.has_listeners_at(pos));
}

static bool _dgn_shift_feature(const coord_def &pos)
{
    const dungeon_feature_type dfeat = grd(pos);
    if (!is_critical_feature(dfeat) && !env.markers.find(pos, MAT_ANY))
        return (false);

    const coord_def dest =
        _dgn_find_nearest_square(pos, NULL, _is_feature_shift_target);

    if (in_bounds(dest) && dest != pos)
    {
        grd(dest) = dfeat;

        if (dfeat == DNGN_ENTER_SHOP)
            if (shop_struct *s = get_shop(pos))
                s->pos = dest;

        env.markers.move(pos, dest);
        dungeon_events.move_listeners(pos, dest);
        shopping_list.move_things(pos, dest);
    }
    return (true);
}

static void _dgn_check_terrain_items(const coord_def &pos, bool preserve_items)
{
    const dungeon_feature_type feat = grd(pos);

    int item = igrd(pos);
    bool did_destroy = false;
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
            did_destroy = true;
        }
    }
}

static void _dgn_check_terrain_monsters(const coord_def &pos)
{
    if (monsters* m = monster_at(pos))
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

void _dgn_check_terrain_player(const coord_def pos)
{
    if (pos != you.pos())
        return;

    if (you.can_pass_through(pos))
    {
        // If the monster can't stay submerged in the new terrain and
        // there aren't any adjacent squares where it can stay
        // submerged then move it.
        monsters* mon = monster_at(pos);
        if (mon && !mon->submerged())
            monster_teleport(mon, true, false);
        move_player_to_grid(pos, false, true, true);
    }
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
#ifdef USE_TILE
    tile_init_flavour(pos);
#endif
}

static void _announce_swap_real(coord_def orig_pos, coord_def dest_pos)
{
    const dungeon_feature_type orig_feat = grd(dest_pos);

    const std::string orig_name =
        feature_description(dest_pos, false,
                            you.see_cell(orig_pos) ? DESC_CAP_THE : DESC_CAP_A,
                            false);

    std::string prep = feat_preposition(orig_feat, false);

    std::string orig_actor, dest_actor;
    if (orig_pos == you.pos())
        orig_actor = "you";
    else if (const monsters *m = monster_at(orig_pos))
    {
        if (you.can_see(m))
            orig_actor = m->name(DESC_NOCAP_THE);
    }

    if (dest_pos == you.pos())
        dest_actor = "you";
    else if (const monsters *m = monster_at(dest_pos))
    {
        if (you.can_see(m))
            dest_actor = m->name(DESC_NOCAP_THE);
    }

    std::ostringstream str;
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
    ASSERT(in_bounds(pos1) && in_bounds(pos2));
    ASSERT(pos1 != pos2);

    if (is_sanctuary(pos1) || is_sanctuary(pos2))
        return (false);

    const dungeon_feature_type feat1 = grd(pos1);
    const dungeon_feature_type feat2 = grd(pos2);

    if (is_notable_terrain(feat1) && !you.see_cell(pos1)
        && is_terrain_known(pos1))
    {
        return (false);
    }

    if (is_notable_terrain(feat2) && !you.see_cell(pos2)
        && is_terrain_known(pos2))
    {
        return (false);
    }

    const unsigned short col1 = env.grid_colours(pos1);
    const unsigned short col2 = env.grid_colours(pos2);

    const unsigned long prop1 = env.pgrid(pos1);
    const unsigned long prop2 = env.pgrid(pos2);

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
        return (false);
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
        return (true);
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
        menv[mgrd(pos1)].set_position(pos1);
    if (monster_at(pos2))
        menv[mgrd(pos2)].set_position(pos2);

    // Swap clouds.
    move_cloud(env.cgrid(pos1), temp);
    move_cloud(env.cgrid(pos2), pos1);
    move_cloud(env.cgrid(temp), pos2);

    if (pos1 == you.pos())
    {
        you.set_position(pos2);
        viewwindow(false);
    }
    else if (pos2 == you.pos())
    {
        you.set_position(pos1);
        viewwindow(false);
    }

    set_terrain_changed(pos1);
    set_terrain_changed(pos2);

    if (announce)
        _announce_swap(pos1, pos2);

    return (true);
}

static bool _ok_dest_cell(const actor* orig_actor,
                          const dungeon_feature_type orig_feat,
                          const coord_def dest_pos)
{
    const dungeon_feature_type dest_feat = grd(dest_pos);

    if (orig_feat == dest_feat)
        return (false);

    if (is_notable_terrain(dest_feat))
        return (false);

    actor* dest_actor = actor_at(dest_pos);

    if (orig_actor && !orig_actor->is_habitable_feat(dest_feat))
        return (false);
    if (dest_actor && !dest_actor->is_habitable_feat(orig_feat))
        return (false);

    return (true);
}

bool slide_feature_over(const coord_def &src, coord_def prefered_dest,
                        bool announce)
{
    ASSERT(in_bounds(src));

    const dungeon_feature_type orig_feat = grd(src);
    const actor* orig_actor = actor_at(src);

    if (in_bounds(prefered_dest)
        && _ok_dest_cell(orig_actor, orig_feat, prefered_dest))
    {
        ASSERT(prefered_dest != src);
    }
    else
    {
        int squares = 0;
        for (adjacent_iterator ai(src); ai; ++ai)
        {
            if (_ok_dest_cell(orig_actor, orig_feat, *ai)
                && one_chance_in(++squares))
            {
                prefered_dest = *ai;
            }
        }
    }

    if (!in_bounds(prefered_dest))
        return (false);

    ASSERT(prefered_dest != src);
    return swap_features(src, prefered_dest, false, announce);
}

// Returns true if we manage to scramble free.
bool fall_into_a_pool( const coord_def& entry, bool allow_shift,
                       unsigned char terrain )
{
    bool escape = false;
    coord_def empty;

    if (you.species == SP_MERFOLK && terrain == DNGN_DEEP_WATER)
    {
        // These can happen when we enter deep water directly -- bwr
        merfolk_start_swimming();
        return (false);
    }

    // sanity check
    if (terrain != DNGN_LAVA && (beogh_water_walk() || you.can_swim()))
        return (false);

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
            mpr( "The lava burns you to a cinder!" );
            ouch(INSTANT_DEATH, NON_MONSTER, KILLED_BY_LAVA);
        }
        else
        {
            // should boost # of bangs per damage in the future {dlb}
            mpr( "The lava burns you!" );
            ouch((10 + roll_dice(2, 50)) / resist, NON_MONSTER, KILLED_BY_LAVA);
        }

        expose_player_to_element( BEAM_LAVA, 14 );
    }

    // a distinction between stepping and falling from you.duration[DUR_LEVITATION]
    // prevents stepping into a thin stream of lava to get to the other side.
    if (scramble())
    {
        if (allow_shift)
        {
            escape = empty_surrounds(you.pos(), DNGN_FLOOR, 1, false, empty);
        }
        else
        {
            // back out the way we came in, if possible
            if (grid_distance(you.pos(), entry) == 1
                && !monster_at(entry))
            {
                escape = true;
                empty = entry;
            }
            else  // zero or two or more squares away, with no way back
            {
                escape = false;
            }
        }
    }
    else
    {
        if (you.attribute[ATTR_TRANSFORMATION] == TRAN_STATUE)
            mpr("You sink like a stone!");
        else
            mpr("You try to escape, but your burden drags you down!");
    }

    if (escape)
    {
        if (in_bounds(empty) && !is_feat_dangerous(grd(empty)))
        {
            mpr("You manage to scramble free!");
            move_player_to_grid( empty, false, false, true );

            if (terrain == DNGN_LAVA)
                expose_player_to_element( BEAM_LAVA, 14 );

            return (true);
        }
    }

    mpr("You drown...");

    if (terrain == DNGN_LAVA)
        ouch(INSTANT_DEATH, NON_MONSTER, KILLED_BY_LAVA);
    else if (terrain == DNGN_DEEP_WATER)
        ouch(INSTANT_DEATH, NON_MONSTER, KILLED_BY_WATER);

    return (false);
}

typedef std::map<std::string, dungeon_feature_type> feat_desc_map;
static feat_desc_map feat_desc_cache;

void init_feat_desc_cache()
{
    for (int i = 0; i < NUM_FEATURES; i++)
    {
        dungeon_feature_type feat = static_cast<dungeon_feature_type>(i);
        std::string          desc = feature_description(feat);

        lowercase(desc);
        if (feat_desc_cache.find(desc) == feat_desc_cache.end())
            feat_desc_cache[desc] = feat;
    }
}

dungeon_feature_type feat_by_desc(std::string desc)
{
    lowercase(desc);

    if (desc[desc.size() - 1] != '.')
        desc += ".";

    feat_desc_map::iterator i = feat_desc_cache.find(desc);

    if (i != feat_desc_cache.end())
        return (i->second);

    return (DNGN_UNSEEN);
}

// If active is true, the player is just stepping onto the feature, with the
// message: "<feature> slides away as you move <prep> it!"
// Else, the actor is already on the feature:
// "<feature> moves from <prep origin> to <prep destination>!"
std::string feat_preposition(dungeon_feature_type feat, bool active,
                             const actor* who)
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

std::string stair_climb_verb(dungeon_feature_type feat)
{
    ASSERT(feat_stair_direction(feat) != CMD_NO_CMD);

    if (feat_is_staircase(feat))
        return "climb";
    else if (feat_is_escape_hatch(feat))
        return "use";
    else
        return "pass through";
}

const char *dngn_feature_names[] =
{
"unseen", "closed_door", "detected_secret_door", "secret_door",
"wax_wall", "metal_wall", "green_crystal_wall", "rock_wall",
"slimy_wall", "stone_wall", "permarock_wall",
"clear_rock_wall", "clear_stone_wall", "clear_permarock_wall", "open_sea",
"tree", "orcish_idol", "", "", "", "",
"granite_statue", "statue_reserved_1", "statue_reserved_2",
"", "", "", "", "", "", "", "",
"", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
"", "", "", "", "", "", "", "", "", "", "", "", "", "lava",
"deep_water", "", "", "shallow_water", "water_stuck", "floor",
"floor_special", "floor_reserved", "exit_hell", "enter_hell",
"open_door", "", "", "trap_mechanical", "trap_magical", "trap_natural",
"undiscovered_trap", "", "enter_shop", "enter_labyrinth",
"stone_stairs_down_i", "stone_stairs_down_ii",
"stone_stairs_down_iii", "escape_hatch_down", "stone_stairs_up_i",
"stone_stairs_up_ii", "stone_stairs_up_iii", "escape_hatch_up", "",
"", "enter_dis", "enter_gehenna", "enter_cocytus",
"enter_tartarus", "enter_abyss", "exit_abyss", "stone_arch",
"enter_pandemonium", "exit_pandemonium", "transit_pandemonium",
"", "", "", "builder_special_wall", "builder_special_floor", "",
"", "", "enter_orcish_mines", "enter_hive", "enter_lair",
"enter_slime_pits", "enter_vaults", "enter_crypt",
"enter_hall_of_blades", "enter_zot", "enter_temple",
"enter_snake_pit", "enter_elven_halls", "enter_tomb",
"enter_swamp", "enter_shoals", "enter_reserved_2",
"enter_reserved_3", "enter_reserved_4", "", "", "",
"return_from_orcish_mines", "return_from_hive",
"return_from_lair", "return_from_slime_pits",
"return_from_vaults", "return_from_crypt",
"return_from_hall_of_blades", "return_from_zot",
"return_from_temple", "return_from_snake_pit",
"return_from_elven_halls", "return_from_tomb",
"return_from_swamp", "return_from_shoals", "return_reserved_2",
"return_reserved_3", "return_reserved_4", "", "", "", "", "", "",
"", "", "", "", "", "", "", "enter_portal_vault", "exit_portal_vault",
"", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
"", "", "altar_zin", "altar_shining_one", "altar_kikubaaqudgha",
"altar_yredelemnul", "altar_xom", "altar_vehumet",
"altar_okawaru", "altar_makhleb", "altar_sif_muna", "altar_trog",
"altar_nemelex_xobeh", "altar_elyvilon", "altar_lugonu",
"altar_beogh", "altar_jiyva", "altar_fedhas", "altar_cheibriados", "", "", "",
"fountain_blue", "fountain_sparkling", "fountain_blood",
"dry_fountain_blue", "dry_fountain_sparkling", "dry_fountain_blood",
"permadry_fountain", "abandoned_shop"
};

dungeon_feature_type dungeon_feature_by_name(const std::string &name)
{
    COMPILE_CHECK(ARRAYSZ(dngn_feature_names) == NUM_FEATURES, c1);
    if (name.empty())
        return (DNGN_UNSEEN);

    for (unsigned i = 0; i < ARRAYSZ(dngn_feature_names); ++i)
    {
        if (dngn_feature_names[i] == name)
        {
            if (jiyva_is_dead() && name == "altar_jiyva")
                return (DNGN_FLOOR);

            return (static_cast<dungeon_feature_type>(i));
        }
    }

    return (DNGN_UNSEEN);
}

std::vector<std::string> dungeon_feature_matches(const std::string &name)
{
    std::vector<std::string> matches;

    COMPILE_CHECK(ARRAYSZ(dngn_feature_names) == NUM_FEATURES, c1);
    if (name.empty())
        return (matches);

    for (unsigned i = 0; i < ARRAYSZ(dngn_feature_names); ++i)
        if (strstr(dngn_feature_names[i], name.c_str()))
            matches.push_back(dngn_feature_names[i]);

    return (matches);
}

const char *dungeon_feature_name(dungeon_feature_type rfeat)
{
    const unsigned feat = rfeat;

    if (feat >= ARRAYSZ(dngn_feature_names))
        return (NULL);

    return dngn_feature_names[feat];
}
