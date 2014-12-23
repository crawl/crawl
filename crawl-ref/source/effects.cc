/**
 * @file
 * @brief Misc stuff.
**/

#include "AppHdr.h"

#include "effects.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <queue>
#include <set>

#include "abyss.h"
#include "act-iter.h"
#include "areas.h"
#include "bloodspatter.h"
#include "branch.h"
#include "cloud.h"
#include "coordit.h"
#include "database.h"
#include "delay.h"
#include "dgnevent.h"
#include "dgn-shoals.h"
#include "directn.h"
#include "dungeon.h"
#include "english.h"
#include "exercise.h"
#include "fight.h"
#include "godabil.h"
#include "godpassive.h"
#include "hiscores.h"
#include "invent.h"
#include "itemprop.h"
#include "items.h"
#include "libutil.h"
#include "losglobal.h"
#include "macro.h"
#include "message.h"
#include "misc.h"
#include "mon-behv.h"
#include "mon-cast.h"
#include "mon-death.h"
#include "mon-pathfind.h"
#include "mon-place.h"
#include "mon-project.h"
#include "mutation.h"
#include "notes.h"
#include "player-stats.h"
#include "prompt.h"
#include "random-weight.h"
#include "religion.h"
#include "rot.h"
#include "shout.h"
#include "skills.h"
#include "spl-clouds.h"
#include "spl-miscast.h"
#include "spl-summoning.h"
#include "stairs.h"
#include "state.h"
#include "stringutil.h"
#include "terrain.h"
#include "throw.h"
#include "travel.h"
#include "unwind.h"
#include "view.h"
#include "viewchar.h"
#include "xom.h"

/**
 * Choose a random, spooky hell effect message, print it, and make a loud noise
 * if appropriate. (1/6 chance of loud noise.)
 */
static void _hell_effect_noise()
{
    const bool loud = one_chance_in(6) && !silenced(you.pos());
    string msg = getMiscString(loud ? "hell_effect_noisy"
                                    : "hell_effect_quiet");
    if (msg.empty())
        msg = "Something hellishly buggy happens.";

    mprf(MSGCH_HELL_EFFECT, "%s", msg.c_str());
    if (loud)
        noisy(15, you.pos());
}

/**
 * Choose a random miscast effect (from a weighted list) & apply it to the
 * player.
 */
static void _random_hell_miscast()
{
    const spschool_flag_type which_miscast
        = random_choose_weighted(8, SPTYP_NECROMANCY,
                                 4, SPTYP_SUMMONING,
                                 2, SPTYP_CONJURATION,
                                 1, SPTYP_CHARMS,
                                 1, SPTYP_HEXES,
                                 0);

    MiscastEffect(&you, nullptr, HELL_EFFECT_MISCAST, which_miscast,
                  4 + random2(6), random2avg(97, 3),
                  "the effects of Hell");
}

/// The thematically appropriate hell effects for a given hell branch.
struct hell_effect_spec
{
    /// The type of greater demon to spawn from hell effects.
    vector<monster_type> fiend_types;
    /// The appropriate theme of miscast effects to toss at the player.
    spschool_flag_type miscast_type;
    /// A weighted list of lesser creatures to spawn.
    vector<pair<monster_type, int>> minor_summons;
};

/// Hell effects for each branch of hell
static map<branch_type, hell_effect_spec> hell_effects_by_branch =
{
    { BRANCH_DIS, { {RANDOM_DEMON_GREATER}, SPTYP_EARTH, {
        { RANDOM_MONSTER, 100 }, // TODO
    }}},
    { BRANCH_GEHENNA, { {MONS_BRIMSTONE_FIEND}, SPTYP_FIRE, {
        { RANDOM_MONSTER, 100 }, // TODO
    }}},
    { BRANCH_COCYTUS, { {MONS_ICE_FIEND, MONS_SHARD_SHRIKE}, SPTYP_ICE, {
        // total weight 100
        { MONS_ZOMBIE, 15 },
        { MONS_SKELETON, 10 },
        { MONS_SIMULACRUM, 10 },
        { MONS_FREEZING_WRAITH, 10 },
        { MONS_FLYING_SKULL, 10 },
        { MONS_TORMENTOR, 10 },
        { MONS_REAPER, 10 },
        { MONS_BONE_DRAGON, 5 },
        { MONS_ICE_DRAGON, 5 },
        { MONS_BLIZZARD_DEMON, 5 },
        { MONS_BLUE_DEVIL, 5 },
        { MONS_ICE_DEVIL, 5 },
    }}},
    { BRANCH_TARTARUS, { {MONS_SHADOW_FIEND}, SPTYP_NECROMANCY, {
        { RANDOM_MONSTER, 100 }, // TODO
    }}},
};

/**
 * Either dump a fiend or a hell-appropriate miscast effect on the player.
 *
 * 40% chance of fiend, 60% chance of miscast.
 */
static void _themed_hell_summon_or_miscast()
{
    const hell_effect_spec *spec = map_find(hell_effects_by_branch,
                                            you.where_are_you);
    if (!spec)
        die("Attempting to call down a hell effect in a non-hellish branch.");

    if (x_chance_in_y(2, 5))
    {
        const monster_type fiend
            = spec->fiend_types[random2(spec->fiend_types.size())];
        create_monster(
                       mgen_data::hostile_at(fiend, "the effects of Hell",
                                             true, 0, 0, you.pos()));
    }
    else
    {
        MiscastEffect(&you, nullptr, HELL_EFFECT_MISCAST, spec->miscast_type,
                      4 + random2(6), random2avg(97, 3),
                      "the effects of Hell");
    }
}

/**
 * Try to summon at some number of random spawns from the current branch, to
 * harass the player & give them easy xp/TSO piety. Occasionally, to kill them.
 *
 * Min zero, max five, average 1.67.
 *
 * Can and does summon bands as individual spawns.
 */
