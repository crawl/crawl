#include "AppHdr.h"

#include "mon-pathfind.h"

#include "directn.h"
#include "env.h"
#include "los.h"
#include "misc.h"
#include "mon-movetarget.h"
#include "mon-place.h"
#include "religion.h"
#include "state.h"
#include "terrain.h"
#include "traps.h"

/////////////////////////////////////////////////////////////////////////////
// monster_pathfind

// The pathfinding is an implementation of the A* algorithm. Beginning at the
// monster position we check all neighbours of a given grid, estimate the
// distance needed for any shortest path including this grid and push the
// result into a hash. We can then easily access all points with the shortest
// distance estimates and then check _their_ neighbours and so on.
// The algorithm terminates once we reach the destination since - because
// of the sorting of grids by shortest distance in the hash - there can be no
// path between start and target that is shorter than the current one. There
// could be other paths that have the same length but that has no real impact.
// If the hash has been cleared and the start grid has not been encountered,
// then there's no path that matches the requirements fed into monster_pathfind.
// (These requirements are usually preference of habitat of a specific monster
// or a limit of the distance between start and any grid on the path.)

int mons_tracking_range(const monster* mon)
{
    int range = 0;
    switch (mons_intel(*mon))
    {
    case I_BRAINLESS:
        range = 3;
        break;
    case I_ANIMAL:
        range = 5;
        break;
    case I_HUMAN:
        range = LOS_DEFAULT_RANGE;
        break;
    }

    if (mons_is_native_in_branch(*mon))
        range += 3;

    if (player_under_penance(GOD_ASHENZARI))
        range *= 5;

    if (mons_foe_is_marked(*mon) || mon->has_ench(ENCH_HAUNTING))
        range *= 5;

    ASSERT(range);

    return range;
}

//#define DEBUG_PATHFIND
monster_pathfind::monster_pathfind()
    : mons(nullptr), start(), target(), pos(), allow_diagonals(true),
      traverse_unmapped(false), fill_range(false),
      range(0), min_length(0), max_length(0),
      dist(), prev(), hash(), traversable_cache()
{
}

monster_pathfind::~monster_pathfind()
{
}

void monster_pathfind::set_range(int r)
{
    if (r >= 0)
        range = r;
}

coord_def monster_pathfind::next_pos(const coord_def &c) const
{
    return c + Compass[prev[c.x][c.y]];
}

// The main method in the monster_pathfind class.
// Returns true if a path was found, else false.
bool monster_pathfind::init_pathfind(const monster* mon, coord_def dest,
                                     bool diag, bool msg, bool pass_unmapped)
{
    mons   = mon;

    start  = mon->pos();
    target = dest;
    pos    = start;
    allow_diagonals   = diag;
    traverse_unmapped = pass_unmapped;
    traverse_in_sight = (!crawl_state.game_is_arena()
                         && mon->friendly() && mon->is_summoned()
                         && you.see_cell_no_trans(mon->pos()));
    traverse_no_actors = false;

    // Easy enough. :P
    if (start == target)
    {
        if (msg)
            mpr("The monster is already there!");

        return true;
    }

    return start_pathfind(msg);
}

bool monster_pathfind::init_pathfind(coord_def src, coord_def dest, bool doors,
                                     bool diag, bool no_actors, bool msg)
{
    start  = src;
    target = dest;
    pos    = start;
    allow_diagonals = diag;
    traverse_doors = doors;
    traverse_no_actors = no_actors;

    // Easy enough. :P
    if (start == target)
        return true;

    return start_pathfind(msg);
}

void monster_pathfind::fill_traversability(const monster* mon, int _range,
                                           bool no_actors)
{
    fill_range = true;
    traverse_no_actors = no_actors;
    set_range(_range);
    init_pathfind(mon, coord_def());
}

