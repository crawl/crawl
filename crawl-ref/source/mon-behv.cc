/*
 *  File:       mon-behv.h
 *  Summary:    Monster behaviour functions.
 *  Written by: Linley Henzell
 */

#include "AppHdr.h"
#include "mon-behv.h"

#include "externs.h"
#include "options.h"

#include "coord.h"
#include "coordit.h"
#include "map_knowledge.h"
#include "fprop.h"
#include "exclude.h"
#include "los.h"
#include "monplace.h"
#include "monstuff.h"
#include "mon-util.h"
#include "random.h"
#include "state.h"
#include "terrain.h"
#include "traps.h"
#include "tutorial.h"
#include "view.h"

static void _set_nearest_monster_foe(monsters *monster);

// Check all grids in LoS and mark lava and/or water as seen if the
// appropriate grids are encountered, so we later only need to do the
// visibility check for monsters that can't pass a feature potentially in
// the way. We don't care about shallow water as most monsters can safely
// cross that, and fire elementals alone aren't really worth the extra
// hassle. :)
static void _check_lava_water_in_sight()
{
    you.lava_in_sight = you.water_in_sight = 0;
    for (radius_iterator ri(you.pos(), LOS_RADIUS); ri; ++ri)
    {
        const dungeon_feature_type feat = grd(*ri);
        if (feat == DNGN_LAVA)
        {
            you.lava_in_sight = 1;
            if (you.water_in_sight > 0)
                break;
        }
        else if (feat == DNGN_DEEP_WATER)
        {
            you.water_in_sight = 1;
            if (you.lava_in_sight > 0)
                break;
        }
    }
}

// If a monster can see but not directly reach the target, and then fails to
// find a path to get there, mark all surrounding (in a radius of 2) monsters
// of the same (or greater) movement restrictions as also being unable to
// find a path, so we won't need to calculate again.
// Should there be a direct path to the target for a monster thus marked, it
// will still be able to come nearer (and the mark will then be cleared).
static void _mark_neighbours_target_unreachable(monsters *mon)
{
    // Highly intelligent monsters are perfectly capable of pathfinding
    // and don't need their neighbour's advice.
    const mon_intel_type intel = mons_intel(mon);
    if (intel > I_NORMAL)
        return;

    const bool flies         = mons_flies(mon);
    const bool amphibious    = mons_amphibious(mon);
    const habitat_type habit = mons_primary_habitat(mon);

    for (radius_iterator ri(mon->pos(), 2, true, false); ri; ++ri)
    {
        if (*ri == mon->pos())
            continue;

        // Don't alert monsters out of sight (e.g. on the other side of
        // a wall).
        if (!mon->see_cell(*ri))
            continue;

        monsters* const m = monster_at(*ri);
        if (m == NULL)
            continue;

        // Don't restrict smarter monsters as they might find a path
        // a dumber monster wouldn't.
        if (mons_intel(m) > intel)
            continue;

        // Monsters of differing habitats might prefer different routes.
        if (mons_primary_habitat(m) != habit)
            continue;

        // A flying monster has an advantage over a non-flying one.
        if (!flies && mons_flies(m))
            continue;

        // Same for a swimming one, around water.
        if (you.water_in_sight > 0 && !amphibious && mons_amphibious(m))
            continue;

        if (m->travel_target == MTRAV_NONE)
            m->travel_target = MTRAV_UNREACHABLE;
    }
}

static void _set_no_path_found(monsters *mon)
{
#ifdef DEBUG_PATHFIND
    mpr("No path found!");
#endif

    mon->travel_target = MTRAV_UNREACHABLE;
    // Pass information on to nearby monsters.
    _mark_neighbours_target_unreachable(mon);
}

static bool _target_is_unreachable(monsters *mon)
{
    return (mon->travel_target == MTRAV_UNREACHABLE
            || mon->travel_target == MTRAV_KNOWN_UNREACHABLE);
}

//#define DEBUG_PATHFIND

// The monster is trying to get to the player (MHITYOU).
// Check whether there's an unobstructed path to the player (in sight!),
// either by using an existing travel_path or calculating a new one.
// Returns true if no further handling necessary, else false.
static bool _try_pathfind(monsters *mon, const dungeon_feature_type can_move,
                          bool potentially_blocking)
{
    // Just because we can *see* the player, that doesn't mean
    // we can actually get there. To find about that, we first
    // check for transparent walls. If there are transparent
    // walls in the way we'll need pathfinding, no matter what.
    // (Though monsters with a los attack don't need to get any
    // closer to hurt the player.)
    // If no walls are detected, there could still be a river
    // or a pool of lava in the way. So we check whether there
    // is water or lava in LoS (boolean) and if so, try to find
    // a way around it. It's possible that the player can see
    // lava but it actually has no influence on the monster's
    // movement (because it's lying in the opposite direction)
    // but if so, we'll find that out during path finding.
    // In another attempt of optimization, don't bother with
    // path finding if the monster in question has no trouble
    // travelling through water or flying across lava.
    // Also, if no path is found (too far away, perhaps) set a
    // flag, so we don't directly calculate the whole thing again
    // next turn, and even extend that flag to neighbouring
    // monsters of similar movement restrictions.

    // Smart monsters that can fire through walls won't use
    // pathfinding, and it's also not necessary if the monster
    // is already adjacent to you.
    if (potentially_blocking && mons_intel(mon) >= I_NORMAL
           && !mon->friendly() && mons_has_los_ability(mon->type)
        || grid_distance(mon->pos(), you.pos()) == 1)
    {
        potentially_blocking = false;
    }
    else
    {
        // If we don't already know whether there's water or lava
        // in LoS of the player, find out now.
        if (you.lava_in_sight == -1 || you.water_in_sight == -1)
            _check_lava_water_in_sight();

        // Flying monsters don't see water/lava as obstacle.
        // Also don't use pathfinding if the monster can shoot
        // across the blocking terrain, and is smart enough to
        // realise that.
        if (!potentially_blocking && !mons_flies(mon)
            && (mons_intel(mon) < I_NORMAL
                || mon->friendly()
                || (!mons_has_ranged_spell(mon, true)
                    && !mons_has_ranged_attack(mon))))
        {
            const habitat_type habit = mons_primary_habitat(mon);
            if (you.lava_in_sight > 0 && habit != HT_LAVA
                || you.water_in_sight > 0 && habit != HT_WATER
                   && can_move != DNGN_DEEP_WATER)
            {
                potentially_blocking = true;
            }
        }
    }

    if (!potentially_blocking
        || can_go_straight(mon->pos(), you.pos(), can_move))
    {
        // The player is easily reachable.
        // Clear travel path and target, if necessary.
        if (mon->travel_target != MTRAV_PATROL
            && mon->travel_target != MTRAV_NONE)
        {
            if (mon->is_travelling())
                mon->travel_path.clear();
            mon->travel_target = MTRAV_NONE;
        }
        return (false);
    }

    // Even if the target has been to "unreachable" (the monster already tried,
    // and failed, to find a path) there's a chance of trying again.
    if (!_target_is_unreachable(mon) || one_chance_in(12))
    {
#ifdef DEBUG_PATHFIND
        mprf("%s: Player out of reach! What now?",
             mon->name(DESC_PLAIN).c_str());
#endif
        // If we're already on our way, do nothing.
        if (mon->is_travelling() && mon->travel_target == MTRAV_PLAYER)
        {
            const int len = mon->travel_path.size();
            const coord_def targ = mon->travel_path[len - 1];

            // Current target still valid?
            if (can_go_straight(targ, you.pos(), can_move))
            {
                // Did we reach the target?
                if (mon->pos() == mon->travel_path[0])
                {
                    // Get next waypoint.
                    mon->travel_path.erase( mon->travel_path.begin() );

                    if (!mon->travel_path.empty())
                    {
                        mon->target = mon->travel_path[0];
                        return (true);
                    }
                }
                else if (can_go_straight(mon->pos(), mon->travel_path[0],
                                       can_move))
                {
                    mon->target = mon->travel_path[0];
                    return (true);
                }
            }
        }

        // Use pathfinding to find a (new) path to the player.
        const int dist = grid_distance(mon->pos(), you.pos());

#ifdef DEBUG_PATHFIND
        mprf("Need to calculate a path... (dist = %d)", dist);
#endif
        const int range = mons_tracking_range(mon);
        if (range > 0 && dist > range)
        {
            mon->travel_target = MTRAV_UNREACHABLE;
#ifdef DEBUG_PATHFIND
            mprf("Distance too great, don't attempt pathfinding! (%s)",
                 mon->name(DESC_PLAIN).c_str());
#endif
            return (false);
        }

#ifdef DEBUG_PATHFIND
        mprf("Need a path for %s from (%d, %d) to (%d, %d), max. dist = %d",
             mon->name(DESC_PLAIN).c_str(), mon->pos(), you.pos(), range);
#endif
        monster_pathfind mp;
        if (range > 0)
            mp.set_range(range);

        if (mp.init_pathfind(mon, you.pos()))
        {
            mon->travel_path = mp.calc_waypoints();
            if (!mon->travel_path.empty())
            {
                // Okay then, we found a path.  Let's use it!
                mon->target = mon->travel_path[0];
                mon->travel_target = MTRAV_PLAYER;
                return (true);
            }
            else
                _set_no_path_found(mon);
        }
        else
            _set_no_path_found(mon);
    }

    // We didn't find a path.
    return (false);
}