static void _minor_hell_summons()
{
    hell_effect_spec *spec = map_find(hell_effects_by_branch,
                                      you.where_are_you);
    if (!spec)
        die("Attempting to call down a hell effect in a non-hellish branch.");

    // Try to summon at least one and up to five random monsters. {dlb}
    mgen_data mg;
    mg.pos = you.pos();
    mg.foe = MHITYOU;
    mg.non_actor_summoner = "the effects of Hell";
    create_monster(mg);

    for (int i = 0; i < 4; ++i)
    {
        if (one_chance_in(3))
        {
            monster_type *type
                = random_choose_weighted(spec->minor_summons);
            ASSERT(type);
            mg.cls = *type;
            create_monster(mg);
        }
    }
}

/// Nasty things happen to people who spend too long in Hell.
static void _hell_effects(int /*time_delta*/)
{
    if (!player_in_hell())
        return;

    // 50% chance at max piety
    if (you_worship(GOD_ZIN) && x_chance_in_y(you.piety, MAX_PIETY * 2)
        || is_sanctuary(you.pos()))
    {
        simple_god_message("'s power protects you from the chaos of Hell!");
        return;
    }

    _hell_effect_noise();

    if (one_chance_in(3))
        _random_hell_miscast();
    else if (x_chance_in_y(5, 9))
        _themed_hell_summon_or_miscast();

    if (one_chance_in(3))   // NB: No "else"
        _minor_hell_summons();
}

// This function checks whether we can turn a wall into a floor space and
// still keep a corridor-like environment. The wall in position x is a
// a candidate for switching if it's flanked by floor grids to two sides
// and by walls (any type) to the remaining cardinal directions.
//
//   .        #          2
//  #x#  or  .x.   ->   0x1
//   .        #          3
static bool _feat_is_flanked_by_walls(const coord_def &p)
{
    const coord_def adjs[] = { coord_def(p.x-1,p.y),
                               coord_def(p.x+1,p.y),
                               coord_def(p.x  ,p.y-1),
                               coord_def(p.x  ,p.y+1) };

    // paranoia!
    for (coord_def c : adjs)
        if (!in_bounds(c))
            return false;

    return feat_is_wall(grd(adjs[0])) && feat_is_wall(grd(adjs[1]))
              && feat_has_solid_floor(grd(adjs[2])) && feat_has_solid_floor(grd(adjs[3]))
           || feat_has_solid_floor(grd(adjs[0])) && feat_has_solid_floor(grd(adjs[1]))
              && feat_is_wall(grd(adjs[2])) && feat_is_wall(grd(adjs[3]));
}

// Sometimes if a floor is turned into a wall, a dead-end will be created.
// If this is the case, we need to make sure that it is at least two grids
// deep.
//
// Example: If a wall is built at X (A), two dead-ends are created, a short
//          and a long one. The latter is perfectly fine, but the former
//          looks a bit odd. If Y is chosen, this looks much better (B).
//
// #######    (A)  #######    (B)  #######
// ...XY..         ...#...         ....#..
// #.#####         #.#####         #.#####
//
// What this function does is check whether the neighbouring floor grids
// are flanked by walls on both sides, and if so, the grids following that
// also have to be floor flanked by walls.
//
//   czd
//   a.b   -> if (a, b == walls) then (c, d == walls) or return false
//   #X#
//    .
//
// Grid z may be floor or wall, either way we have a corridor of at least
// length 2.
static bool _deadend_check_wall(const coord_def &p)
{
    // The grids to the left and right of p are walls. (We already know that
    // they are symmetric, so only need to check one side. We also know that
    // the other direction, here up/down must then be non-walls.)
    if (feat_is_wall(grd[p.x-1][p.y]))
    {
        // Run the check twice, once in either direction.
        for (int i = -1; i <= 1; i++)
        {
            if (i == 0)
                continue;

            const coord_def a(p.x-1, p.y+i);
            const coord_def b(p.x+1, p.y+i);
            const coord_def c(p.x-1, p.y+2*i);
            const coord_def d(p.x+1, p.y+2*i);

            if (in_bounds(a) && in_bounds(b)
                && feat_is_wall(grd(a)) && feat_is_wall(grd(b))
                && (!in_bounds(c) || !in_bounds(d)
                    || !feat_is_wall(grd(c)) || !feat_is_wall(grd(d))))
            {
                return false;
            }
        }
    }
    else // The grids above and below p are walls.
    {
        for (int i = -1; i <= 1; i++)
        {
            if (i == 0)
                continue;

            const coord_def a(p.x+i  , p.y-1);
            const coord_def b(p.x+i  , p.y+1);
            const coord_def c(p.x+2*i, p.y-1);
            const coord_def d(p.x+2*i, p.y+1);

            if (in_bounds(a) && in_bounds(b)
                && feat_is_wall(grd(a)) && feat_is_wall(grd(b))
                && (!in_bounds(c) || !in_bounds(d)
                    || !feat_is_wall(grd(c)) || !feat_is_wall(grd(d))))
            {
                return false;
            }
        }
    }

    return true;
}

