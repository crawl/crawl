#include "AppHdr.h"

#include "mon-movetarget.h"

#include "act-iter.h"
#include "branch.h"
#include "coord.h"
#include "coordit.h"
#include "env.h"
#include "fprop.h"
#include "itemprop.h"
#include "items.h"
#include "libutil.h"
#include "los_def.h"
#include "losglobal.h"
#include "mon-behv.h"
#include "mon-pathfind.h"
#include "mon-place.h"
#include "state.h"
#include "terrain.h"
#include "traps.h"

// If a monster can see but not directly reach the target, and then fails to
// find a path to get there, mark all surrounding (in a radius of 2) monsters
// of the same (or greater) movement restrictions as also being unable to
// find a path, so we won't need to calculate again.
// Should there be a direct path to the target for a monster thus marked, it
// will still be able to come nearer (and the mark will then be cleared).
static void _mark_neighbours_target_unreachable(monster* mon)
{
    const mon_intel_type intel = mons_intel(*mon);
    const bool flies         = mon->airborne();
    const bool amphibious    = (mons_habitat(*mon) == HT_AMPHIBIOUS);
    const bool amph_lava     = (mons_habitat(*mon) == HT_AMPHIBIOUS_LAVA);
    const habitat_type habit = mons_primary_habitat(*mon);

    for (radius_iterator ri(mon->pos(), 2, C_SQUARE); ri; ++ri)
    {
        if (*ri == mon->pos())
            continue;

        // Don't alert monsters out of sight (e.g. on the other side of
        // a wall).
        if (!mon->see_cell(*ri))
            continue;

        monster* const m = monster_at(*ri);
        if (m == nullptr)
            continue;

        // Don't restrict smarter monsters as they might find a path
        // a dumber monster wouldn't.
        if (mons_intel(*m) > intel)
            continue;

        // Monsters of differing habitats might prefer different routes.
        if (mons_primary_habitat(*m) != habit)
            continue;

        // Wall clinging monsters use different pathfinding.
        if (mon->can_cling_to_walls() != m->can_cling_to_walls())
            continue;

        // A flying monster has an advantage over a non-flying one.
        // Same for a swimming one.
        if (!flies && m->airborne()
            || !amphibious && mons_habitat(*m) == HT_AMPHIBIOUS
            || !amph_lava  && mons_habitat(*m) == HT_AMPHIBIOUS_LAVA)
        {
            continue;
        }

        // If the monster is trying to reach the same foe, consider their
        // foe also unreachable.
        if (m->travel_target == MTRAV_NONE && m->foe == mon->foe)
            m->travel_target = MTRAV_UNREACHABLE;
    }
}

static void _set_no_path_found(monster* mon)
{
#ifdef DEBUG_PATHFIND
    mpr("No path found!");
#endif

    mon->travel_target = MTRAV_UNREACHABLE;
    // Pass information on to nearby monsters.
    _mark_neighbours_target_unreachable(mon);
}

bool target_is_unreachable(monster* mon)
{
    return mon->travel_target == MTRAV_UNREACHABLE
           || mon->travel_target == MTRAV_KNOWN_UNREACHABLE;
}

//#define DEBUG_PATHFIND