static bool _is_level_exit(const coord_def& pos)
{
    // All types of stairs.
    if (feat_is_stair(grd(pos)))
        return (true);

    // Teleportation and shaft traps.
    const trap_type tt = get_trap_type(pos);
    if (tt == TRAP_TELEPORT || tt == TRAP_SHAFT)
        return (true);

    return (false);
}

// Returns true if a monster left the level.
static bool _pacified_leave_level(monsters *mon, std::vector<level_exit> e,
                                  int e_index)
{
    // If a pacified monster is leaving the level, and has reached an
    // exit (whether that exit was its target or not), handle it here.
    // Likewise, if a pacified monster is far enough away from the
    // player, make it leave the level.
    if (_is_level_exit(mon->pos())
        || (e_index != -1 && mon->pos() == e[e_index].target)
        || grid_distance(mon->pos(), you.pos()) >= LOS_RADIUS * 4)
    {
        make_mons_leave_level(mon);
        return (true);
    }

    return (false);
}

// Counts deep water twice.
static int _count_water_neighbours(coord_def p)
{
    int water_count = 0;
    for (adjacent_iterator ai(p); ai; ++ai)
    {
        if (grd(*ai) == DNGN_SHALLOW_WATER)
            water_count++;
        else if (grd(*ai) == DNGN_DEEP_WATER)
            water_count += 2;
    }
    return (water_count);
}

// Pick the nearest water grid that is surrounded by the most
// water squares within LoS.
static bool _find_siren_water_target(monsters *mon)
{
    ASSERT(mon->type == MONS_SIREN);

    // Moving away could break the entrancement, so don't do this.
    if ((mon->pos() - you.pos()).rdist() >= 6)
        return (false);

    // Already completely surrounded by deep water.
    if (_count_water_neighbours(mon->pos()) >= 16)
        return (true);

    if (mon->travel_target == MTRAV_SIREN)
    {
        coord_def targ_pos(mon->travel_path[mon->travel_path.size() - 1]);
#ifdef DEBUG_PATHFIND
        mprf("siren target is (%d, %d), dist = %d",
             targ_pos.x, targ_pos.y, (int) (mon->pos() - targ_pos).rdist());
#endif
        if ((mon->pos() - targ_pos).rdist() > 2)
            return (true);
    }

    int best_water_count = 0;
    coord_def best_target;
    bool first = true;

    while (true)
    {
        int best_num = 0;
        for (radius_iterator ri(mon->pos(), LOS_RADIUS, true, false);
             ri; ++ri)
        {
            if (!feat_is_water(grd(*ri)))
                continue;

            // In the first iteration only count water grids that are
            // not closer to the player than to the siren.
            if (first && (mon->pos() - *ri).rdist() > (you.pos() - *ri).rdist())
                continue;

            // Counts deep water twice.
            const int water_count = _count_water_neighbours(*ri);
            if (water_count < best_water_count)
                continue;

            if (water_count > best_water_count)
            {
                best_water_count = water_count;
                best_target = *ri;
                best_num = 1;
            }
            else // water_count == best_water_count
            {
                const int old_dist = (mon->pos() - best_target).rdist();
                const int new_dist = (mon->pos() - *ri).rdist();
                if (new_dist > old_dist)
                    continue;

                if (new_dist < old_dist)
                {
                    best_target = *ri;
                    best_num = 1;
                }
                else if (one_chance_in(++best_num))
                    best_target = *ri;
            }
        }

        if (!first || best_water_count > 0)
            break;

        // Else start the second iteration.
        first = false;
    }

    if (!best_water_count)
        return (false);

    // We're already optimally placed.
    if (best_target == mon->pos())
        return (true);

    monster_pathfind mp;
#ifdef WIZARD
    // Remove old highlighted areas to make place for the new ones.
    for (rectangle_iterator ri(1); ri; ++ri)
        env.pgrid(*ri) &= ~(FPROP_HIGHLIGHT);
#endif

    if (mp.init_pathfind(mon, best_target))
    {
        mon->travel_path = mp.calc_waypoints();

        if (!mon->travel_path.empty())
        {
#ifdef WIZARD
            for (unsigned int i = 0; i < mon->travel_path.size(); i++)
                env.pgrid(mon->travel_path[i]) |= FPROP_HIGHLIGHT;
#endif
#ifdef DEBUG_PATHFIND
            mprf("Found a path to (%d, %d) with %d surrounding water squares",
                 best_target.x, best_target.y, best_water_count);
#endif
            // Okay then, we found a path.  Let's use it!
            mon->target = mon->travel_path[0];
            mon->travel_target = MTRAV_SIREN;
            return (true);
        }
    }

    return (false);
}