// Similar to the above, checks whether turning a wall grid into floor
// would create a short "dead-end" of only 1 grid.
//
// In the example below, X would create miniature dead-ends at positions
// a and b, but both Y and Z avoid this, and the resulting mini-mazes
// look much better.
//
// ########   (A)  ########     (B)  ########     (C)  ########
// #.....#.        #....a#.          #.....#.          #.....#.
// #.#YXZ#.        #.##.##.          #.#.###.          #.###.#.
// #.#.....        #.#b....          #.#.....          #.#.....
//
// In general, if a floor grid horizontally or vertically adjacent to the
// change target has a floor neighbour diagonally adjacent to the change
// target, the next neighbour in the same direction needs to be floor,
// as well.
static bool _deadend_check_floor(const coord_def &p)
{
    if (feat_is_wall(grd[p.x-1][p.y]))
    {
        for (int i = -1; i <= 1; i++)
        {
            if (i == 0)
                continue;

            const coord_def a(p.x, p.y+2*i);
            if (!in_bounds(a) || feat_has_solid_floor(grd(a)))
                continue;

            for (int j = -1; j <= 1; j++)
            {
                if (j == 0)
                    continue;

                const coord_def b(p.x+2*j, p.y+i);
                if (!in_bounds(b))
                    continue;

                const coord_def c(p.x+j, p.y+i);
                if (feat_has_solid_floor(grd(c)) && !feat_has_solid_floor(grd(b)))
                    return false;
            }
        }
    }
    else
    {
        for (int i = -1; i <= 1; i++)
        {
            if (i == 0)
                continue;

            const coord_def a(p.x+2*i, p.y);
            if (!in_bounds(a) || feat_has_solid_floor(grd(a)))
                continue;

            for (int j = -1; j <= 1; j++)
            {
                if (j == 0)
                    continue;

                const coord_def b(p.x+i, p.y+2*j);
                if (!in_bounds(b))
                    continue;

                const coord_def c(p.x+i, p.y+j);
                if (feat_has_solid_floor(grd(c)) && !feat_has_solid_floor(grd(b)))
                    return false;
            }
        }
    }

    return true;
}