bool monster_pathfind::start_pathfind(bool msg)
{
    // NOTE: We never do any traversable() check for the target square.
    //       This means that even if the target cannot be reached
    //       we may still find a path leading adjacent to this position, which
    //       is desirable if e.g. the player is hovering over deep water
    //       surrounded by shallow water or floor, or if a foe is hiding in
    //       a wall.

    max_length = min_length = grid_distance(pos, target);
    if (fill_range)
    {
        min_length = 1;
        max_length = range;
    }
    for (int i = 0; i < GXM; i++)
        for (int j = 0; j < GYM; j++)
        {
            dist[i][j] = INFINITE_DISTANCE;
            traversable_cache[i][j] = maybe_bool::maybe;
        }

    dist[pos.x][pos.y] = 0;

    bool success = false;
    do
    {
        // Calculate the distance to all neighbours of the current position,
        // and add them to the hash, if they haven't already been looked at.
        success = calc_path_to_neighbours();
        if (success)
            return true;

        // Pull the position with shortest distance estimate to our target grid.
        success = get_best_position();

        if (!success)
        {
            if (msg)
            {
                mprf("Couldn't find a path from (%d,%d) to (%d,%d).",
                     target.x, target.y, start.x, start.y);
            }
            return false;
        }
    }
    while (true);
}

// Returns true as soon as we encounter the target.
bool monster_pathfind::calc_path_to_neighbours()
{
    coord_def npos;
    int distance, old_dist, total;

    // For each point, we look at all neighbour points. Check the orthogonals
    // last, so that, should an orthogonal and a diagonal direction have the
    // same total travel cost, the orthogonal will be picked first, and thus
    // zigzagging will be significantly reduced.
    //
    //      1  0  3       This means directions are looked at, in order,
    //       \ | /        1, 3, 5, 7 (diagonals) followed by 0, 2, 4, 6
    //      6--.--2       (orthogonals). This is achieved by the assignment
    //       / | \        of (dir = 0) once dir has passed 7.
    //      7  4  5
    //
    // To avoid bias, we'll choose a random 90 degree rotation
    int rotate = random2(4) * 2; // equal probability of 0,2,4,6
    for (int idir = 1; idir < 8; (idir += 2) == 9 && (idir = 0))
    {
        // Skip diagonal movement.
        if (!allow_diagonals && (idir % 2))
            continue;

        int dir = (idir + rotate) % 8;  // apply our random 90 degree rotation

        npos = pos + Compass[dir];

#ifdef DEBUG_PATHFIND
        mprf("Looking at neighbour (%d,%d)", npos.x, npos.y);
#endif
        if (!in_bounds(npos))
            continue;

        // Ignore this grid if it takes us above the allowed distance
        // away from the target.
        if (range && estimated_cost(npos) > range)
            continue;

        if (!traversable_memoized(npos) && npos != target)
            continue;

        distance = dist[pos.x][pos.y] + travel_cost(npos);
        old_dist = dist[npos.x][npos.y];

        // Also bail out if this would make the path longer than twice the
        // allowed distance from the target. (This factor may need tuning.)
        //
        // This is actually motivated by performance, as pathfinding
        // in mazes with see-through walls (e.g. plants) can otherwise
        // soak up a lot of CPU cycles.
        if (range && distance > range * 2)
            continue;

#ifdef DEBUG_PATHFIND
        mprf("old dist: %d, new dist: %d, infinite: %d", old_dist, distance,
             INFINITE_DISTANCE);
#endif
        // If the new distance is better than the old one (initialised with
        // INFINITE), update the position.
        if (distance < old_dist)
        {
            // Calculate new total path length.
            total = distance + estimated_cost(npos);
            if (old_dist == INFINITE_DISTANCE)
            {
#ifdef DEBUG_PATHFIND
                mprf("Adding (%d,%d) to hash (total dist = %d)",
                     npos.x, npos.y, total);
#endif
                add_new_pos(npos, total);
                if (total > max_length)
                    max_length = total;
            }
            else
            {
#ifdef DEBUG_PATHFIND
                mprf("Improving (%d,%d) to total dist %d",
                     npos.x, npos.y, total);
#endif

                update_pos(npos, total);
            }

            // Update distance start->pos.
            dist[npos.x][npos.y] = distance;

            // Set backtracking information.
            // Converts the Compass direction to its counterpart.
            //      0  1  2         4  5  6
            //      7  .  3   ==>   3  .  7       e.g. (3 + 4) % 8          = 7
            //      6  5  4         2  1  0            (7 + 4) % 8 = 11 % 8 = 3

            prev[npos.x][npos.y] = (dir + 4) % 8;

            // Are we finished?
            if (npos == target)
            {
#ifdef DEBUG_PATHFIND
                mpr("Arrived at target.");
#endif
                return true;
            }
        }
    }
    return false;
}