static bool _find_wall_target(monsters *mon)
{
    ASSERT(mons_wall_shielded(mon));

    if (mon->travel_target == MTRAV_WALL)
    {
        coord_def targ_pos(mon->travel_path[mon->travel_path.size() - 1]);

        // Target grid might have changed since we started, like if the
        // player destroys the wall the monster wants to hide in.
        if (cell_is_solid(targ_pos)
            && monster_habitable_grid(mon, grd(targ_pos)))
        {
            // Wall is still good.
#ifdef DEBUG_PATHFIND
            mprf("%s target is (%d, %d), dist = %d",
                 mon->name(DESC_PLAIN, true).c_str(),
                 targ_pos.x, targ_pos.y, (int) (mon->pos() - targ_pos).rdist());
#endif
            return (true);
        }

        mon->travel_path.clear();
        mon->travel_target = MTRAV_NONE;
    }

    int       best_dist             = INT_MAX;
    bool      best_closer_to_player = false;
    coord_def best_target;

    for (radius_iterator ri(mon->pos(), LOS_RADIUS, true, false);
         ri; ++ri)
    {
        if (!cell_is_solid(*ri)
            || !monster_habitable_grid(mon, grd(*ri)))
        {
            continue;
        }

        int  dist = (mon->pos() - *ri).rdist();
        bool closer_to_player = false;
        if (dist > (you.pos() - *ri).rdist())
            closer_to_player = true;

        if (dist < best_dist)
        {
            best_dist             = dist;
            best_closer_to_player = closer_to_player;
            best_target           = *ri;
        }
        else if (best_closer_to_player && !closer_to_player
                 && dist == best_dist)
        {
            best_closer_to_player = false;
            best_target           = *ri;
        }
    }

    if (best_dist == INT_MAX || !in_bounds(best_target))
        return (false);

    monster_pathfind mp;
#ifdef WIZARD
    // Remove old highlighted areas to make place for the new ones.
    for (rectangle_iterator ri(1); ri; ++ri)
        env.pgrid(*ri) &= ~(FPROP_HIGHLIGHT);
#endif

    if (mp.init_pathfind(mon, best_target))
    {
        mon->travel_path = mp.calc_waypoints();

        if (!mon->travel_path.empty())
        {
#ifdef WIZARD
            for (unsigned int i = 0; i < mon->travel_path.size(); i++)
                env.pgrid(mon->travel_path[i]) |= FPROP_HIGHLIGHT;
#endif
#ifdef DEBUG_PATHFIND
            mprf("Found a path to (%d, %d)", best_target.x, best_target.y);
#endif
            // Okay then, we found a path.  Let's use it!
            mon->target = mon->travel_path[0];
            mon->travel_target = MTRAV_WALL;
            return (true);
        }
    }
    return (false);
}

// Returns true if further handling neeeded.
static bool _handle_monster_travelling(monsters *mon,
                                       const dungeon_feature_type can_move)
{
#ifdef DEBUG_PATHFIND
    mprf("Monster %s reached target (%d, %d)",
         mon->name(DESC_PLAIN).c_str(), mon->target.x, mon->target.y);
#endif

    // Hey, we reached our first waypoint!
    if (mon->pos() == mon->travel_path[0])
    {
#ifdef DEBUG_PATHFIND
        mpr("Arrived at first waypoint.");
#endif
        mon->travel_path.erase( mon->travel_path.begin() );
        if (mon->travel_path.empty())
        {
#ifdef DEBUG_PATHFIND
            mpr("We reached the end of our path: stop travelling.");
#endif
            mon->travel_target = MTRAV_NONE;
            return (true);
        }
        else
        {
            mon->target = mon->travel_path[0];
#ifdef DEBUG_PATHFIND
            mprf("Next waypoint: (%d, %d)", mon->target.x, mon->target.y);
#endif
            return (false);
        }
    }

    // Can we still see our next waypoint?
    if (!can_go_straight(mon->pos(), mon->travel_path[0], can_move))
    {
#ifdef DEBUG_PATHFIND
        mpr("Can't see waypoint grid.");
#endif
        // Apparently we got sidetracked a bit.
        // Check the waypoints vector backwards and pick the first waypoint
        // we can see.

        // XXX: Note that this might still not be the best thing to do
        // since another path might be even *closer* to our actual target now.
        // Not by much, though, since the original path was optimal (A*) and
        // the distance between the waypoints is rather small.

        int erase = -1;  // Erase how many waypoints?
        const int size = mon->travel_path.size();
        for (int i = size - 1; i >= 0; --i)
        {
            if (can_go_straight(mon->pos(), mon->travel_path[i], can_move))
            {
                mon->target = mon->travel_path[i];
                erase = i;
                break;
            }
        }

        if (erase > 0)
        {
#ifdef DEBUG_PATHFIND
            mprf("Need to erase %d of %d waypoints.",
                 erase, size);
#endif
            // Erase all waypoints that came earlier:
            // we don't need them anymore.
            while (0 < erase--)
                mon->travel_path.erase( mon->travel_path.begin() );
        }
        else
        {
            // We can't reach our old path from our current
            // position, so calculate a new path instead.
            monster_pathfind mp;

            // The last coordinate in the path vector is our destination.
            const int len = mon->travel_path.size();
            if (mp.init_pathfind(mon, mon->travel_path[len-1]))
            {
                mon->travel_path = mp.calc_waypoints();
                if (!mon->travel_path.empty())
                {
                    mon->target = mon->travel_path[0];
#ifdef DEBUG_PATHFIND
                    mprf("Next waypoint: (%d, %d)",
                         mon->target.x, mon->target.y);
#endif
                }
                else
                {
                    mon->travel_target = MTRAV_NONE;
                    return (true);
                }
            }
            else
            {
                // Or just forget about the whole thing.
                mon->travel_path.clear();
                mon->travel_target = MTRAV_NONE;
                return (true);
            }
        }
    }

    // Else, we can see the next waypoint and are making good progress.
    // Carry on, then!
    return (false);
}