// Changes a small portion of a labyrinth by exchanging wall against floor
// grids in such a way that connectivity remains guaranteed.
void change_labyrinth(bool msg)
{
    int size = random_range(12, 24); // size of the shifted area (square)
    coord_def c1, c2; // upper left, lower right corners of the shifted area

    vector<coord_def> targets;

    // Try 10 times for an area that is little mapped.
    for (int tries = 10; tries > 0; --tries)
    {
        targets.clear();

        int x = random_range(LABYRINTH_BORDER, GXM - LABYRINTH_BORDER - size);
        int y = random_range(LABYRINTH_BORDER, GYM - LABYRINTH_BORDER - size);
        c1 = coord_def(x, y);
        c2 = coord_def(x + size, y + size);

        int count_known = 0;
        for (rectangle_iterator ri(c1, c2); ri; ++ri)
            if (env.map_knowledge(*ri).seen())
                count_known++;

        if (tries > 1 && count_known > size * size / 6)
            continue;

        // Fill a vector with wall grids that are potential targets for
        // swapping against floor, i.e. are flanked by walls to two cardinal
        // directions, and by floor on the two remaining sides.
        for (rectangle_iterator ri(c1, c2); ri; ++ri)
        {
            if (env.map_knowledge(*ri).seen() || !feat_is_wall(grd(*ri)))
                continue;

            // Skip on grids inside vaults so as not to disrupt them.
            if (map_masked(*ri, MMT_VAULT))
                continue;

            // Make sure we don't accidentally create "ugly" dead-ends.
            if (_feat_is_flanked_by_walls(*ri) && _deadend_check_floor(*ri))
                targets.push_back(*ri);
        }

        if (targets.size() >= 8)
            break;
    }

    if (targets.empty())
    {
        if (msg)
            mpr("No unexplored wall grids found!");
        return;
    }

    if (msg)
    {
        mprf(MSGCH_DIAGNOSTICS, "Changing labyrinth from (%d, %d) to (%d, %d)",
             c1.x, c1.y, c2.x, c2.y);

        string path_str = "";
        mprf(MSGCH_DIAGNOSTICS, "Here's the list of targets: ");
        for (coord_def target : targets)
        {
            snprintf(info, INFO_SIZE, "(%d, %d)  ", target.x, target.y);
            path_str += info;
        }
        mprf(MSGCH_DIAGNOSTICS, "%s", path_str.c_str());
        mprf(MSGCH_DIAGNOSTICS, "-> #targets = %u", (unsigned int)targets.size());
    }

#ifdef WIZARD
    // Remove old highlighted areas to make place for the new ones.
    if (you.wizard)
        for (rectangle_iterator ri(1); ri; ++ri)
            env.pgrid(*ri) &= ~(FPROP_HIGHLIGHT);
#endif

    // How many switches we'll be doing.
    const int max_targets = random_range(min((int) targets.size(), 12),
                                         min((int) targets.size(), 45));

    // Shuffle the targets, then pick the max_targets first ones.
    shuffle_array(targets);

    // For each of the chosen wall grids, calculate the path connecting the
    // two floor grids to either side, and block off one floor grid on this
    // path to close the circle opened by turning the wall into floor.
    for (int count = 0; count < max_targets; count++)
    {
        const coord_def c(targets[count]);
        // Maybe not valid anymore...
        if (!feat_is_wall(grd(c)) || !_feat_is_flanked_by_walls(c))
            continue;

        // Use the adjacent floor grids as source and destination.
        coord_def src(c.x-1,c.y);
        coord_def dst(c.x+1,c.y);
        if (!feat_has_solid_floor(grd(src)) || !feat_has_solid_floor(grd(dst)))
        {
            src = coord_def(c.x, c.y-1);
            dst = coord_def(c.x, c.y+1);
        }

        // Pathfinding from src to dst...
        monster_pathfind mp;
        bool success = mp.init_pathfind(src, dst, false, msg);
        if (!success)
        {
            if (msg)
                mprf(MSGCH_DIAGNOSTICS, "Something went badly wrong - no path found!");
            continue;
        }

        // Get the actual path.
        const vector<coord_def> path = mp.backtrack();

        // Replace the wall with floor, but preserve the old grid in case
        // we find no floor grid to swap with.
        // It's better if the change is done now, so the grid can be
        // treated as floor rather than a wall, and we don't need any
        // special cases.
        dungeon_feature_type old_grid = grd(c);
        grd(c) = DNGN_FLOOR;
        set_terrain_changed(c);

        // Add all floor grids meeting a couple of conditions to a vector
        // of potential switch points.
        vector<coord_def> points;
        for (const coord_def p : path)
        {
            // The point must be inside the changed area.
            if (p.x < c1.x || p.x > c2.x || p.y < c1.y || p.y > c2.y)
                continue;

            // Only replace plain floor.
            if (grd(p) != DNGN_FLOOR)
                continue;

            // Don't change any grids we remember.
            if (env.map_knowledge(p).seen())
                continue;

            // We don't want to deal with monsters being shifted around.
            if (monster_at(p))
                continue;

            // Do not pick a grid right next to the original wall.
            if (abs(p.x-c.x) + abs(p.y-c.y) <= 1)
                continue;

            if (_feat_is_flanked_by_walls(p) && _deadend_check_wall(p))
                points.push_back(p);
        }

        if (points.empty())
        {
            // Take back the previous change.
            grd(c) = old_grid;
            set_terrain_changed(c);
            continue;
        }

        // Randomly pick one floor grid from the vector and replace it
        // with an adjacent wall type.
        const int pick = random_range(0, (int) points.size() - 1);
        const coord_def p(points[pick]);
        if (msg)
        {
            mprf(MSGCH_DIAGNOSTICS, "Switch %d (%d, %d) with %d (%d, %d).",
                 (int) old_grid, c.x, c.y, (int) grd(p), p.x, p.y);
        }
#ifdef WIZARD
        if (you.wizard)
        {
            // Highlight the switched grids.
            env.pgrid(c) |= FPROP_HIGHLIGHT;
            env.pgrid(p) |= FPROP_HIGHLIGHT;
        }
#endif

        // Shift blood some of the time.
        if (is_bloodcovered(c))
        {
            if (one_chance_in(4))
            {
                int wall_count = 0;
                coord_def old_adj(c);
                for (adjacent_iterator ai(c); ai; ++ai)
                    if (feat_is_wall(grd(*ai)) && one_chance_in(++wall_count))
                        old_adj = *ai;

                if (old_adj != c && maybe_bloodify_square(old_adj))
                    env.pgrid(c) &= (~FPROP_BLOODY);
            }
        }
        else if (one_chance_in(500))
        {
            // Rarely add blood randomly, accumulating with time...
            maybe_bloodify_square(c);
        }

        // Rather than use old_grid directly, replace with an adjacent
        // wall type, preferably stone, rock, or metal.
        old_grid = grd[p.x-1][p.y];
        if (!feat_is_wall(old_grid))
        {
            old_grid = grd[p.x][p.y-1];
            if (!feat_is_wall(old_grid))
            {
                if (msg)
                {
                    mprf(MSGCH_DIAGNOSTICS,
                         "No adjacent walls at pos (%d, %d)?", p.x, p.y);
                }
                old_grid = DNGN_STONE_WALL;
            }
            else if (old_grid != DNGN_ROCK_WALL && old_grid != DNGN_STONE_WALL
                     && old_grid != DNGN_METAL_WALL && !one_chance_in(3))
            {
                old_grid = grd[p.x][p.y+1];
            }
        }
        else if (old_grid != DNGN_ROCK_WALL && old_grid != DNGN_STONE_WALL
                 && old_grid != DNGN_METAL_WALL && !one_chance_in(3))
        {
            old_grid = grd[p.x+1][p.y];
        }
        grd(p) = old_grid;
        set_terrain_changed(p);

        // Shift blood some of the time.
        if (is_bloodcovered(p))
        {
            if (one_chance_in(4))
            {
                int floor_count = 0;
                coord_def new_adj(p);
                for (adjacent_iterator ai(c); ai; ++ai)
                    if (feat_has_solid_floor(grd(*ai)) && one_chance_in(++floor_count))
                        new_adj = *ai;

                if (new_adj != p && maybe_bloodify_square(new_adj))
                    env.pgrid(p) &= (~FPROP_BLOODY);
            }
        }
        else if (one_chance_in(100))
        {
            // Occasionally add blood randomly, accumulating with time...
            maybe_bloodify_square(p);
        }
    }

    // The directions are used to randomly decide where to place items that
    // have ended up in walls during the switching.
    vector<coord_def> dirs;
    dirs.emplace_back(-1,-1);
    dirs.emplace_back(0,-1);
    dirs.emplace_back(1,-1);
    dirs.emplace_back(-1, 0);

    dirs.emplace_back(1, 0);
    dirs.emplace_back(-1, 1);
    dirs.emplace_back(0, 1);
    dirs.emplace_back(1, 1);

    // Search the entire shifted area for stacks of items now stuck in walls
    // and move them to a random adjacent non-wall grid.
    for (rectangle_iterator ri(c1, c2); ri; ++ri)
    {
        if (!feat_is_wall(grd(*ri)) || igrd(*ri) == NON_ITEM)
            continue;

        if (msg)
        {
            mprf(MSGCH_DIAGNOSTICS,
                 "Need to move around some items at pos (%d, %d)...",
                 ri->x, ri->y);
        }
        // Search the eight possible directions in random order.
        shuffle_array(dirs);
        for (coord_def dir : dirs)
        {
            const coord_def p = *ri + dir;
            if (!in_bounds(p))
                continue;

            if (feat_has_solid_floor(grd(p)))
            {
                // Once a valid grid is found, move all items from the
                // stack onto it.
                move_items(*ri, p);

                if (msg)
                {
                    mprf(MSGCH_DIAGNOSTICS, "Moved items over to (%d, %d)",
                         p.x, p.y);
                }
                break;
            }
        }
    }

    // Recheck item coordinates, to make totally sure.
    fix_item_coordinates();

    // Finally, give the player a clue about what just happened.
    const int which = (silenced(you.pos()) ? 2 + random2(2)
                                           : random2(4));
    switch (which)
    {
    case 0: mpr("You hear an odd grinding sound!"); break;
    case 1: mpr("You hear the creaking of ancient gears!"); break;
    case 2: mpr("The floor suddenly vibrates beneath you!"); break;
    case 3: mpr("You feel a sudden draft!"); break;
    }
}