// Starting at known min_length (minimum total estimated path distance), check
// the hash for existing vectors, then pick the last entry of the first vector
// that matches. Update min_length, if necessary.
bool monster_pathfind::get_best_position()
{
    for (int i = min_length; i <= max_length; i++)
    {
        if (!hash[i].empty())
        {
            if (i > min_length)
                min_length = i;

            vector<coord_def> &vec = hash[i];
            // Pick the last position pushed into the vector as it's most
            // likely to be close to the target.
            pos = vec[vec.size()-1];
            vec.pop_back();

#ifdef DEBUG_PATHFIND
            mprf("Returning (%d, %d) as best pos with total dist %d.",
                 pos.x, pos.y, min_length);
#endif

            return true;
        }
#ifdef DEBUG_PATHFIND
        mprf("No positions for path length %d.", i);
#endif
    }

    // Nothing found? Then there's no path! :(
    return false;
}

// Using the prev vector backtrack from start to target to find all steps to
// take along the shortest path.
vector<coord_def> monster_pathfind::backtrack()
{
#ifdef DEBUG_PATHFIND
    mpr("Backtracking...");
#endif
    vector<coord_def> path;
    pos = target;
    path.push_back(pos);

    if (pos == start)
        return path;

    int dir;
    do
    {
        dir = prev[pos.x][pos.y];
        pos = pos + Compass[dir];
        ASSERT_IN_BOUNDS(pos);
#ifdef DEBUG_PATHFIND
        mprf("prev: (%d, %d), pos: (%d, %d)", Compass[dir].x, Compass[dir].y,
                                              pos.x, pos.y);
#endif
        path.insert(path.begin(), pos);

        if (pos.origin())
            break;
    }
    while (pos != start);
    ASSERT(pos == start);

    return path;
}

// Reduces the path coordinates to only a couple of key waypoints needed
// to reach the target. Waypoints are chosen such that from one waypoint you
// can see (and, more importantly, reach) the next one. Note that
// can_go_straight() is probably rather too conservative in these estimates.
// This is done because Crawl's pathfinding - once a target is in sight and easy
// reach - is both very robust and natural, especially if we want to flexibly
// avoid plants and other monsters in the way.
vector<coord_def> monster_pathfind::calc_waypoints()
{
    vector<coord_def> path = backtrack();

    // If no path found, nothing to be done.
    if (path.empty())
        return path;

    vector<coord_def> waypoints;
    pos = path[0];

#ifdef DEBUG_PATHFIND
    mpr("\nWaypoints:");
#endif
    for (unsigned int i = 1; i < path.size(); i++)
    {
        if (can_go_straight(mons, pos, path[i]) && mons_traversable(path[i]))
            continue;
        else
        {
            pos = path[i-1];
            waypoints.push_back(pos);
#ifdef DEBUG_PATHFIND
            mprf("waypoint: (%d, %d)", pos.x, pos.y);
#endif
        }
    }

    // Add the actual target to the list of waypoints, so we can later check
    // whether a tracked enemy has moved too much, in case we have to update
    // the path.
    if (pos != path[path.size() - 1])
        waypoints.push_back(path[path.size() - 1]);

    return waypoints;
}