static bool _choose_random_patrol_target_grid(monsters *mon)
{
    const int intel = mons_intel(mon);

    // Zombies will occasionally just stand around.
    // This does not mean that they don't move every second turn. Rather,
    // once they reach their chosen target, there's a 50% chance they'll
    // just remain there until next turn when this function is called
    // again.
    if (intel == I_PLANT && coinflip())
        return (true);

    // If there's no chance we'll find the patrol point, quit right away.
    if (grid_distance(mon->pos(), mon->patrol_point) > 2 * LOS_RADIUS)
        return (false);

    // Can the monster see the patrol point from its current position?
    const bool patrol_seen = mon->mon_see_cell(mon->patrol_point,
                                 habitat2grid(mons_primary_habitat(mon)));

    if (intel == I_PLANT && !patrol_seen)
    {
        // Really stupid monsters won't even try to get back into the
        // patrol zone.
        return (false);
    }

    // While the patrol point is in easy reach, monsters of insect/plant
    // intelligence will only use a range of 5 (distance from the patrol point).
    // Otherwise, try to get back using the full LOS.
    const int  rad      = (intel >= I_ANIMAL || !patrol_seen) ? LOS_RADIUS : 5;
    const bool is_smart = (intel >= I_NORMAL);

    los_def patrol(mon->patrol_point, opacity_monmove(*mon),
                   circle_def(rad, C_ROUND));
    patrol.update();
    los_def lm(mon->pos(), opacity_monmove(*mon));
    if (is_smart || !patrol_seen)
    {
        // For stupid monsters, don't bother if the patrol point is in sight.
        lm.update();
    }

    int count_grids = 0;
    for (radius_iterator ri(mon->patrol_point, LOS_RADIUS, true, false);
         ri; ++ri)
    {
        // Don't bother for the current position. If everything fails,
        // we'll stay here anyway.
        if (*ri == mon->pos())
            continue;

        if (!mon->can_pass_through_feat(grd(*ri)))
            continue;

        // Don't bother moving to squares (currently) occupied by a
        // monster. We'll usually be able to find other target squares
        // (and if we're not, we couldn't move anyway), and this avoids
        // monsters trying to move onto a grid occupied by a plant or
        // sleeping monster.
        if (monster_at(*ri))
            continue;

        if (patrol_seen)
        {
            // If the patrol point can be easily (within LOS) reached
            // from the current position, it suffices if the target is
            // within reach of the patrol point OR the current position:
            // we can easily get there.
            // Only smart monsters will even attempt to move out of the
            // patrol area.
            // NOTE: Either of these can take us into a position where the
            // target cannot be easily reached (e.g. blocked by a wall)
            // and the patrol point is out of sight, too. Such a case
            // will be handled below, though it might take a while until
            // a monster gets out of a deadlock. (5% chance per turn.)
            if (!patrol.see_cell(*ri) &&
                (!is_smart || !lm.see_cell(*ri)))
            {
                continue;
            }
        }
        else
        {
            // If, however, the patrol point is out of reach, we have to
            // make sure the new target brings us into reach of it.
            // This means that the target must be reachable BOTH from
            // the patrol point AND the current position.
            if (!patrol.see_cell(*ri) ||
                !lm.see_cell(*ri))
            {
                continue;
            }

            // If this fails for all surrounding squares (probably because
            // we're too far away), we fall back to heading directly for
            // the patrol point.
        }

        bool set_target = false;
        if (intel == I_PLANT && *ri == mon->patrol_point)
        {
            // Slightly greater chance to simply head for the centre.
            count_grids += 3;
            if (x_chance_in_y(3, count_grids))
                set_target = true;
        }
        else if (one_chance_in(++count_grids))
            set_target = true;

        if (set_target)
            mon->target = *ri;
    }

    return (count_grids);
}// Returns true if further handling neeeded.
static bool _handle_monster_patrolling(monsters *mon)
{
    if (!_choose_random_patrol_target_grid(mon))
    {
        // If we couldn't find a target that is within easy reach
        // of the monster and close to the patrol point, depending
        // on monster intelligence, do one of the following:
        //  * set current position as new patrol point
        //  * forget about patrolling
        //  * head back to patrol point

        if (mons_intel(mon) == I_PLANT)
        {
            // Really stupid monsters forget where they're supposed to be.
            if (mon->friendly())
            {
                // Your ally was told to wait, and wait it will!
                // (Though possibly not where you told it to.)
                mon->patrol_point = mon->pos();
            }
            else
            {
                // Stop patrolling.
                mon->patrol_point.reset();
                mon->travel_target = MTRAV_NONE;
                return (true);
            }
        }
        else
        {
            // It's time to head back!
            // Other than for tracking the player, there's currently
            // no distinction between smart and stupid monsters when
            // it comes to travelling back to the patrol point. This
            // is in part due to the flavour of e.g. bees finding
            // their way back to the Hive (and patrolling should
            // really be restricted to cases like this), and for the
            // other part it's not all that important because we
            // calculate the path once and then follow it home, and
            // the player won't ever see the orderly fashion the
            // bees will trudge along.
            // What he will see is them swarming back to the Hive
            // entrance after some time, and that is what matters.
            monster_pathfind mp;
            if (mp.init_pathfind(mon, mon->patrol_point))
            {
                mon->travel_path = mp.calc_waypoints();
                if (!mon->travel_path.empty())
                {
                    mon->target = mon->travel_path[0];
                    mon->travel_target = MTRAV_PATROL;
                }
                else
                {
                    // We're so close we don't even need a path.
                    mon->target = mon->patrol_point;
                }
            }
            else
            {
                // Stop patrolling.
                mon->patrol_point.reset();
                mon->travel_target = MTRAV_NONE;
                return (true);
            }
        }
    }
    else
    {
#ifdef DEBUG_PATHFIND
        mprf("Monster %s (pp: %d, %d) is now patrolling to (%d, %d)",
             mon->name(DESC_PLAIN).c_str(),
             mon->patrol_point.x, mon->patrol_point.y,
             mon->target.x, mon->target.y);
#endif
    }

    return (false);
}

void set_random_target(monsters* mon)
{
    mon->target = random_in_bounds(); // If we don't find anything better.
    for (int tries = 0; tries < 150; ++tries)
    {
        coord_def delta = coord_def(random2(13), random2(13)) - coord_def(6, 6);
        if (delta.origin())
            continue;

        const coord_def newtarget = delta + mon->pos();
        if (!in_bounds(newtarget))
            continue;

        mon->target = newtarget;
        break;
    }
}

static void _check_wander_target(monsters *mon, bool isPacified = false,
                                 dungeon_feature_type can_move = DNGN_UNSEEN)
{
    // default wander behaviour
    if (mon->pos() == mon->target
        || mons_is_batty(mon) || !isPacified && one_chance_in(20))
    {
        bool need_target = true;

        if (!can_move)
        {
            can_move = (mons_amphibious(mon) ? DNGN_DEEP_WATER
                                             : DNGN_SHALLOW_WATER);
        }

        if (mon->is_travelling())
            need_target = _handle_monster_travelling(mon, can_move);

        // If we still need a target because we're not travelling
        // (any more), check for patrol routes instead.
        if (need_target && mon->is_patrolling())
            need_target = _handle_monster_patrolling(mon);

        // XXX: This is really dumb wander behaviour... instead of
        // changing the goal square every turn, better would be to
        // have the monster store a direction and have the monster
        // head in that direction for a while, then shift the
        // direction to the left or right.  We're changing this so
        // wandering monsters at least appear to have some sort of
        // attention span.  -- bwr
        if (need_target)
            set_random_target(mon);
    }
}