// Check whether there's an unobstructed path to our foe,
// either by using an existing travel_path or calculating a new one.
// Returns true if no further handling necessary, else false.
bool try_pathfind(monster* mon)
{
    // Just because we can *see* our target, that doesn't mean
    // we can actually get there.
    // If no path is found (too far away, perhaps) set a
    // flag, so we don't directly calculate the whole thing again
    // next turn, and even extend that flag to neighbouring
    // monsters of similar movement restrictions.

    const actor* foe = (mon->friendly() && mon->foe == MHITYOU ? &you
                                                               : mon->get_foe());

    // Trying to pathfind towards nothing in particular; bail out.
    if (!foe)
        return false;

    const coord_def targpos = foe->pos();

    bool need_pathfind = !can_go_straight(mon, mon->pos(), targpos);

    // Smart monsters that can fire through obstacles won't use
    // pathfinding.
    if (need_pathfind
        && !mon->friendly()
        && mon->can_see(*foe)
        && mons_has_los_ability(mon->type))
    {
        need_pathfind = false;
    }

    // Also don't use pathfinding if the monster can shoot
    // across the blocking terrain, and is smart enough to
    // realise that.
    if (need_pathfind
        && !mon->friendly()
        && mons_has_ranged_attack(*mon)
        && cell_see_cell(mon->pos(), targpos, LOS_SOLID_SEE))
    {
        need_pathfind = false;
    }

    if (!need_pathfind)
    {
        // The target is easily reachable.
        // Clear travel path and target, if necessary.
        if (mon->travel_target != MTRAV_PATROL
            && mon->travel_target != MTRAV_NONE)
        {
            if (mon->is_travelling())
                mon->travel_path.clear();
            mon->travel_target = MTRAV_NONE;
        }
        return false;
    }

    // If the target is "unreachable" (the monster already tried,
    // and failed, to find a path), there's a chance of trying again.
    // The chance is higher for wall clinging monsters to help them avoid
    // shallow water. Retreating monsters retry every turn.
    if (target_is_unreachable(mon) && !one_chance_in(12)
        && !(mon->can_cling_to_walls() && one_chance_in(4)))
    {
        return false;
    }

#ifdef DEBUG_PATHFIND
    mprf("%s: Target out of reach! What now?",
         mon->name(DESC_PLAIN).c_str());
#endif
    // If we're already on our way, do nothing.
    if (mon->is_travelling() && mon->travel_target == MTRAV_FOE)
    {
        const int len = mon->travel_path.size();
        const coord_def targ = mon->travel_path[len - 1];

        // Current target still valid?
        if (can_go_straight(mon, targ, targpos))
        {
            // Did we reach the target?
            if (mon->pos() == mon->travel_path[0])
            {
                // Get next waypoint.
                mon->travel_path.erase(mon->travel_path.begin());

                if (!mon->travel_path.empty())
                {
                    mon->target = mon->travel_path[0];
                    return true;
                }
            }
            else if (can_go_straight(mon, mon->pos(), mon->travel_path[0]))
            {
                mon->target = mon->travel_path[0];
                return true;
            }
        }
    }

    // Use pathfinding to find a (new) path to the target.
    const int dist = grid_distance(mon->pos(), targpos);

#ifdef DEBUG_PATHFIND
    mprf("Need to calculate a path... (dist = %d)", dist);
#endif
    const int range = mon->friendly() ? 1000 : mons_tracking_range(mon);

    if (range > 0 && dist > range)
    {
        mon->travel_target = MTRAV_UNREACHABLE;
#ifdef DEBUG_PATHFIND
        mprf("Distance too great, don't attempt pathfinding! (%s)",
             mon->name(DESC_PLAIN).c_str());
#endif
        return false;
    }

#ifdef DEBUG_PATHFIND
    mprf("Need a path for %s from (%d, %d) to (%d, %d), max. dist = %d",
         mon->name(DESC_PLAIN).c_str(), mon->pos().x, mon->pos().y,
         targpos.x, targpos.y, range);
#endif
    monster_pathfind mp;
    if (range > 0)
        mp.set_range(range);

    if (mp.init_pathfind(mon, targpos))
    {
        mon->travel_path = mp.calc_waypoints();
        if (!mon->travel_path.empty())
        {
            // Okay then, we found a path. Let's use it!
            mon->target = mon->travel_path[0];
            mon->travel_target = MTRAV_FOE;
            return true;
        }
    }

    // We didn't find a path.
    _set_no_path_found(mon);
    return false;
}

static bool _is_level_exit(const coord_def& pos)
{
    // All types of stairs.
    if (feat_is_stair(grd(pos)))
        return true;

    // Teleportation and shaft traps.
    const trap_type tt = get_trap_type(pos);
    if (tt == TRAP_TELEPORT || tt == TRAP_TELEPORT_PERMANENT
        || tt == TRAP_SHAFT)
    {
        return true;
    }

    return false;
}

// Returns true if a monster left the level.
bool pacified_leave_level(monster* mon, vector<level_exit> e, int e_index)
{
    // If a pacified monster is leaving the level, and has reached an
    // exit (whether that exit was its target or not), handle it here.
    // Likewise, if a pacified monster is far enough away from the
    // player, make it leave the level.
    if (_is_level_exit(mon->pos())
        || (e_index != -1 && mon->pos() == e[e_index].target)
        || grid_distance(mon->pos(), you.pos()) >= LOS_RADIUS * 3)
    {
        make_mons_leave_level(mon);
        return true;
    }

    return false;
}