static void _handle_magic_contamination()
{
    int added_contamination = 0;

    // Scale has been increased by a factor of 1000, but the effect now happens
    // every turn instead of every 20 turns, so everything has been multiplied
    // by 50 and scaled to you.time_taken.

    if (you.duration[DUR_INVIS])
        added_contamination += 30;

    if (you.duration[DUR_HASTE])
        added_contamination += 30;

    // Is there a point to this? It's not even strong enough to cancel normal
    // dissipation, so it only slows it down. Shouldn't it cancel dissipation
    // like haste and invis do?
    if (you.duration[DUR_FINESSE])
        added_contamination += 20;

#if TAG_MAJOR_VERSION == 34
    if (you.duration[DUR_REGENERATION] && you.species == SP_DJINNI)
        added_contamination += 20;
#endif
    // The Orb halves dissipation (well a bit more, I had to round it),
    // but won't cause glow on its own -- otherwise it'd spam the player
    // with messages about contamination oscillating near zero.
    if (you.magic_contamination && orb_haloed(you.pos()))
        added_contamination += 13;

    // Normal dissipation
    if (!you.duration[DUR_INVIS] && !you.duration[DUR_HASTE])
        added_contamination -= 25;

    // Scaling to turn length
    added_contamination = div_rand_round(added_contamination * you.time_taken,
                                         BASELINE_DELAY);

    contaminate_player(added_contamination, false);
}

// Bad effects from magic contamination.
static void _magic_contamination_effects()
{
    mprf(MSGCH_WARN, "Your body shudders with the violent release "
                     "of wild energies!");

    const int contam = you.magic_contamination;

    // For particularly violent releases, make a little boom.
    if (contam > 10000 && coinflip())
    {
        bolt beam;

        beam.flavour      = BEAM_RANDOM;
        beam.glyph        = dchar_glyph(DCHAR_FIRED_BURST);
        beam.damage       = dice_def(3, div_rand_round(contam, 2000 ));
        beam.target       = you.pos();
        beam.name         = "magical storm";
        //XXX: Should this be MID_PLAYER?
        beam.source_id    = MID_NOBODY;
        beam.aux_source   = "a magical explosion";
        beam.ex_size      = max(1, min(9, div_rand_round(contam, 15000)));
        beam.ench_power   = div_rand_round(contam, 200);
        beam.is_explosion = true;

        // Undead enjoy extra contamination explosion damage because
        // the magical contamination has a harder time dissipating
        // through non-living flesh. :-)
        if (you.undead_state() != US_ALIVE)
            beam.damage.size *= 2;

        beam.explode();
    }

#if TAG_MAJOR_VERSION == 34
    const mutation_permanence_class mutclass = you.species == SP_DJINNI
        ? MUTCLASS_TEMPORARY
        : MUTCLASS_NORMAL;
#else
    const mutation_permanence_class mutclass = MUTCLASS_NORMAL;
#endif

    // We want to warp the player, not do good stuff!
    mutate(one_chance_in(5) ? RANDOM_MUTATION : RANDOM_BAD_MUTATION,
           "mutagenic glow", true, coinflip(), false, false, mutclass, false);

    // we're meaner now, what with explosions and whatnot, but
    // we dial down the contamination a little faster if its actually
    // mutating you.  -- GDL
    contaminate_player(-(random2(contam / 4) + 1000));
}
// Checks if the player should be hit with magic contaimination effects,
// then actually does it if they should be.
static void _handle_magic_contamination(int /*time_delta*/)
{
    // [ds] Move magic contamination effects closer to b26 again.
    const bool glow_effect = get_contamination_level() > 1
            && x_chance_in_y(you.magic_contamination, 12000);

    if (glow_effect)
    {
        if (is_sanctuary(you.pos()))
        {
            mprf(MSGCH_GOD, "Your body momentarily shudders from a surge of wild "
                            "energies until Zin's power calms it.");
        }
        else
            _magic_contamination_effects();
    }
}

// Adjust the player's stats if s/he's diseased (or recovering).
static void _recover_stats(int /*time_delta*/)
{
    if (!you.disease)
    {
        bool recovery = true;

        // The better-fed you are, the faster your stat recovery.
        if (you.species == SP_VAMPIRE)
        {
            if (you.hunger_state == HS_STARVING)
            {
                // No stat recovery for starving vampires.
                recovery = false;
            }
            else if (you.hunger_state <= HS_HUNGRY)
            {
                // Halved stat recovery for hungry vampires.
                recovery = coinflip();
            }
        }

        // Slow heal 3 mutation stops stat recovery.
        if (player_mutation_level(MUT_SLOW_HEALING) == 3)
            recovery = false;

        // Rate of recovery equals one level of MUT_DETERIORATION.
        if (recovery && x_chance_in_y(4, 200))
            restore_stat(STAT_RANDOM, 1, false, true);
    }
    else
    {
        // If Cheibriados has slowed your biology, disease might
        // not actually do anything.
        if (one_chance_in(30)
            && !(you_worship(GOD_CHEIBRIADOS)
                 && you.piety >= piety_breakpoint(0)
                 && coinflip()))
        {
            mprf(MSGCH_WARN, "Your disease is taking its toll.");
            lose_stat(STAT_RANDOM, 1, false, "disease");
        }
    }
}

// Adjust the player's stats if s/he has the deterioration mutation.
static void _deteriorate(int /*time_delta*/)
{
    if (player_mutation_level(MUT_DETERIORATION)
        && x_chance_in_y(player_mutation_level(MUT_DETERIORATION) * 5 - 1, 200))
    {
        lose_stat(STAT_RANDOM, 1, false, "deterioration mutation");
    }
}