static void _arena_set_foe(monsters *mons)
{
    const int mind = monster_index(mons);

    int nearest = -1;
    int best_distance = -1;

    int nearest_unseen = -1;
    int best_unseen_distance = -1;
    for (int i = 0; i < MAX_MONSTERS; ++i)
    {
        if (mind == i)
            continue;

        const monsters *other(&menv[i]);
        if (!other->alive() || mons_aligned(mind, i))
            continue;

        // Don't fight test spawners, since they're only pseudo-monsters
        // placed to spawn real monsters, plus they're impossible to
        // kill.  But test spawners can fight each other, to give them a
        // target to spawn against.
        if (other->type == MONS_TEST_SPAWNER
            && mons->type != MONS_TEST_SPAWNER)
        {
            continue;
        }

        const int distance = grid_distance(mons->pos(), other->pos());
        const bool seen = mons->can_see(other);

        if (seen)
        {
            if (best_distance == -1 || distance < best_distance)
            {
                best_distance = distance;
                nearest = i;
            }
        }
        else
        {
            if (best_unseen_distance == -1 || distance < best_unseen_distance)
            {
                best_unseen_distance = distance;
                nearest_unseen = i;
            }
        }

        if ((best_distance == -1 || distance < best_distance)
            && mons->can_see(other))

        {
            best_distance = distance;
            nearest = i;
        }
    }

    if (nearest != -1)
    {
        mons->foe = nearest;
        mons->target = menv[nearest].pos();
        mons->behaviour = BEH_SEEK;
    }
    else if (nearest_unseen != -1)
    {
        mons->target = menv[nearest_unseen].pos();
        if (mons->type == MONS_TEST_SPAWNER)
        {
            mons->foe       = nearest_unseen;
            mons->behaviour = BEH_SEEK;
        }
        else
            mons->behaviour = BEH_WANDER;
    }
    else
    {
        mons->foe       = MHITNOT;
        mons->behaviour = BEH_WANDER;
    }
    if (mons->behaviour == BEH_WANDER)
        _check_wander_target(mons);

    ASSERT(mons->foe == MHITNOT || !mons->target.origin());
}

static void _find_all_level_exits(std::vector<level_exit> &e)
{
    e.clear();

    for (rectangle_iterator ri(1); ri; ++ri)
    {
        if (!in_bounds(*ri))
            continue;

        if (_is_level_exit(*ri))
            e.push_back(level_exit(*ri, false));
    }
}

static int _mons_find_nearest_level_exit(const monsters *mon,
                                         std::vector<level_exit> &e,
                                         bool reset = false)
{
    if (e.empty() || reset)
        _find_all_level_exits(e);

    int retval = -1;
    int old_dist = -1;

    for (unsigned int i = 0; i < e.size(); ++i)
    {
        if (e[i].unreachable)
            continue;

        int dist = grid_distance(mon->pos(), e[i].target);

        if (old_dist == -1 || old_dist >= dist)
        {
            // Ignore teleportation and shaft traps that the monster
            // shouldn't know about.
            if (!mons_is_native_in_branch(mon)
                && grd(e[i].target) == DNGN_UNDISCOVERED_TRAP)
            {
                continue;
            }

            retval = i;
            old_dist = dist;
        }
    }

    return (retval);
}

static void _set_random_slime_target(monsters* mon)
{
    // Strictly neutral slimes will go for the nearest item.
    int item_idx;
    coord_def orig_target = mon->target;

    for (radius_iterator ri(mon->pos(), LOS_RADIUS, true, false); ri; ++ri)
    {
        item_idx = igrd(*ri);
        if (item_idx != NON_ITEM)
        {
            for (stack_iterator si(*ri); si; ++si)
            {
                item_def& item(*si);

                if (is_item_jelly_edible(item))
                {
                    mon->target = *ri;
                    break;
                }
            }
        }
    }

    if (mon->target == mon->pos() || mon->target == you.pos())
        set_random_target(mon);
}