// Assesses how desirable a spot is to a merfolk avatar (preferring spaces
// surrounded by water, at least one of which is deep, and with at least one
// neighbour which is inhabitable without swimming or flight)
static int _merfolk_avatar_water_score(coord_def p, bool& deep)
{
    int score = 0;
    bool near_floor = false;
    deep = false;

    for (adjacent_iterator ai(p); ai; ++ai)
    {
        if (grd(*ai) == DNGN_SHALLOW_WATER)
        {
            score++;
            near_floor = true;
        }
        else if (grd(*ai) == DNGN_DEEP_WATER)
        {
            score++;
            deep = true;
        }
        else if (feat_has_solid_floor(grd(*ai)))
            near_floor = true;
    }

    // Don't prefer any locations non-adjacent to either shallow water or land
    if (!near_floor)
        return 0;

    // Greatly prefer at least one tile of neighbouring deep water
    if (deep)
        score += 6;

    // Slightly prefer standing in deep water, if possible
    if (grd(p) == DNGN_DEEP_WATER)
        score++;

    return score;
}

// Pick the nearest water grid that is surrounded by the most
// water squares within LoS.
bool find_merfolk_avatar_water_target(monster* mon)
{
    ASSERT(mon->type == MONS_MERFOLK_AVATAR);

    // Moving away could break the entrancement, so don't do this.
    if (grid_distance(mon->pos(), you.pos()) >= 5)
    {
        mon->firing_pos.reset();
        return false;
    }

    bool deep;

    // If our current location is good enough, don't bother moving towards
    // some other spot which might be somewhat better
    if (_merfolk_avatar_water_score(mon->pos(), deep) >= 12 && deep
        && grd(mon->pos()) == DNGN_DEEP_WATER)
    {
        mon->firing_pos = mon->pos();
        return true;
    }

    if (mon->travel_target == MTRAV_MERFOLK_AVATAR)
    {
        coord_def targ_pos(mon->travel_path[mon->travel_path.size() - 1]);
#ifdef DEBUG_PATHFIND
        mprf("merfolk avatar target is (%d, %d), dist = %d",
             targ_pos.x, targ_pos.y, (int) (mon->pos() - targ_pos).rdist());
#endif
        if ((mon->pos() - targ_pos).rdist() > 2)
            return true;
    }

    int best_water_count = 0;
    coord_def best_target;

    deep = false;

    // Try two iterations, the second more relaxed than the first.  But if
    // we find deep water on the first pass, don't bother with the second.
    for (int iteration = 0; iteration < 2 && !deep; ++iteration)
    {
        int best_num = 0;
        for (radius_iterator ri(mon->pos(), LOS_NO_TRANS); ri; ++ri)
        {
            if (!feat_is_water(grd(*ri)))
                continue;

            const int dist = grid_distance(mon->pos(), *ri);

            // In the first iteration only count water grids that are
            // not closer to the player than to the merfolk avatar.
            if (iteration == 0 && dist > grid_distance(you.pos(), *ri))
                continue;

            // Counts deep water twice.
            const int water_count = _merfolk_avatar_water_score(*ri, deep);
            if (water_count > best_water_count)
            {
                best_water_count = water_count;
                best_target = *ri;
                best_num = 1;
            }
            else if (water_count == best_water_count)
            {
                const int old_dist = best_target.origin() ? INFINITE_DISTANCE
                                   : grid_distance(mon->pos(), best_target);
                if (dist < old_dist)
                {
                    best_target = *ri;
                    best_num = 1;
                }
                else if (dist == old_dist && one_chance_in(++best_num))
                    best_target = *ri;
            }
        }
    }

    if (!best_water_count)
        return false;

    // We're already optimally placed.
    if (best_target == mon->pos())
    {
        mon->firing_pos = mon->pos();
        return true;
    }

    monster_pathfind mp;
#ifdef WIZARD
    // Remove old highlighted areas to make place for the new ones.
    if (you.wizard)
        for (rectangle_iterator ri(1); ri; ++ri)
            env.pgrid(*ri) &= ~(FPROP_HIGHLIGHT);
#endif

    if (mp.init_pathfind(mon, best_target))
    {
        mon->travel_path = mp.calc_waypoints();

        if (!mon->travel_path.empty())
        {
#ifdef WIZARD
            if (you.wizard)
            {
                for (coord_def pos : mon->travel_path)
                    env.pgrid(pos) |= FPROP_HIGHLIGHT;
            }
#endif
#ifdef DEBUG_PATHFIND
            mprf("Found a path to (%d, %d) with %d surrounding water squares",
                 best_target.x, best_target.y, best_water_count);
#endif
            // Okay then, we found a path. Let's use it!
            mon->firing_pos = mon->travel_path[0];
            mon->travel_target = MTRAV_MERFOLK_AVATAR;
            return true;
        }
    }

    return false;
}

