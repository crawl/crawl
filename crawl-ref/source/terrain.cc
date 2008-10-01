/*
 *  File:       terrain.cc
 *  Summary:    Terrain related functions.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 */

#include "AppHdr.h"
#include "externs.h"
#include "terrain.h"

#include <algorithm>

#include "dgnevent.h"
#include "directn.h"
#include "itemprop.h"
#include "items.h"
#include "message.h"
#include "misc.h"
#include "monplace.h"
#include "monstuff.h"
#include "mstuff2.h"
#include "ouch.h"
#include "overmap.h"
#include "player.h"
#include "religion.h"
#include "spells3.h"
#include "stuff.h"
#include "transfor.h"
#include "traps.h"
#include "view.h"

bool grid_is_wall(dungeon_feature_type grid)
{
    return (grid >= DNGN_MINWALL && grid <= DNGN_MAXWALL);
}

bool grid_is_stone_stair(dungeon_feature_type grid)
{
    switch (grid)
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

bool grid_is_escape_hatch(dungeon_feature_type grid)
{
    return (grid == DNGN_ESCAPE_HATCH_UP || grid == DNGN_ESCAPE_HATCH_DOWN);
}

bool grid_sealable_portal(dungeon_feature_type grid)
{
    switch (grid)
    {
    case DNGN_ENTER_HELL:
    case DNGN_ENTER_ABYSS:
    case DNGN_ENTER_PANDEMONIUM:
    case DNGN_ENTER_LABYRINTH:
    case DNGN_ENTER_PORTAL_VAULT:
        return (true);
    default:
        return (false);
    }
}

bool grid_is_portal(dungeon_feature_type grid)
{
    return (grid == DNGN_ENTER_PORTAL_VAULT || grid == DNGN_EXIT_PORTAL_VAULT);
}

command_type grid_stair_direction(dungeon_feature_type grid)
{
    switch (grid)
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
    case DNGN_RETURN_RESERVED_2:
    case DNGN_RETURN_RESERVED_3:
    case DNGN_RETURN_RESERVED_4:
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
    case DNGN_ENTER_RESERVED_2:
    case DNGN_ENTER_RESERVED_3:
    case DNGN_ENTER_RESERVED_4:
        return (CMD_GO_DOWNSTAIRS);

    default:
        return (CMD_NO_CMD);
    }
}

bool grid_is_opaque( dungeon_feature_type grid )
{
    return (grid < DNGN_MINSEE);
}

bool grid_is_solid( dungeon_feature_type grid )
{
    return (grid < DNGN_MINMOVE);
}

bool grid_is_solid( int x, int y )
{
    return (grid_is_solid(grd[x][y]));
}

bool grid_is_solid(const coord_def &c)
{
    return (grid_is_solid(grd(c)));
}

bool grid_is_rock(dungeon_feature_type grid)
{
    return (grid == DNGN_ORCISH_IDOL
            || grid == DNGN_GRANITE_STATUE
            || grid == DNGN_SECRET_DOOR
            || grid >= DNGN_ROCK_WALL
               && grid <= DNGN_CLEAR_PERMAROCK_WALL);
}

bool grid_is_permarock(dungeon_feature_type grid)
{
    return (grid == DNGN_PERMAROCK_WALL || grid == DNGN_CLEAR_PERMAROCK_WALL);
}

bool grid_is_trap(dungeon_feature_type grid, bool undiscovered_too)
{
    return (grid == DNGN_TRAP_MECHANICAL || grid == DNGN_TRAP_MAGICAL
            || grid == DNGN_TRAP_NATURAL
            || (undiscovered_too && grid == DNGN_UNDISCOVERED_TRAP));
}

bool grid_is_water(dungeon_feature_type grid)
{
    return (grid == DNGN_SHALLOW_WATER || grid == DNGN_DEEP_WATER);
}

bool grid_is_watery( dungeon_feature_type grid )
{
    return (grid_is_water(grid) || grid == DNGN_FOUNTAIN_BLUE);
}

bool grid_destroys_items( dungeon_feature_type grid )
{
    return (grid == DNGN_LAVA || grid == DNGN_DEEP_WATER);
}

// Returns 0 if grid is not an altar, else it returns the GOD_* type.
god_type grid_altar_god( dungeon_feature_type grid )
{
    if (grid >= DNGN_ALTAR_FIRST_GOD && grid <= DNGN_ALTAR_LAST_GOD)
        return (static_cast<god_type>( grid - DNGN_ALTAR_FIRST_GOD + 1 ));

    return (GOD_NO_GOD);
}