//---------------------------------------------------------------
//
// handle_behaviour
//
// 1. Evaluates current AI state
// 2. Sets monster target x,y based on current foe
//
// XXX: Monsters of I_NORMAL or above should select a new target
// if their current target is another monster which is sitting in
// a wall and is immune to most attacks while in a wall, unless
// the monster has a spell or special/nearby ability which isn't
// affected by the wall.
//---------------------------------------------------------------
void handle_behaviour(monsters *mon)
{
    bool changed = true;
    bool isFriendly = mon->friendly();
    bool isNeutral  = mon->neutral();
    bool wontAttack = mon->wont_attack();

    // Whether the player is in LOS of the monster and can see
    // or has guessed the player's location.
    bool proxPlayer = mons_near(mon) && !crawl_state.arena;

    bool trans_wall_block = trans_wall_blocking(mon->pos());

#ifdef WIZARD
    // If stealth is greater than actually possible (wizmode level)
    // pretend the player isn't there, but only for hostile monsters.
    if (proxPlayer && you.skills[SK_STEALTH] > 27 && !mon->wont_attack())
        proxPlayer = false;
#endif
    bool proxFoe;
    bool isHurt     = (mon->hit_points <= mon->max_hit_points / 4 - 1);
    bool isHealthy  = (mon->hit_points > mon->max_hit_points / 2);
    bool isSmart    = (mons_intel(mon) > I_ANIMAL);
    bool isScared   = mon->has_ench(ENCH_FEAR);
    bool isMobile   = !mons_is_stationary(mon);
    bool isPacified = mon->pacified();
    bool patrolling = mon->is_patrolling();
    static std::vector<level_exit> e;
    static int                     e_index = -1;

    // Check for confusion -- early out.
    if (mon->has_ench(ENCH_CONFUSION))
    {
        set_random_target(mon);
        return;
    }

    if (mons_is_fleeing_sanctuary(mon)
        && mons_is_fleeing(mon)
        && is_sanctuary(you.pos()))
    {
        return;
    }

    if (crawl_state.arena)
    {
        if (Options.arena_force_ai)
        {
            if (!mon->get_foe() || mon->target.origin() || one_chance_in(3))
                mon->foe = MHITNOT;
            if (mon->foe == MHITNOT || mon->foe == MHITYOU)
                _arena_set_foe(mon);
            return;
        }
        // If we're not forcing monsters to attack, just make sure they're
        // not targetting the player in arena mode.
        else if (mon->foe == MHITYOU)
            mon->foe = MHITNOT;
    }

    if (mons_wall_shielded(mon) && cell_is_solid(mon->pos()))
    {
        // Monster is safe, so its behaviour can be simplified to fleeing.
        if (mon->behaviour == BEH_CORNERED || mon->behaviour == BEH_PANIC
            || isScared)
        {
            mon->behaviour = BEH_FLEE;
        }
    }

    const dungeon_feature_type can_move =
        (mons_amphibious(mon)) ? DNGN_DEEP_WATER : DNGN_SHALLOW_WATER;

    // Validate current target exists.
    if (mon->foe != MHITNOT && mon->foe != MHITYOU)
    {
        const monsters& foe_monster = menv[mon->foe];
        if (!foe_monster.alive())
            mon->foe = MHITNOT;
        if (foe_monster.friendly() == isFriendly)
            mon->foe = MHITNOT;
    }

    // Change proxPlayer depending on invisibility and standing
    // in shallow water.
    if (proxPlayer && !you.visible_to(mon))
    {
        proxPlayer = false;

        const int intel = mons_intel(mon);
        // Sometimes, if a player is right next to a monster, they will 'see'.
        if (grid_distance(you.pos(), mon->pos()) == 1
            && one_chance_in(3))
        {
            proxPlayer = true;
        }

        // [dshaligram] Very smart monsters have a chance of clueing in to
        // invisible players in various ways.
        if (intel == I_NORMAL && one_chance_in(13)
                 || intel == I_HIGH && one_chance_in(6))
        {
            proxPlayer = true;
        }
    }

    // Set friendly target, if they don't already have one.
    // Berserking allies ignore your commands!
    if (isFriendly
        && you.pet_target != MHITNOT
        && (mon->foe == MHITNOT || mon->foe == MHITYOU)
        && !mon->berserk()
        && mon->mons_species() != MONS_GIANT_SPORE)
    {
        mon->foe = you.pet_target;
    }

    // Instead, berserkers attack nearest monsters.
    if ((mon->berserk() || mon->mons_species() == MONS_GIANT_SPORE)
        && (mon->foe == MHITNOT || isFriendly && mon->foe == MHITYOU))
    {
        // Intelligent monsters prefer to attack the player,
        // even when berserking.
        if (!isFriendly && proxPlayer && mons_intel(mon) >= I_NORMAL)
            mon->foe = MHITYOU;
        else
            _set_nearest_monster_foe(mon);
    }

    // Pacified monsters leaving the level prefer not to attack.
    // Others choose the nearest foe.
    if (!isPacified && mon->foe == MHITNOT)
        _set_nearest_monster_foe(mon);

    // Monsters do not attack themselves. {dlb}
    if (mon->foe == monster_index(mon))
        mon->foe = MHITNOT;

    // Friendly and good neutral monsters do not attack other friendly
    // and good neutral monsters.
    if (mon->foe != MHITNOT && mon->foe != MHITYOU
        && wontAttack && menv[mon->foe].wont_attack())
    {
        mon->foe = MHITNOT;
    }

    // Neutral monsters prefer not to attack players, or other neutrals.
    if (isNeutral && mon->foe != MHITNOT
        && (mon->foe == MHITYOU || menv[mon->foe].neutral()))
    {
        mon->foe = MHITNOT;
    }

    // Unfriendly monsters fighting other monsters will usually
    // target the player, if they're healthy.
    if (!isFriendly && !isNeutral
        && mon->foe != MHITYOU && mon->foe != MHITNOT
        && proxPlayer && !mon->berserk() && isHealthy
        && !one_chance_in(3))
    {
        mon->foe = MHITYOU;
    }

    // Validate current target again.
    if (mon->foe != MHITNOT && mon->foe != MHITYOU)
    {
        const monsters& foe_monster = menv[mon->foe];
        if (!foe_monster.alive())
            mon->foe = MHITNOT;
        if (foe_monster.friendly() == isFriendly)
            mon->foe = MHITNOT;
    }

    while (changed)
    {
        actor* afoe = mon->get_foe();
        proxFoe = afoe && mon->can_see(afoe);

        coord_def foepos = coord_def(0,0);
        if (afoe)
            foepos = afoe->pos();

        if (mon->foe == MHITYOU)
            proxFoe = proxPlayer;   // Take invis into account.

        // Track changes to state; attitude never changes here.
        beh_type new_beh       = mon->behaviour;
        unsigned short new_foe = mon->foe;

        // Take care of monster state changes.
        switch (mon->behaviour)
        {
        case BEH_SLEEP:
            // default sleep state
            mon->target = mon->pos();
            new_foe = MHITNOT;
            break;

        case BEH_LURK:
        case BEH_SEEK:
            // No foe? Then wander or seek the player.
            if (mon->foe == MHITNOT)
            {
                if (crawl_state.arena || !proxPlayer || isNeutral || patrolling
                    || mon->type == MONS_GIANT_SPORE)
                    new_beh = BEH_WANDER;
                else
                {
                    new_foe = MHITYOU;
                    mon->target = you.pos();
                }
                break;
            }

            // Foe gone out of LOS?
            if (!proxFoe)
            {
                if (mon->travel_target == MTRAV_SIREN)
                    mon->travel_target = MTRAV_NONE;

                if (mon->foe == MHITYOU && mon->is_travelling()
                    && mon->travel_target == MTRAV_PLAYER)
                {
                    // We've got a target, so we'll continue on our way.
#ifdef DEBUG_PATHFIND
                    mpr("Player out of LoS... start wandering.");
#endif
                    new_beh = BEH_WANDER;
                    break;
                }

                if (isFriendly)
                {
                    if (patrolling || crawl_state.arena)
                    {
                        new_foe = MHITNOT;
                        new_beh = BEH_WANDER;
                    }
                    else
                    {
                        new_foe = MHITYOU;
                        mon->target = foepos;
                    }
                    break;
                }

                ASSERT(mon->foe != MHITNOT);
                if (mon->foe_memory > 0)
                {
                    // If we've arrived at our target x,y
                    // do a stealth check.  If the foe
                    // fails, monster will then start
                    // tracking foe's CURRENT position,
                    // but only for a few moves (smell and
                    // intuition only go so far).

                    if (mon->pos() == mon->target)
                    {
                        if (mon->foe == MHITYOU)
                        {
                            if (one_chance_in(you.skills[SK_STEALTH]/3))
                                mon->target = you.pos();
                            else
                                mon->foe_memory = 0;
                        }
                        else
                        {
                            if (coinflip())     // XXX: cheesy!
                                mon->target = menv[mon->foe].pos();
                            else
                                mon->foe_memory = 0;
                        }
                    }

                    // Either keep chasing, or start wandering.
                    if (mon->foe_memory < 2)
                    {
                        mon->foe_memory = 0;
                        new_beh = BEH_WANDER;
                    }
                    break;
                }

                ASSERT(mon->foe_memory == 0);
                // Hack: smarter monsters will tend to pursue the player longer.
                switch (mons_intel(mon))
                {
                case I_HIGH:
                    mon->foe_memory = 100 + random2(200);
                    break;
                case I_NORMAL:
                    mon->foe_memory = 50 + random2(100);
                    break;
                case I_ANIMAL:
                case I_INSECT:
                    mon->foe_memory = 25 + random2(75);
                    break;
                case I_PLANT:
                    mon->foe_memory = 10 + random2(50);
                    break;
                }
                break;  // switch/case BEH_SEEK
            }

            ASSERT(proxFoe && mon->foe != MHITNOT);
            // Monster can see foe: continue 'tracking'
            // by updating target x,y.
            if (mon->foe == MHITYOU)
            {
                // The foe is the player.
                if (mon->type == MONS_SIREN
                    && you.beheld_by(mon)
                    && _find_siren_water_target(mon))
                {
                    break;
                }

                if (_try_pathfind(mon, can_move, trans_wall_block))
                    break;

                // Whew. If we arrived here, path finding didn't yield anything
                // (or wasn't even attempted) and we need to set our target
                // the traditional way.

                // Sometimes, your friends will wander a bit.
                if (isFriendly && one_chance_in(8))
                {
                    set_random_target(mon);
                    mon->foe = MHITNOT;
                    new_beh  = BEH_WANDER;
                }
                else
                {
                    mon->target = you.pos();
                }
            }
            else
            {
                // We have a foe but it's not the player.
                mon->target = menv[mon->foe].pos();
            }

            // Smart monsters, zombified monsters other than spectral
            // things, plants, and nonliving monsters cannot flee.
            if (isHurt && !isSmart && isMobile
                && (!mons_is_zombified(mon) || mon->type == MONS_SPECTRAL_THING)
                && mon->holiness() != MH_PLANT
                && mon->holiness() != MH_NONLIVING)
            {
                new_beh = BEH_FLEE;
            }
            break;

        case BEH_WANDER:
            if (isPacified)
            {
                // If a pacified monster isn't travelling toward
                // someplace from which it can leave the level, make it
                // start doing so.  If there's no such place, either
                // search the level for such a place again, or travel
                // randomly.
                if (mon->travel_target != MTRAV_PATROL)
                {
                    new_foe = MHITNOT;
                    mon->travel_path.clear();

                    e_index = _mons_find_nearest_level_exit(mon, e);

                    if (e_index == -1 || one_chance_in(20))
                        e_index = _mons_find_nearest_level_exit(mon, e, true);

                    if (e_index != -1)
                    {
                        mon->travel_target = MTRAV_PATROL;
                        patrolling = true;
                        mon->patrol_point = e[e_index].target;
                        mon->target = e[e_index].target;
                    }
                    else
                    {
                        mon->travel_target = MTRAV_NONE;
                        patrolling = false;
                        mon->patrol_point.reset();
                        set_random_target(mon);
                    }
                }

                if (_pacified_leave_level(mon, e, e_index))
                    return;
            }

            if (mon->strict_neutral() && mons_is_slime(mon)
                && you.religion == GOD_JIYVA)
            {
                _set_random_slime_target(mon);
            }

            // Is our foe in LOS?
            // Batty monsters don't automatically reseek so that
            // they'll flitter away, we'll reset them just before
            // they get movement in handle_monsters() instead. -- bwr
            if (proxFoe && !mons_is_batty(mon))
            {
                new_beh = BEH_SEEK;
                break;
            }

            _check_wander_target(mon, isPacified, can_move);

            // During their wanderings, monsters will eventually relax
            // their guard (stupid ones will do so faster, smart
            // monsters have longer memories).  Pacified monsters will
            // also eventually switch the place from which they want to
            // leave the level, in case their current choice is blocked.
            if (!proxFoe && mon->foe != MHITNOT
                   && one_chance_in(isSmart ? 60 : 20)
                || isPacified && one_chance_in(isSmart ? 40 : 120))
            {
                new_foe = MHITNOT;
                if (mon->is_travelling() && mon->travel_target != MTRAV_PATROL
                    || isPacified)
                {
#ifdef DEBUG_PATHFIND
                    mpr("It's been too long! Stop travelling.");
#endif
                    mon->travel_path.clear();
                    mon->travel_target = MTRAV_NONE;

                    if (isPacified && e_index != -1)
                        e[e_index].unreachable = true;
                }
            }
            break;

        case BEH_FLEE:
            // Check for healed.
            if (isHealthy && !isScared)
                new_beh = BEH_SEEK;

            // Smart monsters flee until they can flee no more...
            // possible to get a 'CORNERED' event, at which point
            // we can jump back to WANDER if the foe isn't present.

            if (isFriendly)
            {
                // Special-cased below so that it will flee *towards* you.
                if (mon->foe == MHITYOU)
                    mon->target = you.pos();
            }
            else if (mons_wall_shielded(mon) && _find_wall_target(mon))
                ; // Wall target found.
            else if (proxFoe)
            {
                // Special-cased below so that it will flee *from* the
                // correct position.
                mon->target = foepos;
            }
            break;

        case BEH_CORNERED:
            // Plants and nonliving monsters cannot fight back.
            if (mon->holiness() == MH_PLANT
                || mon->holiness() == MH_NONLIVING)
            {
                break;
            }

            if (isHealthy)
                new_beh = BEH_SEEK;

            // Foe gone out of LOS?
            if (!proxFoe)
            {
                if ((isFriendly || proxPlayer) && !isNeutral && !patrolling)
                    new_foe = MHITYOU;
                else
                    new_beh = BEH_WANDER;
            }
            else
            {
                mon->target = foepos;
            }
            break;

        default:
            return;     // uh oh
        }

        changed = (new_beh != mon->behaviour || new_foe != mon->foe);
        mon->behaviour = new_beh;

        if (mon->foe != new_foe)
            mon->foe_memory = 0;

        mon->foe = new_foe;
    }

    if (mon->travel_target == MTRAV_WALL && cell_is_solid(mon->pos()))
    {
        if (mon->behaviour == BEH_FLEE)
        {
            // Monster is safe, so stay put.
            mon->target = mon->pos();
            mon->foe = MHITNOT;
        }
    }
}