// Exercise armour *xor* stealth skill: {dlb}
static void _wait_practice(int /*time_delta*/)
{
    practise(EX_WAIT);
}

static void _lab_change(int /*time_delta*/)
{
    if (player_in_branch(BRANCH_LABYRINTH))
        change_labyrinth();
}

// Update the abyss speed. This place is unstable and the speed can
// fluctuate. It's not a constant increase.
static void _abyss_speed(int /*time_delta*/)
{
    if (!player_in_branch(BRANCH_ABYSS))
        return;

    if (you_worship(GOD_CHEIBRIADOS) && coinflip())
        ; // Speed change less often for Chei.
    else if (coinflip() && you.abyss_speed < 100)
        ++you.abyss_speed;
    else if (one_chance_in(5) && you.abyss_speed > 0)
        --you.abyss_speed;
}

static void _jiyva_effects(int /*time_delta*/)
{
    if (!you_worship(GOD_JIYVA))
        return;

    if (one_chance_in(10))
    {
        int total_jellies = 1 + random2(5);
        bool success = false;
        for (int num_jellies = total_jellies; num_jellies > 0; num_jellies--)
        {
            // Spread jellies around the level.
            coord_def newpos;
            do
            {
                newpos = random_in_bounds();
            }
            while (grd(newpos) != DNGN_FLOOR
                       && grd(newpos) != DNGN_SHALLOW_WATER
                   || monster_at(newpos)
                   || env.cgrid(newpos) != EMPTY_CLOUD
                   || testbits(env.pgrid(newpos), FPROP_NO_JIYVA));

            mgen_data mg(MONS_JELLY, BEH_STRICT_NEUTRAL, 0, 0, 0, newpos,
                         MHITNOT, 0, GOD_JIYVA);
            mg.non_actor_summoner = "Jiyva";

            if (create_monster(mg))
                success = true;
        }

        if (success && !silenced(you.pos()))
        {
            switch (random2(3))
            {
                case 0:
                    simple_god_message(" gurgles merrily.");
                    break;
                case 1:
                    mprf(MSGCH_SOUND, "You hear %s splatter%s.",
                         total_jellies > 1 ? "a series of" : "a",
                         total_jellies > 1 ? "s" : "");
                    break;
                case 2:
                    simple_god_message(" says: Divide and consume!");
                    break;
            }
        }
    }

    if (x_chance_in_y(you.piety / 4, MAX_PIETY)
        && !player_under_penance() && one_chance_in(4))
    {
        jiyva_stat_action();
    }

    if (one_chance_in(25))
        jiyva_eat_offlevel_items();
}

static void _evolve(int time_delta)
{
    if (int lev = player_mutation_level(MUT_EVOLUTION))
        if (one_chance_in(2 / lev)
            && you.attribute[ATTR_EVOL_XP] * (1 + random2(10))
               > (int)exp_needed(you.experience_level + 1))
        {
            you.attribute[ATTR_EVOL_XP] = 0;
            mpr("You feel a genetic drift.");
            bool evol = one_chance_in(5) ?
                delete_mutation(RANDOM_BAD_MUTATION, "evolution", false) :
                mutate(coinflip() ? RANDOM_GOOD_MUTATION : RANDOM_MUTATION,
                       "evolution", false, false, false, false, MUTCLASS_NORMAL,
                       true);
            // it would kill itself anyway, but let's speed that up
            if (one_chance_in(10)
                && (!you.rmut_from_item()
                    || one_chance_in(10)))
            {
                evol |= delete_mutation(MUT_EVOLUTION, "end of evolution", false);
            }
            // interrupt the player only if something actually happened
            if (evol)
                more();
        }
}

// Get around C++ dividing integers towards 0.
static int _div(int num, int denom)
{
    div_t res = div(num, denom);
    return res.rem >= 0 ? res.quot : res.quot - 1;
}

struct timed_effect
{
    timed_effect_type effect;
    void              (*trigger)(int);
    int               min_time;
    int               max_time;
    bool              arena;
};

static struct timed_effect timed_effects[] =
{
    { TIMER_CORPSES,       rot_floor_items,               200,   200, true  },
    { TIMER_HELL_EFFECTS,  _hell_effects,                 200,   600, false },
    { TIMER_STAT_RECOVERY, _recover_stats,                100,   300, false },
    { TIMER_CONTAM,        _handle_magic_contamination,   200,   600, false },
    { TIMER_DETERIORATION, _deteriorate,                  100,   300, false },
    { TIMER_GOD_EFFECTS,   handle_god_time,               100,   300, false },
#if TAG_MAJOR_VERSION == 34
    { TIMER_SCREAM, nullptr,                                0,     0, false },
#endif
    { TIMER_FOOD_ROT,      rot_inventory_food,            100,   300, false },
    { TIMER_PRACTICE,      _wait_practice,                100,   300, false },
    { TIMER_LABYRINTH,     _lab_change,                  1000,  3000, false },
    { TIMER_ABYSS_SPEED,   _abyss_speed,                  100,   300, false },
    { TIMER_JIYVA,         _jiyva_effects,                100,   300, false },
    { TIMER_EVOLUTION,     _evolve,                      5000, 15000, false },
#if TAG_MAJOR_VERSION == 34
    { TIMER_BRIBE_TIMEOUT, nullptr,                         0,     0, false },
#endif
};