bool monster_pathfind::traversable_memoized(const coord_def& p)
{
    if (traversable_cache[p.x][p.y] == maybe_bool::maybe)
        traversable_cache[p.x][p.y] = traversable(p);
    return bool(traversable_cache[p.x][p.y]);
}

// Since traversable_memoized is only called for spaces that were at least
// reachable in some fashion, this can be used after pathfinding calculations
// are done to determine whether this space was somehow reachable within the
// pathfinding range.
bool monster_pathfind::is_reachable(const coord_def& p)
{
    return dist[p.x][p.y] <= range && bool(traversable_cache[p.x][p.y]);
}

bool monster_pathfind::traversable(const coord_def& p)
{
    if (!traverse_unmapped && env.grid(p) == DNGN_UNSEEN)
        return false;

    if (traverse_no_actors && actor_at(p))
        return false;

    // XXX: Hack to be somewhat consistent with uses of
    //      opc_immob elsewhere in pathfinding.
    //      All of this should eventually be replaced by
    //      giving the monster a proper pathfinding LOS.
    if (opc_immob(p) == OPC_OPAQUE && !feat_is_closed_door(env.grid(p)))
    {
        // XXX: Ugly hack to make thorn hunters use their briars for defensive
        //      cover instead of just pathing around them.
        if (mons && mons->type == MONS_THORN_HUNTER
            && monster_at(p)
            && monster_at(p)->type == MONS_BRIAR_PATCH)
        {
            return true;
        }

        return false;
    }

    if (mons)
        return mons_traversable(p);

    if (traverse_doors && feat_is_closed_door(env.grid(p))
        && !cell_is_runed(p))
    {
        return true;
    }

    return feat_has_solid_floor(env.grid(p));
}

// Checks whether a given monster can pass over a certain position, respecting
// its preferred habit and capability of flight or opening doors.
bool monster_pathfind::mons_traversable(const coord_def& p)
{
    return mons_can_traverse(*mons, p, traverse_in_sight);
}

int monster_pathfind::travel_cost(coord_def npos)
{
    if (mons)
        return mons_travel_cost(npos);

    return 1;
}

// Assumes that grids that really cannot be entered don't even get here.
// (Checked by traversable().)
int monster_pathfind::mons_travel_cost(coord_def npos)
{
    ASSERT(grid_distance(pos, npos) <= 1);

    // Doors need to be opened.
    if (feat_is_closed_door(env.grid(npos)))
        return 2;

    // Travelling through water, entering or leaving water is more expensive
    // for non-amphibious monsters, so they'll avoid it where possible.
    // (The resulting path might not be optimal but it will lead to a path
    // a monster of such habits is likely to prefer.)
    if (mons->floundering_at(npos))
        return 2;

    // Try to avoid traps.
    const trap_def* ptrap = trap_at(npos);
    if (ptrap)
    {
        if (ptrap->is_bad_for_player())
        {
            // Your allies take extra precautions to avoid traps that are bad
            // for you (elsewhere further checks are made to mark Zot traps as
            // impassible).
            if (mons->friendly())
                return 3;

            // To hostile monsters, these traps are completely harmless.
            return 1;
        }

        return 2;
    }

    return 1;
}

// The estimated cost to reach a grid is simply max(dx, dy).
int monster_pathfind::estimated_cost(coord_def p)
{
    return fill_range ? grid_distance(p, start) : grid_distance(p, target);
}

void monster_pathfind::add_new_pos(coord_def npos, int total)
{
    hash[total].push_back(npos);
}

void monster_pathfind::update_pos(coord_def npos, int total)
{
    // Find hash position of old distance and delete it,
    // then call_add_new_pos.
    int old_total = dist[npos.x][npos.y] + estimated_cost(npos);

    vector<coord_def> &vec = hash[old_total];
    for (unsigned int i = 0; i < vec.size(); i++)
    {
        if (vec[i] == npos)
        {
            vec.erase(vec.begin() + i);
            break;
        }
    }

    add_new_pos(npos, total);
}