static bool _mons_check_foe(monsters *mon, const coord_def& p,
                            bool friendly, bool neutral)
{
    if (!inside_level_bounds(p))
        return (false);

    if (!friendly && !neutral && p == you.pos()
        && you.visible_to(mon) && !is_sanctuary(p))
    {
        return (true);
    }

    if (monsters *foe = monster_at(p))
    {
        if (foe != mon
            && mon->can_see(foe)
            && (friendly || !is_sanctuary(p))
            && (foe->friendly() != friendly
                || (neutral && !foe->neutral())))
        {
            return (true);
        }
    }
    return (false);
}

// Choose random nearest monster as a foe.
void _set_nearest_monster_foe(monsters *mon)
{
    const bool friendly = mon->friendly();
    const bool neutral  = mon->neutral();

    for (int k = 1; k <= LOS_RADIUS; ++k)
    {
        std::vector<coord_def> monster_pos;
        for (int i = -k; i <= k; ++i)
            for (int j = -k; j <= k; (abs(i) == k ? j++ : j += 2*k))
            {
                const coord_def p = mon->pos() + coord_def(i, j);
                if (_mons_check_foe(mon, p, friendly, neutral))
                    monster_pos.push_back(p);
            }
        if (monster_pos.empty())
            continue;

        const coord_def mpos = monster_pos[random2(monster_pos.size())];
        if (mpos == you.pos())
            mon->foe = MHITYOU;
        else
            mon->foe = env.mgrid(mpos);
        return;
    }
}

