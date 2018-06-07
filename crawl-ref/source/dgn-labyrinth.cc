/**
 * @file
 * @brief The dungeon builder for Lab.
**/

#include "AppHdr.h"

#include "dgn-labyrinth.h"

#include "bloodspatter.h"
#include "coordit.h"
#include "dungeon.h"
#include "items.h"
#include "mapmark.h"
#include "maps.h"
#include "mgen-data.h"
#include "mon-pathfind.h"
#include "mon-place.h"
#include "terrain.h"

typedef list<coord_def> coord_list;

struct dist_feat
{
    int dist;
    dungeon_feature_type feat;

    dist_feat(int _d = 0, dungeon_feature_type _f = DNGN_UNSEEN)
        : dist(_d), feat(_f)
        {
        }
};

static void _find_maze_neighbours(const coord_def &c,
                                  const dgn_region &region,
                                  coord_list &ns)
{
    vector<coord_def> coords;

    for (int yi = -2; yi <= 2; yi += 2)
        for (int xi = -2; xi <= 2; xi += 2)
        {
            if (!!xi == !!yi)
                continue;

            const coord_def cp(c.x + xi, c.y + yi);
            if (region.contains(cp))
                coords.push_back(cp);
        }

    while (!coords.empty())
    {
        const int n = random2(coords.size());
        ns.push_back(coords[n]);
        coords.erase(coords.begin() + n);
    }
}

static void _labyrinth_maze_recurse(const coord_def &c, const dgn_region &where)
{
    coord_list neighbours;
    _find_maze_neighbours(c, where, neighbours);

    coord_list deferred;
    for (auto nc : neighbours)
    {
        if (grd(nc) != DNGN_ROCK_WALL)
            continue;

        grd(nc) = DNGN_FLOOR;
        grd(c + (nc - c) / 2) = DNGN_FLOOR;

        if (!one_chance_in(5))
            _labyrinth_maze_recurse(nc, where);
        else
            deferred.push_back(nc);
    }

    for (auto dc : deferred)
        _labyrinth_maze_recurse(dc, where);
}

static void _labyrinth_build_maze(coord_def &e, const dgn_region &lab)
{
    _labyrinth_maze_recurse(lab.random_point(), lab);

    do
    {
        e = lab.random_point();
    }
    while (grd(e) != DNGN_FLOOR);
}

static void _labyrinth_place_items(const coord_def &end)
{
    int num_items = 7 + random2avg(10, 2);
    for (int i = 0; i < num_items; i++)
    {
        const object_class_type glopop = random_choose_weighted(
                                       14, OBJ_WEAPONS,
                                       14, OBJ_ARMOUR,
                                       3, OBJ_MISSILES,
                                       3, OBJ_MISCELLANY,
                                       14, OBJ_WANDS,
                                       10, OBJ_SCROLLS,
                                       10, OBJ_JEWELLERY,
                                       8, OBJ_BOOKS,
                                       4, OBJ_STAVES);

        const int treasure_item =
            items(true, glopop, OBJ_RANDOM,
                  one_chance_in(3) ? env.absdepth0 * 3 : ISPEC_GOOD_ITEM);

        if (treasure_item != NON_ITEM)
            mitm[treasure_item].pos = end;
    }
}

static void _labyrinth_place_exit(const coord_def &end)
{
    _labyrinth_place_items(end);
    mons_place(mgen_data::sleeper_at(MONS_MINOTAUR, end, MG_PATROLLING));
    grd(end) = DNGN_EXIT_LABYRINTH;
}

// Change the borders of the labyrinth to another (undiggable) wall type.
static void _change_labyrinth_border(const dgn_region &region,
                                     const dungeon_feature_type wall)
{
    const coord_def &end = region.pos + region.size;
    int leftmost = end.x, rightmost = region.pos.x,
        upmost = end.y, downmost = region.pos.y;
    for (int y = region.pos.y-1; y <= end.y; ++y)
    {
        for (int x = region.pos.x-1; x <= end.x; ++x)
        {
            if (!feat_is_solid(grd[x][y]))
            {
                if (x < leftmost)
                    leftmost = x;
                if (x > rightmost)
                    rightmost = x;
                if (y < upmost)
                    upmost = y;
                if (y > downmost)
                    downmost = y;
            }
        }
    }

    for (int y = region.pos.y-1; y <= end.y; ++y)
    {
        for (int x = region.pos.x-1; x <= end.x; ++x)
        {
            if (x < leftmost
                || x > rightmost
                || y < upmost
                || y > downmost)
            {
                ASSERT(feat_is_solid(grd[x][y]));
                grd[x][y] = wall;
            }
        }
    }
}