// Returns true if further handling neeeded.
static bool _handle_monster_travelling(monster* mon)
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
        mon->travel_path.erase(mon->travel_path.begin());
        if (mon->travel_path.empty())
        {
#ifdef DEBUG_PATHFIND
            mpr("We reached the end of our path: stop travelling.");
#endif
            mon->travel_target = MTRAV_NONE;
            return true;
        }
        else
        {
            mon->target = mon->travel_path[0];
#ifdef DEBUG_PATHFIND
            mprf("Next waypoint: (%d, %d)", mon->target.x, mon->target.y);
#endif
            return false;
        }
    }

    // Can we still see our next waypoint?
    if (!can_go_straight(mon, mon->pos(), mon->travel_path[0]))
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
            if (can_go_straight(mon, mon->pos(), mon->travel_path[i]))
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
                mon->travel_path.erase(mon->travel_path.begin());
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
                    return true;
                }
            }
            else
            {
                // Or just forget about the whole thing.
                mon->travel_path.clear();
                mon->travel_target = MTRAV_NONE;
                return true;
            }
        }
    }

    // Else, we can see the next waypoint and are making good progress.
    // Carry on, then!
    return false;
}

static bool _choose_random_patrol_target_grid(monster* mon)
{
    const mon_intel_type intel = mons_intel(*mon);

    // Zombies will occasionally just stand around.
    // This does not mean that they don't move every second turn. Rather,
    // once they reach their chosen target, there's a 50% chance they'll
    // just remain there until next turn when this function is called
    // again.
    if (intel == I_BRAINLESS && coinflip())
        return true;

    // If there's no chance we'll find the patrol point, quit right away.
    if (grid_distance(mon->pos(), mon->patrol_point) > 2 * LOS_RADIUS)
        return false;

    // Can the monster see the patrol point from its current position?
    const bool patrol_seen = mon->see_cell(mon->patrol_point);

    if (intel == I_BRAINLESS && !patrol_seen)
    {
        // Really stupid monsters won't even try to get back into the
        // patrol zone.
        return false;
    }

    // While the patrol point is in easy reach, monsters of brainless
    // intelligence will only use a range of 4 (distance from the patrol point).
    // Otherwise, try to get back using the full LOS.
    const int  rad      = (intel > I_BRAINLESS || !patrol_seen) ? LOS_RADIUS
                                                                : 4;
    const bool is_smart = (intel >= I_HUMAN);

    los_def patrol(mon->patrol_point, opacity_monmove(*mon),
                   circle_def(rad, C_SQUARE));
    patrol.update();
    los_def lm(mon->pos(), opacity_monmove(*mon));
    if (is_smart || !patrol_seen)
    {
        // For stupid monsters, don't bother if the patrol point is in sight.
        lm.update();
    }

    int count_grids = 0;
    // Don't bother for the current position. If everything fails,
    // we'll stay here anyway.
    for (radius_iterator ri(mon->patrol_point, you.current_vision, C_SQUARE, true);
         ri; ++ri)
    {
        if (!in_bounds(*ri) || !mon->can_pass_through_feat(grd(*ri)))
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
            if (!patrol.see_cell(*ri)
                && (!is_smart || !lm.see_cell(*ri)))
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
            if (!patrol.see_cell(*ri)
                || !lm.see_cell(*ri))
            {
                continue;
            }

            // If this fails for all surrounding squares (probably because
            // we're too far away), we fall back to heading directly for
            // the patrol point.
        }

        bool set_target = false;
        if (intel == I_BRAINLESS && *ri == mon->patrol_point)
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

    return count_grids;
}// Returns true if further handling neeeded.
static bool _handle_monster_patrolling(monster* mon)
{
    if (!_choose_random_patrol_target_grid(mon))
    {
        // If we couldn't find a target that is within easy reach
        // of the monster and close to the patrol point, head back
        // to patrol point.

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
            return true;
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

    return false;
}

void set_random_target(monster* mon)
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

        if (!summon_can_attack(mon, newtarget))
            continue;

        if (!mon->is_location_safe(newtarget))
            continue;

        mon->target = newtarget;
        break;
    }
}