// Do various time related actions...
void handle_time()
{
    int base_time = you.elapsed_time % 200;
    int old_time = base_time - you.time_taken;

    // The checks below assume the function is called at least
    // once every 50 elapsed time units.

    // Every 5 turns, spawn random monsters, not in Zotdef.
    if (_div(base_time, 50) > _div(old_time, 50)
        && !crawl_state.game_is_zotdef())
    {
        spawn_random_monsters();
        if (player_in_branch(BRANCH_ABYSS))
          for (int i = 1; i < you.depth; ++i)
                if (x_chance_in_y(i, 5))
                    spawn_random_monsters();
    }

    // Labyrinth and Abyss maprot.
    if (player_in_branch(BRANCH_LABYRINTH) || player_in_branch(BRANCH_ABYSS))
        forget_map(true);

    // Magic contamination from spells and Orb.
    if (!crawl_state.game_is_arena())
        _handle_magic_contamination();

    for (unsigned int i = 0; i < ARRAYSZ(timed_effects); i++)
    {
        if (crawl_state.game_is_arena() && !timed_effects[i].arena)
            continue;

        if (!timed_effects[i].trigger)
        {
            if (you.next_timer_effect[i] < INT_MAX)
                you.next_timer_effect[i] = INT_MAX;
            continue;
        }

        if (you.elapsed_time >= you.next_timer_effect[i])
        {
            int time_delta = you.elapsed_time - you.last_timer_effect[i];
            (timed_effects[i].trigger)(time_delta);
            you.last_timer_effect[i] = you.next_timer_effect[i];
            you.next_timer_effect[i] =
                you.last_timer_effect[i]
                + random_range(timed_effects[i].min_time,
                               timed_effects[i].max_time);
        }
    }
}

/**
 * Return the number of turns it takes for monsters to forget about the player
 * 50% of the time.
 *
 * @param   The intelligence of the monster.
 * @return  An average number of turns before the monster forgets.
 */
static int _mon_forgetfulness_time(mon_intel_type intelligence)
{
    switch (intelligence)
    {
        case I_HIGH:
            return 1000;
        case I_NORMAL:
        default:
            return 500;
        case I_ANIMAL:
        case I_REPTILE:
        case I_INSECT:
            return 250;
        case I_PLANT:
            return 125;
    }
}

/**
 * Make monsters forget about the player after enough time passes off-level.
 *
 * @param mon           The monster in question.
 * @param mon_turns     Monster turns. (Turns * monster speed)
 * @return              Whether the monster forgot about the player.
 */
static bool _monster_forget(monster* mon, int mon_turns)
{
    // After x turns, half of the monsters will have forgotten about the
    // player. A given monster has a 95% chance of forgetting the player after
    // 4*x turns.
    const int forgetfulness_time = _mon_forgetfulness_time(mons_intel(mon));
    const int forget_chances = mon_turns / forgetfulness_time;
    // n.b. this is an integer division, so if range < forgetfulness_time
    // nothing happens

    if (bernoulli(forget_chances, 0.5))
    {
        mon->behaviour = BEH_WANDER;
        mon->foe = MHITNOT;
        mon->target = random_in_bounds();
        return true;
    }

    return false;
}

/**
 * Make ranged monsters flee from the player during their time offlevel.
 *
 * @param mon           The monster in question.
 */
static void _monster_flee(monster *mon)
{
    mon->behaviour = BEH_FLEE;
    dprf("backing off...");

    if (mon->pos() != mon->target)
        return;
    // If the monster is on the target square, fleeing won't work.

    if (in_bounds(env.old_player_pos) && env.old_player_pos != mon->pos())
    {
        // Flee from player's old position if different.
        mon->target = env.old_player_pos;
        return;
    }

    // Randomise the target so we have a direction to flee.
    coord_def mshift(random2(3) - 1, random2(3) - 1);

    // Bounds check: don't let fleeing monsters try to run off the grid.
    const coord_def s = mon->target + mshift;
    if (!in_bounds_x(s.x))
        mshift.x = 0;
    if (!in_bounds_y(s.y))
        mshift.y = 0;

    mon->target.x += mshift.x;
    mon->target.y += mshift.y;

    return;
}

/**
 * Make a monster take a number of moves toward (or away from, if fleeing)
 * their current target, very crudely.
 *
 * @param mon       The mon in question.
 * @param moves     The number of moves to take.
 */
static void _catchup_monster_move(monster* mon, int moves)
{
    coord_def pos(mon->pos());

    // Dirt simple movement.
    for (int i = 0; i < moves; ++i)
    {
        coord_def inc(mon->target - pos);
        inc = coord_def(sgn(inc.x), sgn(inc.y));

        if (mons_is_retreating(mon))
            inc *= -1;

        // Bounds check: don't let shifting monsters try to run off the
        // grid.
        const coord_def s = pos + inc;
        if (!in_bounds_x(s.x))
            inc.x = 0;
        if (!in_bounds_y(s.y))
            inc.y = 0;

        if (inc.origin())
            break;

        const coord_def next(pos + inc);
        const dungeon_feature_type feat = grd(next);
        if (feat_is_solid(feat)
            || monster_at(next)
            || !monster_habitable_grid(mon, feat))
        {
            break;
        }

        pos = next;
    }

    if (!mon->shift(pos))
        mon->shift(mon->pos());
}

/**
 * Move monsters around to fake them walking around while player was
 * off-level.
 *
 * Does not account for monster move speeds.
 *
 * Also make them forget about the player over time.
 *
 * @param mon       The monster under consideration
 * @param turns     The number of offlevel player turns to simulate.
 */
