#pragma once

#include "coord-def.h"
#include "defines.h"
#include "fixedvector.h"
#include "maybe-bool.h"
#include <unordered_map>
#include <vector>

using std::vector;

class monster;

int mons_tracking_range(const monster* mon);

class monster_pathfind
{
public:
    monster_pathfind();
    virtual ~monster_pathfind();

    // public methods
    void set_range(int r);
    coord_def next_pos(const coord_def &p) const;
    bool init_pathfind(const monster* mon, coord_def dest,
                       bool diag = true, bool msg = false,
                       bool pass_unmapped = false);
    bool init_pathfind(coord_def src, coord_def dest, bool doors = true,
                       bool diag = true, bool no_actors = false,
                       bool msg = false);
    void fill_traversability(const monster* mon, int range,
                             bool no_actors = false);
    bool start_pathfind(bool msg = false);
    vector<coord_def> backtrack();
    vector<coord_def> calc_waypoints();
    bool is_reachable(const coord_def& pos);

protected:
    // protected methods
    bool calc_path_to_neighbours();
    bool traversable(const coord_def& p);
    bool traversable_memoized(const coord_def& p);
    int  travel_cost(coord_def npos);
    bool mons_traversable(const coord_def& p);
    int  mons_travel_cost(coord_def npos);
    int  estimated_cost(coord_def npos);
    void add_new_pos(coord_def pos, int total);
    void update_pos(coord_def pos, int total);
    bool get_best_position();

    // The monster trying to find a path.
    const monster* mons;

    // Our destination, and the current position we're looking at.
    coord_def start, target, pos;

    // If false, do not move diagonally along the path.
    bool allow_diagonals;

    // If true, unmapped terrain is treated as traversable no matter the
    // monster involved.
    // (Used for player estimates of whether a monster can travel somewhere.)
    bool traverse_unmapped;

    // Only follow paths that do not leave the player's sight (used for
    // friendly summoned monster which are not already out of sight)
    bool traverse_in_sight;

    // For pathfinding without a specific monster agent, should we be able to
    // traverse closed doors?
    bool traverse_doors;

    // If true, pathfinding will consider all spaces occupied by an actor to
    // be opaque.
    bool traverse_no_actors;

    // If true, will test all reachable spaces within [range] of start, rather
    // than care whether the destination is reachable.
    bool fill_range;

    // Maximum range to search between start and target. None, if zero.
    int range;

    // Currently shortest and longest possible total length of the path.
    int min_length;
    int max_length;

    // The array of distances from start to any already tried point.
    int dist[GXM][GYM];
    // An array to store where we came from on a given shortest path.
    int prev[GXM][GYM];

    FixedVector<vector<coord_def>, GXM * GYM> hash;

    maybe_bool traversable_cache[GXM][GYM];
};