// Try to find a band leader for the given monster
static monster * _active_band_leader(monster * mon)
{
    // Not a band member
    if (!mon->props.exists("band_leader"))
        return nullptr;

    // Try to find our fearless leader.
    unsigned leader_mid = mon->props["band_leader"].get_int();

    return monster_by_mid(leader_mid);
}

// Return true if a target still needs to be set. If returns false, mon->target
// was set.
static bool _band_wander_target(monster * mon)
{
    int dist_thresh = LOS_RADIUS + HERD_COMFORT_RANGE;
    monster * band_leader = _active_band_leader(mon);
    if (band_leader == nullptr)
        return true;

    int leader_dist = grid_distance(mon->pos(), band_leader->pos());
    if (leader_dist > dist_thresh)
    {
        monster_pathfind mp;
        mp.set_range(1000);

        if (mp.init_pathfind(mon, band_leader->pos()))
        {
            mon->travel_path = mp.calc_waypoints();
            if (!mon->travel_path.empty())
            {
                // Okay then, we found a path. Let's use it!
                mon->target = mon->travel_path[0];
                mon->travel_target = MTRAV_PATROL;
                return false;
            }
            else
                return true;
        }

        return true;
    }

    vector<coord_def> positions;

    for (radius_iterator r_it(mon->pos(), LOS_NO_TRANS, true); r_it; ++r_it)
    {
        if (!in_bounds(*r_it))
            continue;

        if (!band_leader->is_location_safe(*r_it))
            continue;

        int dist = grid_distance(*r_it, band_leader->pos());
        if (dist < HERD_COMFORT_RANGE)
            positions.push_back(*r_it);
    }

    if (positions.empty())
        return true;

    mon->target = positions[random2(positions.size())];

    ASSERT_IN_BOUNDS(mon->target);
    return false;
}

// Returns true if a movement target still needs to be set
static bool _herd_wander_target(monster * mon)
{
    vector<monster_iterator> friends;
    map<int, vector<coord_def> > distance_positions;

    int dist_thresh = LOS_RADIUS + HERD_COMFORT_RANGE;

    for (monster_iterator mit; mit; ++mit)
    {
        if (*mit == mon
            || mons_genus(mit->type) != mons_genus(mon->type)
            || grid_distance(mit->pos(), mon->pos()) > dist_thresh)
        {
            continue;
        }

        friends.push_back(mit);
    }

    if (friends.empty())
        return true;

    for (radius_iterator r_it(mon->pos(), LOS_NO_TRANS, true); r_it; ++r_it)
    {
        if (!in_bounds(*r_it))
            continue;

        if (!mon->is_location_safe(*r_it))
            continue;

        int count = 0;
        for (monster_iterator &fr_it : friends)
        {
            if (grid_distance(fr_it->pos(), *r_it) < HERD_COMFORT_RANGE
                && fr_it->see_cell_no_trans(*r_it))
            {
                count++;
            }
        }
        if (count > 0)
            distance_positions[count].push_back(*r_it);
    }
    map<int, vector<coord_def> >::reverse_iterator back =
        distance_positions.rbegin();

    if (back == distance_positions.rend())
        return true;

    mon->target = back->second[random2(back->second.size())];
    return false;
}

static bool _herd_ok(monster * mon)
{
    bool in_bounds = false;
    bool intermediate_range = false;
    int intermediate_thresh = LOS_RADIUS + HERD_COMFORT_RANGE;

    // herdlings magically know others even out of LOS
    for (monster_iterator mit; mit; ++mit)
    {
        if (*mit == mon)
            continue;

        if (mons_genus(mit->type) == mons_genus(mon->type))
        {
            int g_dist = grid_distance(mit->pos(), mon->pos());
            if (g_dist < HERD_COMFORT_RANGE
                && mon->see_cell_no_trans(mit->pos()))
            {
                in_bounds = true;
                break;
            }
            else if (g_dist < intermediate_thresh)
                intermediate_range = true;
        }
    }

    return in_bounds || !intermediate_range;
}

