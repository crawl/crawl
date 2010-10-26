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

#include "coordit.h"
#include "coord.h"
#include "random.h"
#include "env.h"

struct crawl_environment env;

int count_neighbours_with_func (const coord_def& c, bool (*checker)(dungeon_feature_type))
{
    int count = 0;
    for (adjacent_iterator ai(c); ai; ++ai)
    {
        if (checker(grd(*ai)))
            count++;
    }
    return count;
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
        return (true);
    }

    // All branch entries/exits are staircases, except for Zot.
    if (feat == DNGN_ENTER_ZOT || feat == DNGN_RETURN_FROM_ZOT)
        return (false);

    return (feat >= DNGN_ENTER_FIRST_BRANCH && feat <= DNGN_ENTER_LAST_BRANCH)
            || (feat >= DNGN_RETURN_FROM_FIRST_BRANCH
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
    case DNGN_ENTER_DWARF_HALL:
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
    case DNGN_RETURN_FROM_DWARF_HALL:
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
    return feat == DNGN_ORCISH_IDOL
            || feat == DNGN_GRANITE_STATUE
            || feat == DNGN_SECRET_DOOR
            || (feat >= DNGN_ROCK_WALL
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
            || (undiscovered_too && feat == DNGN_UNDISCOVERED_TRAP));
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

bool feat_is_branch_stairs(dungeon_feature_type feat)
{
    return ((feat >= DNGN_ENTER_FIRST_BRANCH && feat <= DNGN_ENTER_LAST_BRANCH)
            || (feat >= DNGN_ENTER_DIS && feat <= DNGN_ENTER_TARTARUS));
}

bool feat_is_bidirectional_portal(dungeon_feature_type feat)
{
    return (feat != DNGN_ENTER_ZOT
            && feat != DNGN_RETURN_FROM_ZOT
            && feat != DNGN_EXIT_HELL);
}

// Find all connected cells containing ft, starting at d.
void find_connected_identical(const coord_def &d, dungeon_feature_type ft,
                              std::set<coord_def>& out)
{
    if (grd(d) != ft)
        return;

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


typedef FixedArray<bool, GXM, GYM> map_mask_boolean;
static std::auto_ptr<map_mask_boolean> _slime_wall_precomputed_neighbour_mask;

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
            return (true);
    return (false);
}

#if 0
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
#endif

bool is_critical_feature(dungeon_feature_type feat)
{
    return (DNGN_TEMP_PORTAL);
}

const char *dngn_feature_names[] =
{
"unseen", "closed_door", "detected_secret_door", "secret_door",
"wax_wall", "metal_wall", "green_crystal_wall", "rock_wall",
"slimy_wall", "stone_wall", "permarock_wall",
"clear_rock_wall", "clear_stone_wall", "clear_permarock_wall", "iron_grate",
"open_sea", "tree", "orcish_idol", "", "", "",
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
"", "", "enter_dwarf_hall", "enter_orcish_mines", "enter_hive", "enter_lair",
"enter_slime_pits", "enter_vaults", "enter_crypt",
"enter_hall_of_blades", "enter_zot", "enter_temple",
"enter_snake_pit", "enter_elven_halls", "enter_tomb",
"enter_swamp", "enter_shoals", "enter_reserved_2",
"enter_reserved_3", "enter_reserved_4", "", "",
"return_from_dwarf_hall", "return_from_orcish_mines", "return_from_hive",
"return_from_lair", "return_from_slime_pits",
"return_from_vaults", "return_from_crypt",
"return_from_hall_of_blades", "return_from_zot",
"return_from_temple", "return_from_snake_pit",
"return_from_elven_halls", "return_from_tomb",
"return_from_swamp", "return_from_shoals", "return_reserved_2",
"return_reserved_3", "return_reserved_4", "", "", "", "", "",
"", "", "", "", "", "", "", "enter_portal_vault", "exit_portal_vault",
"", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
"", "", "altar_zin", "altar_shining_one", "altar_kikubaaqudgha",
"altar_yredelemnul", "altar_xom", "altar_vehumet",
"altar_okawaru", "altar_makhleb", "altar_sif_muna", "altar_trog",
"altar_nemelex_xobeh", "altar_elyvilon", "altar_lugonu",
"altar_beogh", "altar_jiyva", "altar_fedhas", "altar_cheibriados",
"altar_ashenzari", "", "",
"fountain_blue", "fountain_sparkling", "fountain_blood",
"dry_fountain_blue", "dry_fountain_sparkling", "dry_fountain_blood",
"permadry_fountain", "abandoned_shop"
};

dungeon_feature_type dungeon_feature_by_name(const std::string &name)
{
    if (name.empty())
        return (DNGN_UNSEEN);

    for (unsigned i = 0; i < ARRAYSZ(dngn_feature_names); ++i)
    {
        if (dngn_feature_names[i] == name)
        {
            return (static_cast<dungeon_feature_type>(i));
        }
    }

    return (DNGN_UNSEEN);
}

const char *dungeon_feature_name(dungeon_feature_type rfeat)
{
    const unsigned feat = rfeat;

    if (feat >= ARRAYSZ(dngn_feature_names))
        return (NULL);

    return dngn_feature_names[feat];
}