// Called as:
// change_walls_from_centre(region_affected, centre,
//                          { {dist1, feat1}, {dist2, feat2}, ... } )
// What it does:
// Examines each non-vault rock wall in region_affected, calculates
// its (circular) distance from "centre" (the centre need not be in
// region_affected). If distance^2 is less than or equal to dist1,
// then it is replaced by feat1. Otherwise, if distance^2 <= dist2,
// it is replaced by feat2, and so on. Features in vaults (MMT_VAULT)
// are not affected.
static void _change_walls_from_centre(const dgn_region &region,
                                      const coord_def &centre,
                                      const vector<dist_feat> &ldist)
{
    if (ldist.empty())
        return;

    const coord_def &end = region.pos + region.size;
    for (int y = region.pos.y; y < end.y; ++y)
        for (int x = region.pos.x; x < end.x; ++x)
        {
            const coord_def c(x, y);
            if (grd(c) != DNGN_ROCK_WALL || map_masked(c, MMT_VAULT))
                continue;

            const int distance = (c - centre).abs();

            for (const dist_feat &df : ldist)
            {
                if (distance <= df.dist)
                {
                    grd(c) = df.feat;
                    break;
                }
            }
        }
}

static void _place_extra_lab_minivaults()
{
    set<const map_def*> vaults_used;
    while (true)
    {
        const map_def *vault = random_map_in_depth(level_id::current(), true);
        if (!vault || vaults_used.count(vault))
            break;

        vaults_used.insert(vault);
        if (!dgn_safe_place_map(vault, true, false))
            break;
    }
}

// Checks if there is a square with the given mask within radius of pos.
static bool _has_vault_in_radius(const coord_def &pos, int radius,
                                 unsigned mask)
{
    for (radius_iterator rad(pos, radius, C_SQUARE); rad; ++rad)
    {
        if (!in_bounds(*rad))
            continue;
        if (map_masked(*rad, mask))
            return true;
    }

    return false;
}

// Find an entry point that's:
// * At least 24 squares away from the exit.
// * At least 5 squares away from the nearest vault.
// * Floor (well, obviously).
static coord_def _labyrinth_find_entry_point(const dgn_region &reg,
                                             const coord_def &end)
{
    const int min_distance = 24;
    // Try many times.
    for (int i = 0; i < 2000; ++i)
    {
        const coord_def place = reg.random_point();
        if (grd(place) != DNGN_FLOOR)
            continue;

        if ((place - end).rdist() < min_distance)
            continue;

        if (_has_vault_in_radius(place, 5, MMT_VAULT))
            continue;

        return place;
    }
    coord_def unfound;
    return unfound;
}

static void _labyrinth_place_entry_point(const dgn_region &region,
                                         const coord_def &pos)
{
    const coord_def p = _labyrinth_find_entry_point(region, pos);
    if (in_bounds(p))
        env.markers.add(new map_feature_marker(p, DNGN_ENTER_LABYRINTH));
}

static bool _is_deadend(const coord_def& pos)
{
    int count_neighbours = 0;
    for (orth_adjacent_iterator ni(pos); ni; ++ni)
    {
        const coord_def& p = *ni;
        if (!in_bounds(p))
            continue;

        if (grd(p) == DNGN_FLOOR)
            count_neighbours++;
    }

    return count_neighbours <= 1;
}

static coord_def _find_random_deadend(const dgn_region &region)
{
    int tries = 0;
    coord_def result;
    bool floor_pos = false;
    while (++tries < 50)
    {
        coord_def pos = region.random_point();
        if (grd(pos) != DNGN_FLOOR)
            continue;
        else if (!floor_pos)
        {
            result = pos;
            floor_pos = true;
        }

        if (!_is_deadend(pos))
            continue;

        return pos;
    }

    return result;
}

// Adds a bloody trail ending in a dead end with spattered walls.
static void _labyrinth_add_blood_trail(const dgn_region &region)
{
    if (one_chance_in(5))
        return;

    int count_trails = 1 + one_chance_in(5) + one_chance_in(7);

    int tries = 0;
    while (++tries < 20)
    {
        const coord_def start = _find_random_deadend(region);
        const coord_def dest  = region.random_point();
        monster_pathfind mp;
        if (!mp.init_pathfind(dest, start))
            continue;

        const vector<coord_def> path = mp.backtrack();

        if (path.size() < 10)
            continue;

        maybe_bloodify_square(start);
#ifdef WIZARD
        if (you.wizard)
            env.pgrid(start) |= FPROP_HIGHLIGHT;
#endif
        bleed_onto_floor(start, MONS_HUMAN, 150, true, false, INVALID_COORD,
                         true);

        for (unsigned int step = 0; step < path.size(); step++)
        {
            coord_def pos = path[step];

            if (step < 2 || step < 12 && coinflip()
                || step >= 12 && one_chance_in(step/4))
            {
                maybe_bloodify_square(pos);
            }
#ifdef WIZARD
            if (you.wizard)
                env.pgrid(pos) |= FPROP_HIGHLIGHT;
#endif

            if (step >= 10 && one_chance_in(7))
                break;
        }

        if (--count_trails > 0)
            continue;

        break;
    }
}