// Return true if we don't have to do anything to keep within an ok distance
// of our band leader. (If no leader exists we don't have to do anything).
static bool _band_ok(monster * mon)
{
    // Don't have to worry about being close to the leader if no leader can be
    // found.
    monster * leader = _active_band_leader(mon);

    if (!leader)
        return true;

    int g_dist = grid_distance(leader->pos(), mon->pos());

    // If in range, or sufficiently out of range we can just wander around for
    // a while longer.
    if (g_dist < HERD_COMFORT_RANGE && mon->see_cell_no_trans(leader->pos())
        || g_dist >= (LOS_RADIUS + HERD_COMFORT_RANGE))
    {
        return true;
    }

    return false;
}

void check_wander_target(monster* mon, bool isPacified)
{
    // default wander behaviour
    if (mon->pos() == mon->target
        || mons_is_batty(*mon)
        || (!isPacified && !mons_is_avatar(mon->type) && one_chance_in(20))
        || herd_monster(*mon) && !_herd_ok(mon)
        || !_band_ok(mon))
    {
        bool need_target = true;

        if (mon->is_travelling())
            need_target = _handle_monster_travelling(mon);

        // If we still need a target because we're not travelling
        // (any more), check for patrol routes instead.
        if (need_target && mon->is_patrolling())
            need_target = _handle_monster_patrolling(mon);

        if (need_target && herd_monster(*mon))
            need_target = _herd_wander_target(mon);

        if (need_target
            && _active_band_leader(mon) != nullptr)
        {
            need_target = _band_wander_target(mon);
        }

        // XXX: This is really dumb wander behaviour... instead of
        // changing the goal square every turn, better would be to
        // have the monster store a direction and have the monster
        // head in that direction for a while, then shift the
        // direction to the left or right. We're changing this so
        // wandering monsters at least appear to have some sort of
        // attention span.  -- bwr
        if (need_target)
            set_random_target(mon);
    }
}

static void _find_all_level_exits(vector<level_exit> &e)
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

int mons_find_nearest_level_exit(const monster* mon, vector<level_exit> &e,
                                 bool reset)
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
            if (!mons_is_native_in_branch(*mon)
                && grd(e[i].target) == DNGN_UNDISCOVERED_TRAP)
            {
                continue;
            }

            retval = i;
            old_dist = dist;
        }
    }

    return retval;
}
void set_random_slime_target(monster* mon)
{
    // Strictly neutral slimes will go for the nearest item.
    for (distance_iterator ri(mon->pos(), true, false, you.current_vision);
                              ri; ++ri)
    {
        if (!in_bounds(*ri) || !mon->see_cell(*ri))
            continue;
        if (testbits(env.pgrid(*ri), FPROP_NO_JIYVA))
            continue;
        for (stack_iterator si(*ri); si; ++si)
        {
            item_def& item(*si);

            if (item_is_jelly_edible(item))
            {
                mon->target = *ri;
                goto end;
            }
        }
    }
end:
    if (mon->target == mon->pos() || mon->target == you.pos())
        set_random_target(mon);
}

static bool _can_safely_go_through(const monster * mon, const coord_def p)
{
    ASSERT(map_bounds(p));

    if (!monster_habitable_grid(mon, grd(p)))
        return false;

    // Stupid monsters don't pathfind around shallow water
    // except the clinging ones.
    if (mon->floundering_at(p)
        && (mons_intel(*mon) >= I_HUMAN || mon->can_cling_to_walls()))
    {
        return false;
    }

    return true;
}

// Checks whether there is a straight path from p1 to p2 that monster can
// safely passes through.
// If it exists, such a path may be missed; on the other hand, it
// is not guaranteed that p2 is visible from p1 according to LOS rules.
// Not symmetric.
// FIXME: This is used for monster movement. It should instead be
//        something like exists_ray(p1, p2, opacity_monmove(mons)),
//        where opacity_monmove() is fixed to include opacity_immob.
bool can_go_straight(const monster* mon, const coord_def& p1,
                     const coord_def& p2)
{
    // If no distance, then trivially true
    if (p1 == p2)
        return true;

    if (grid_distance(p1, p2) > los_radius)
        return false;

    // XXX: Hack to improve results for now. See FIXME above.
    ray_def ray;
    if (!find_ray(p1, p2, ray, opc_immob))
        return false;

    while (ray.advance() && ray.pos() != p2)
        if (!_can_safely_go_through(mon, ray.pos()))
            return false;

    return true;
}