static void _catchup_monster_moves(monster* mon, int turns)
{
    // Summoned monsters might have disappeared.
    if (!mon->alive())
        return;

    // Expire friendly summons
    if (mon->friendly() && mon->is_summoned() && !mon->is_perm_summoned())
    {
        // You might still see them disappear if you were quick
        if (turns > 2)
            monster_die(mon, KILL_DISMISSED, NON_MONSTER);
        else
        {
            mon_enchant abj  = mon->get_ench(ENCH_ABJ);
            abj.duration = 0;
            mon->update_ench(abj);
        }
        return;
    }

    // Don't move non-land or stationary monsters around.
    if (mons_primary_habitat(mon) != HT_LAND
        || mons_is_zombified(mon)
           && mons_class_primary_habitat(mon->base_monster) != HT_LAND
        || mon->is_stationary())
    {
        return;
    }

    // Don't shift giant spores since that would disrupt their trail.
    if (mon->type == MONS_GIANT_SPORE)
        return;

    // special movement code for ioods, boulder beetles...
    if (mon->is_projectile())
    {
        iood_catchup(mon, turns);
        return;
    }

    // Let sleeping monsters lie.
    if (mon->asleep() || mon->paralysed())
        return;



    const int mon_turns = (turns * mon->speed) / 10;
    const int moves = min(mon_turns, 50);

    // probably too annoying even for DEBUG_DIAGNOSTICS
    dprf("mon #%d: range %d; "
         "pos (%d,%d); targ %d(%d,%d); flags %" PRIx64,
         mon->mindex(), mon_turns, mon->pos().x, mon->pos().y,
         mon->foe, mon->target.x, mon->target.y, mon->flags);

    if (mon_turns <= 0)
        return;


    // did the monster forget about the player?
    const bool forgot = _monster_forget(mon, mon_turns);

    // restore behaviour later if we start fleeing
    unwind_var<beh_type> saved_beh(mon->behaviour);

    if (!forgot && mons_has_ranged_attack(mon))
    {
        // If we're doing short time movement and the monster has a
        // ranged attack (missile or spell), then the monster will
        // flee to gain distance if it's "too close", else it will
        // just shift its position rather than charge the player. -- bwr
        if (grid_distance(mon->pos(), mon->target) >= 3)
        {
            mon->shift(mon->pos());
            dprf("shifted to (%d, %d)", mon->pos().x, mon->pos().y);
            return;
        }

        _monster_flee(mon);
    }

    _catchup_monster_move(mon, moves);

    dprf("moved to (%d, %d)", mon->pos().x, mon->pos().y);
}

//---------------------------------------------------------------
//
// update_level
//
// Update the level when the player returns to it.
//
//---------------------------------------------------------------
void update_level(int elapsedTime)
{
    ASSERT(!crawl_state.game_is_arena());

    // In ZotDef, no time passes while off-level.
    if (crawl_state.game_is_zotdef())
        return;

    const int turns = elapsedTime / 10;

#ifdef DEBUG_DIAGNOSTICS
    int mons_total = 0;

    dprf("turns: %d", turns);
#endif

    rot_floor_items(elapsedTime);
    shoals_apply_tides(turns, true, turns < 5);
    timeout_tombs(turns);
    recharge_rods(turns, true);

    if (env.sanctuary_time)
    {
        if (turns >= env.sanctuary_time)
            remove_sanctuary();
        else
            env.sanctuary_time -= turns;
    }

    dungeon_events.fire_event(
        dgn_event(DET_TURN_ELAPSED, coord_def(0, 0), turns * 10));

    for (monster_iterator mi; mi; ++mi)
    {
#ifdef DEBUG_DIAGNOSTICS
        mons_total++;
#endif

        // Pacified monsters often leave the level now.
        if (mi->pacified() && turns > random2(40) + 21)
        {
            make_mons_leave_level(*mi);
            continue;
        }

        // Following monsters don't get movement.
        if (mi->flags & MF_JUST_SUMMONED)
            continue;

        // XXX: Allow some spellcasting (like Healing and Teleport)? - bwr
        // const bool healthy = (mi->hit_points * 2 > mi->max_hit_points);

        mi->heal(div_rand_round(turns * mi->off_level_regen_rate(), 100));

        // Handle nets specially to remove the trapping property of the net.
        if (mi->caught())
            mi->del_ench(ENCH_HELD, true);

        _catchup_monster_moves(*mi, turns);

        mi->foe_memory = max(mi->foe_memory - turns, 0);

        if (turns >= 10 && mi->alive())
            mi->timeout_enchantments(turns / 10);
    }

#ifdef DEBUG_DIAGNOSTICS
    dprf("total monsters on level = %d", mons_total);
#endif

    for (int i = 0; i < MAX_CLOUDS; i++)
        delete_cloud(i);
}

static void _recharge_rod(item_def &rod, int aut, bool in_inv)
{
    if (rod.base_type != OBJ_RODS || rod.charges >= rod.charge_cap)
        return;

    // Skill calculations with a massive scale would overflow, cap it.
    // The worst case, a -3 rod, takes 17000 aut to fully charge.
    // -4 rods don't recharge at all.
    aut = min(aut, MAX_ROD_CHARGE * ROD_CHARGE_MULT * 10);

    int rate = 4 + rod.special;

    rate *= 10 * aut;
    if (in_inv)
        rate += skill_bump(SK_EVOCATIONS, aut);
    rate = div_rand_round(rate, 100);

    if (rate > rod.charge_cap - rod.charges) // Prevent overflow
        rate = rod.charge_cap - rod.charges;

    // With this, a +0 rod with no skill gets 1 mana per 25.0 turns

    if (rod.plus / ROD_CHARGE_MULT != (rod.plus + rate) / ROD_CHARGE_MULT)
    {
        if (item_equip_slot(rod) == EQ_WEAPON)
            you.wield_change = true;
        if (item_is_quivered(rod))
            you.redraw_quiver = true;
    }

    rod.plus += rate;

    if (in_inv && rod.charges == rod.charge_cap)
    {
        msg::stream << "Your " << rod.name(DESC_QUALNAME) << " has recharged."
                    << endl;
        if (is_resting())
            stop_running();
    }

    return;
}

void recharge_rods(int aut, bool level_only)
{
    if (!level_only)
    {
        for (int item = 0; item < ENDOFPACK; ++item)
            _recharge_rod(you.inv[item], aut, true);
    }

    for (int item = 0; item < MAX_ITEMS; ++item)
        _recharge_rod(mitm[item], aut, false);
}