static bool _find_random_nonmetal_wall(const dgn_region &region,
                                       coord_def &pos)
{
    int tries = 0;
    while (++tries < 50)
    {
        pos = region.random_point();
        if (!in_bounds(pos))
            continue;

        if (grd(pos) == DNGN_ROCK_WALL || grd(pos) == DNGN_STONE_WALL)
            return true;
    }
    return false;
}

static bool _grid_has_wall_neighbours(const coord_def& pos,
                                      const coord_def& next_samedir)
{
    for (orth_adjacent_iterator ni(pos); ni; ++ni)
    {
        const coord_def& p = *ni;
        if (!in_bounds(p))
            continue;

        if (p == next_samedir)
            continue;

        if (grd(p) == DNGN_ROCK_WALL || grd(p) == DNGN_STONE_WALL)
            return true;
    }
    return false;
}

static void _vitrify_wall_and_neighbours(const coord_def& pos, bool first)
{
    if (grd(pos) == DNGN_ROCK_WALL)
        grd(pos) = DNGN_CLEAR_ROCK_WALL;
    else if (grd(pos) == DNGN_STONE_WALL)
        grd(pos) = DNGN_CLEAR_STONE_WALL;
    else
        return;

    if (you.wizard)
        env.pgrid(pos) |= FPROP_HIGHLIGHT;

    for (orth_adjacent_iterator ai(pos); ai; ++ai)
    {
        const coord_def& p = *ai;
        if (!in_bounds(p) || map_masked(p, MMT_VAULT))
            continue;

        // Always continue vitrification if there are adjacent
        // walls other than continuing in the same direction.
        // Otherwise, there's still a 40% chance of continuing.
        // Also, always do more than one iteration.
        if (first || x_chance_in_y(2,5)
            || _grid_has_wall_neighbours(p, p + (p - pos)))
        {
            _vitrify_wall_and_neighbours(p, false);
        }
    }
}

// Turns some connected rock or stone walls into the transparent versions.
static void _labyrinth_add_glass_walls(const dgn_region &region)
{
    int glass_num = 2 + random2(6);

    if (one_chance_in(3)) // Potentially lots of glass.
        glass_num += random2(7);

    coord_def pos;
    for (int i = 0; i < glass_num; ++i)
    {
        if (!_find_random_nonmetal_wall(region, pos))
            break;

        if (_has_vault_in_radius(pos, 5, MMT_VAULT))
            continue;

        _vitrify_wall_and_neighbours(pos, true);
    }
}

void dgn_build_labyrinth_level()
{
    env.level_build_method += " labyrinth";
    env.level_layout_types.insert("labyrinth");

    dgn_region lab = dgn_region::absolute(LABYRINTH_BORDER,
                                           LABYRINTH_BORDER,
                                           GXM - LABYRINTH_BORDER - 1,
                                           GYM - LABYRINTH_BORDER - 1);

    // First decide if we're going to use a Lab minivault.
    const map_def *vault = random_map_in_depth(level_id::current(), false,
                                               MB_FALSE);
    vault_placement place;

    coord_def end;
    _labyrinth_build_maze(end, lab);

    if (!vault || !dgn_safe_place_map(vault, true, false))
    {
        vault = nullptr;
        _labyrinth_place_exit(end);
    }
    else
    {
        const vault_placement &rplace = *env.level_vaults.back();
        if (rplace.map.has_tag("generate_loot"))
        {
            for (vault_place_iterator vi(rplace); vi; ++vi)
                if (grd(*vi) == DNGN_EXIT_LABYRINTH || feat_is_stone_stair(grd(*vi)))
                {
                    _labyrinth_place_items(*vi);
                    break;
                }
        }
        place.pos  = rplace.pos;
        place.size = rplace.size;
    }

    if (vault)
        end = place.pos + place.size / 2;

    _place_extra_lab_minivaults();

    _change_walls_from_centre(lab, end, { { 15 * 15, DNGN_METAL_WALL },
                                          { 34 * 34, DNGN_STONE_WALL } });

    _change_labyrinth_border(lab, DNGN_PERMAROCK_WALL);

    _labyrinth_add_blood_trail(lab);
    _labyrinth_add_glass_walls(lab);

    _labyrinth_place_entry_point(lab, end);

    link_items();
}