// Returns DNGN_FLOOR for non-gods, otherwise returns the altar for the god.
dungeon_feature_type altar_for_god( god_type god )
{
    if (god == GOD_NO_GOD || god >= NUM_GODS)
        return (DNGN_FLOOR);  // Yeah, lame. Tell me about it.

    return static_cast<dungeon_feature_type>(DNGN_ALTAR_FIRST_GOD + god - 1);
}

bool grid_is_branch_stairs(dungeon_feature_type grid)
{
    return ((grid >= DNGN_ENTER_FIRST_BRANCH && grid <= DNGN_ENTER_LAST_BRANCH)
            || (grid >= DNGN_ENTER_DIS && grid <= DNGN_ENTER_TARTARUS));
}

// Find all connected cells containing ft, starting at d.
void find_connected_identical(coord_def d, dungeon_feature_type ft,
                              std::set<coord_def>& out)
{
    if (grd[d.x][d.y] != ft) return;

    std::string prop = env.markers.property_at(d, MAT_ANY,
                                               "connected_exclude");

    if (!prop.empty())
    {
        // Even if this square is excluded from being a part of connected
        // cells, add it if it's the starting square.
        if (out.size() == 0)
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
void find_connected_range(coord_def d, dungeon_feature_type ft_min,
                          dungeon_feature_type ft_max,
                          std::set<coord_def>& out)
{
    if (grd[d.x][d.y] < ft_min || grd[d.x][d.y] > ft_max) return;

    std::string prop = env.markers.property_at(d, MAT_ANY,
                                               "connected_exclude");

    if (!prop.empty())
    {
        // Even if this square is excluded from being a part of connected
        // cells, add it if it's the starting square.
        if (out.size() == 0)
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

void get_door_description(int door_size, const char** adjective, const char** noun)
{
    const char* descriptions[] = {
        "miniscule " , "buggy door",
        ""           , "door",
        "large "     , "door",
        ""           , "gate",
        "huge "      , "gate",
    };

    const unsigned int idx = MIN( (unsigned int) door_size*2,
                                  ARRAYSZ(descriptions) - 2 );
    *adjective = descriptions[idx];
    *noun = descriptions[idx+1];
}

unsigned int show_appearance(const coord_def &ep)
{
    unsigned int object = env.show(ep);
    const coord_def gc = view2grid(show2view(ep));

    if (object == DNGN_SECRET_DOOR)
        object = grid_secret_door_appearance(gc);

    return object;
}

dungeon_feature_type grid_appearance(const coord_def &gc)
{
    dungeon_feature_type grid = grd(gc);
    if (grid == DNGN_SECRET_DOOR)
        grid = grid_secret_door_appearance(gc);

    return grid;
}

dungeon_feature_type grid_secret_door_appearance(const coord_def &where)
{
    dungeon_feature_type ret = DNGN_FLOOR;

    for (int dx = -1; dx <= 1; dx++)
        for (int dy = -1; dy <= 1; dy++)
        {
            // only considering orthogonal grids
            if ((abs(dx) + abs(dy)) % 2 == 0)
                continue;

            const dungeon_feature_type targ = grd[where.x + dx][where.y + dy];

            if (!grid_is_wall( targ ))
                continue;

            if (ret == DNGN_FLOOR)
                ret = targ;
            else if (ret != targ)
                ret = ((ret < targ) ? ret : targ);
        }

    return ((ret == DNGN_FLOOR) ? DNGN_ROCK_WALL
                                : ret);
}

const char *grid_item_destruction_message(dungeon_feature_type grid)
{
    return (grid == DNGN_DEEP_WATER ? "You hear a splash." :
            grid == DNGN_LAVA       ? "You hear a sizzling splash."
                                    : "You hear a crunching noise.");
}

static coord_def _dgn_find_nearest_square(
    const coord_def &pos,
    bool (*acceptable)(const coord_def &),
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

            if (p != pos && acceptable(p))
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

static bool _item_safe_square(const coord_def &pos)
{
    const dungeon_feature_type feat = grd(pos);
    return (is_traversable(feat) && !grid_destroys_items(feat));
}

// Moves an item on the floor to the nearest adjacent floor-space.
static bool _dgn_shift_item(const coord_def &pos, item_def &item)
{
    const coord_def np = _dgn_find_nearest_square(pos, _item_safe_square);
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
    return (grid_stair_direction(feat) != CMD_NO_CMD
            || grid_altar_god(feat) != GOD_NO_GOD);
}

static bool _is_feature_shift_target(const coord_def &pos)
{
    return (grd(pos) == DNGN_FLOOR && !dungeon_events.has_listeners_at(pos));
}

static bool _dgn_shift_feature(const coord_def &pos)
{
    const dungeon_feature_type dfeat = grd(pos);
    if (!is_critical_feature(dfeat) && !env.markers.find(pos, MAT_ANY))
        return (false);

    const coord_def dest =
        _dgn_find_nearest_square(pos, _is_feature_shift_target);

    if (in_bounds(dest) && dest != pos)
    {
        grd(dest) = dfeat;

        if (dfeat == DNGN_ENTER_SHOP)
            if (shop_struct *s = get_shop(pos))
                s->pos = dest;

        env.markers.move(pos, dest);
        dungeon_events.move_listeners(pos, dest);
    }
    return (true);
}

static void _dgn_check_terrain_items(const coord_def &pos, bool preserve_items)
{
    const dungeon_feature_type grid = grd(pos);
    if (grid_is_solid(grid) || grid_destroys_items(grid))
    {
        int item = igrd(pos);
        bool did_destroy = false;
        while (item != NON_ITEM)
        {
            const int curr = item;
            item = mitm[item].link;

            // Game-critical item.
            if (preserve_items || item_is_critical(mitm[curr]))
                _dgn_shift_item(pos, mitm[curr]);
            else
            {
                item_was_destroyed(mitm[curr]);
                destroy_item(curr);
                did_destroy = true;
            }
        }
        if (did_destroy && player_can_hear(pos))
            mprf(MSGCH_SOUND, grid_item_destruction_message(grid));
    }
}

static void _dgn_check_terrain_monsters(const coord_def &pos)
{
    const int mindex = mgrd(pos);
    if (mindex != NON_MONSTER)
    {
        monsters *mons = &menv[mindex];

        if (mons->has_ench(ENCH_SUBMERGED)
            && !monster_can_submerge(mons, grd(pos)))
        {
            mons->del_ench(ENCH_SUBMERGED);
        }

        if (grid_is_solid(grd(pos)))
            monster_teleport(mons, true, false);
        else
            mons_check_pool(mons, KILL_MISC, -1);
    }

}

static void _dgn_check_terrain_blood(const coord_def &pos,
                                     dungeon_feature_type old_feat,
                                     dungeon_feature_type new_feat)
{
    if (env.map(pos).property != FPROP_BLOODY)
        return;

    if (new_feat == DNGN_UNSEEN)
    {
        // Caller has already changed the grid, and old_feat is actually
        // the new feat.
        if (old_feat != DNGN_FLOOR && !grid_is_solid(old_feat))
            env.map(pos).property = FPROP_NONE;
    }
    else
    {
        if (grid_is_solid(old_feat) != grid_is_solid(new_feat)
            || grid_is_water(new_feat) || grid_destroys_items(new_feat)
            || is_critical_feature(new_feat))
        {
            env.map(pos).property = FPROP_NONE;
        }
    }
}

void dungeon_terrain_changed(const coord_def &pos,
                             dungeon_feature_type nfeat,
                             bool affect_player,
                             bool preserve_features,
                             bool preserve_items)
{
    if (grd(pos) == nfeat)
        return;

    _dgn_check_terrain_blood(pos, grd(pos), nfeat);

    if (nfeat != DNGN_UNSEEN)
    {
        if (preserve_features)
            _dgn_shift_feature(pos);

        unnotice_feature(level_pos(level_id::current(), pos));
        grd(pos) = nfeat;
        env.grid_colours(pos) = BLACK;
        if (is_notable_terrain(nfeat) && see_grid(pos))
            seen_notable_thing(nfeat, pos);

        destroy_trap(pos);
    }

    _dgn_check_terrain_items(pos, preserve_items);
    _dgn_check_terrain_monsters(pos);

    if (affect_player && pos == you.pos())
    {
        if (!grid_is_solid(grd(pos)))
        {
            if (!you.airborne())
            {
                // If the monster can't stay submerged in the new terrain
                // and there aren't any adjacent squares where it can
                // stay submerged then move it.
                if (mgrd(you.pos()) != NON_MONSTER
                    && !mons_is_submerged( &menv[ mgrd(you.pos()) ] ))
                {
                    monster_teleport( &menv[ mgrd(you.pos()) ], true, false);
                }
                move_player_to_grid(pos, false, true, false);
            }
        }
        else
            you_teleport_now(true, false);
    }

    set_terrain_changed(pos.x, pos.y);
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
    if (terrain != DNGN_LAVA && beogh_water_walk())
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
            if (grid_distance( you.pos(), entry ) == 1
                && mgrd(entry) == NON_MONSTER)
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
        if (in_bounds(empty) && !is_grid_dangerous(grd(empty)))
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
        dungeon_feature_type grid = static_cast<dungeon_feature_type>(i);
        std::string          desc = feature_description(grid);

        lowercase(desc);
        if (feat_desc_cache.find(desc) == feat_desc_cache.end())
            feat_desc_cache[desc] = grid;
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