//-----------------------------------------------------------------
//
// behaviour_event
//
// 1. Change any of: monster state, foe, and attitude
// 2. Call handle_behaviour to re-evaluate AI state and target x, y
//
//-----------------------------------------------------------------
void behaviour_event(monsters *mon, mon_event_type event, int src,
                     coord_def src_pos, bool allow_shout)
{
    ASSERT(src >= 0 && src <= MHITYOU);
    ASSERT(!crawl_state.arena || src != MHITYOU);
    ASSERT(in_bounds(src_pos) || src_pos.origin());

    const beh_type old_behaviour = mon->behaviour;

    bool isSmart          = (mons_intel(mon) > I_ANIMAL);
    bool wontAttack       = mon->wont_attack();
    bool sourceWontAttack = false;
    bool setTarget        = false;
    bool breakCharm       = false;
    bool was_sleeping     = mon->asleep();

    if (src == MHITYOU)
        sourceWontAttack = true;
    else if (src != MHITNOT)
        sourceWontAttack = menv[src].wont_attack();

    if (is_sanctuary(mon->pos()) && mons_is_fleeing_sanctuary(mon))
    {
        mon->behaviour = BEH_FLEE;
        mon->foe       = MHITYOU;
        mon->target    = env.sanctuary_pos;
        return;
    }

    switch (event)
    {
    case ME_DISTURB:
        // Assumes disturbed by noise...
        if (mon->asleep())
        {
            mon->behaviour = BEH_WANDER;

            if (mons_near(mon))
                remove_auto_exclude(mon, true);
        }

        // A bit of code to make Projected Noise actually do
        // something again.  Basically, dumb monsters and
        // monsters who aren't otherwise occupied will at
        // least consider the (apparent) source of the noise
        // interesting for a moment. -- bwr
        if (!isSmart || mon->foe == MHITNOT || mons_is_wandering(mon))
        {
            if (mon->is_patrolling())
                break;

            ASSERT(!src_pos.origin());
            mon->target = src_pos;
        }
        break;

    case ME_WHACK:
    case ME_ANNOY:
        // Will turn monster against <src>, unless they
        // are BOTH friendly or good neutral AND stupid,
        // or else fleeing anyway.  Hitting someone over
        // the head, of course, always triggers this code.
        if (event == ME_WHACK
            || ((wontAttack != sourceWontAttack || isSmart)
                && !mons_is_fleeing(mon) && !mons_is_panicking(mon)))
        {
            // Monster types that you can't gain experience from cannot
            // fight back, so don't bother having them do so.  If you
            // worship Fedhas, create a ring of friendly plants, and try
            // to break out of the ring by killing a plant, you'll get
            // a warning prompt and penance only once.  Without the
            // hostility check, the plant will remain friendly until it
            // dies, and you'll get a warning prompt and penance once
            // *per hit*.  This may not be the best way to address the
            // issue, though. -cao
            if (mons_class_flag(mon->type, M_NO_EXP_GAIN)
                && mon->attitude != ATT_FRIENDLY
                && mon->attitude != ATT_GOOD_NEUTRAL)
            {
                return;
            }

            mon->foe = src;

            if (mon->asleep() && mons_near(mon))
                remove_auto_exclude(mon, true);

            if (!mons_is_cornered(mon))
                mon->behaviour = BEH_SEEK;

            if (src == MHITYOU)
            {
                mon->attitude = ATT_HOSTILE;
                breakCharm    = true;
            }
        }

        // Now set target so that monster can whack back (once) at an
        // invisible foe.
        if (event == ME_WHACK)
            setTarget = true;
        break;

    case ME_ALERT:
        // Allow monsters falling asleep while patrolling (can happen if
        // they're left alone for a long time) to be woken by this event.
        if (mon->friendly() && mon->is_patrolling()
            && !mon->asleep())
        {
            break;
        }

        // Avoid moving friendly giant spores out of BEH_WANDER
        if (mon->friendly() && mon->type == MONS_GIANT_SPORE)
            break;

        if (mon->asleep() && mons_near(mon))
            remove_auto_exclude(mon, true);

        // Will alert monster to <src> and turn them
        // against them, unless they have a current foe.
        // It won't turn friends hostile either.
        if (!mons_is_fleeing(mon) && !mons_is_panicking(mon)
            && !mons_is_cornered(mon))
        {
            mon->behaviour = BEH_SEEK;
        }

        if (mon->foe == MHITNOT)
            mon->foe = src;

        if (!src_pos.origin()
            && (mon->foe == MHITNOT || mon->foe == src
                || mons_is_wandering(mon)))
        {
            if (mon->is_patrolling())
                break;

            mon->target = src_pos;

            // XXX: Should this be done in _handle_behaviour()?
            if (src == MHITYOU && src_pos == you.pos()
                && !you.see_cell(mon->pos()))
            {
                const dungeon_feature_type can_move =
                    (mons_amphibious(mon)) ? DNGN_DEEP_WATER
                                           : DNGN_SHALLOW_WATER;

                _try_pathfind(mon, can_move, true);
            }
        }
        break;

    case ME_SCARE:
        // Stationary monsters can't flee, and berserking monsters
        // are too enraged.
        if (mons_is_stationary(mon) || mon->berserk())
        {
            mon->del_ench(ENCH_FEAR, true, true);
            break;
        }

        // Neither do plants or nonliving beings.
        if (mon->holiness() == MH_PLANT
            || mon->holiness() == MH_NONLIVING)
        {
            mon->del_ench(ENCH_FEAR, true, true);
            break;
        }

        // Assume monsters know where to run from, even if player is
        // invisible.
        mon->behaviour = BEH_FLEE;
        mon->foe       = src;
        mon->target    = src_pos;
        if (src == MHITYOU)
        {
            // Friendly monsters don't become hostile if you read a
            // scroll of fear, but enslaved ones will.
            // Send friendlies off to a random target so they don't cling
            // to you in fear.
            if (mon->friendly())
            {
                breakCharm = true;
                mon->foe   = MHITNOT;
                set_random_target(mon);
            }
            else
                setTarget = true;
        }
        else if (mon->friendly() && !crawl_state.arena)
            mon->foe = MHITYOU;

        if (observe_cell(mon->pos()))
            learned_something_new(TUT_FLEEING_MONSTER);
        break;

    case ME_CORNERED:
        // Some monsters can't flee.
        if (mon->behaviour != BEH_FLEE && !mon->has_ench(ENCH_FEAR))
            break;

        // Pacified monsters shouldn't change their behaviour.
        if (mon->pacified())
            break;

        // Just set behaviour... foe doesn't change.
        if (!mons_is_cornered(mon))
        {
            if (mon->friendly() && !crawl_state.arena)
            {
                mon->foe = MHITYOU;
                simple_monster_message(mon, " returns to your side!");
            }
            else
                simple_monster_message(mon, " turns to fight!");
        }

        mon->behaviour = BEH_CORNERED;
        break;

    case ME_EVAL:
        break;
    }

    if (setTarget)
    {
        if (src == MHITYOU)
        {
            mon->target = you.pos();
            mon->attitude = ATT_HOSTILE;
        }
        else if (src != MHITNOT)
            mon->target = src_pos;
    }

    // Now, break charms if appropriate.
    if (breakCharm)
        mon->del_ench(ENCH_CHARM);

    // Do any resultant foe or state changes.
    handle_behaviour(mon);
    ASSERT(in_bounds(mon->target) || mon->target.origin());

    // If it woke up and you're its new foe, it might shout.
    if (was_sleeping && !mon->asleep() && allow_shout
        && mon->foe == MHITYOU && !mon->wont_attack())
    {
        handle_monster_shouts(mon);
    }

    const bool wasLurking =
        (old_behaviour == BEH_LURK && !mons_is_lurking(mon));
    const bool isPacified = mon->pacified();

    if ((wasLurking || isPacified)
        && (event == ME_DISTURB || event == ME_ALERT || event == ME_EVAL))
    {
        // Lurking monsters or pacified monsters leaving the level won't
        // stop doing so just because they noticed something.
        mon->behaviour = old_behaviour;
    }
    else if (wasLurking && mon->has_ench(ENCH_SUBMERGED)
             && !mon->del_ench(ENCH_SUBMERGED))
    {
        // The same goes for lurking submerged monsters, if they can't
        // unsubmerge.
        mon->behaviour = BEH_LURK;
    }

    ASSERT(!crawl_state.arena
           || mon->foe != MHITYOU && mon->target != you.pos());
}

void make_mons_stop_fleeing(monsters *mon)
{
    if (mons_is_fleeing(mon))
        behaviour_event(mon, ME_CORNERED);
}
