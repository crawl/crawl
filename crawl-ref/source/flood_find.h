/**
 * @file
 * @brief flood_find template
**/

#ifndef FLOOD_FIND_H
#define FLOOD_FIND_H

#include "terrain.h"
#include "travel.h"

template <typename fgrd, typename bound_check>
class flood_find : public travel_pathfind
{
public:
    flood_find(const fgrd &f, const bound_check &bc, bool avoid_vaults = false,
                                                bool _check_traversable = true);

    void add_feat(int feat);
    void add_point(const coord_def &pos);
    coord_def find_first_from(const coord_def &c, const map_mask &vlts);
    bool points_connected_from(const coord_def &start);
    bool any_point_connected_from(const coord_def &start);
    bool has_exit_from(const coord_def &start);

    bool did_leave_vault() const { return left_vault; }

protected:
    bool path_flood(const coord_def &c, const coord_def &dc) override;
protected:
    bool point_hunt, want_exit, no_vault, check_traversable;
    bool needed_features[NUM_FEATURES];
    vector<coord_def> needed_points;
    bool left_vault;
    const map_mask *vaults;

    const fgrd &fgrid;
    bound_check bcheck;
};

template <typename fgrd, typename bound_check>
flood_find<fgrd, bound_check>::flood_find(const fgrd &f, const bound_check &bc,
                                     bool avoid_vaults, bool _check_traversable)
    : travel_pathfind(), point_hunt(false), want_exit(false),
      no_vault(avoid_vaults), check_traversable(_check_traversable),
      needed_features(), needed_points(), left_vault(true), vaults(nullptr),
      fgrid(f), bcheck(bc)
{
    memset(needed_features, false, sizeof needed_features);
}

template <typename fgrd, typename bound_check>
void flood_find<fgrd, bound_check>::add_feat(int feat)
{
    if (feat >= 0 && feat < NUM_FEATURES)
        needed_features[feat] = true;
}

template <typename fgrd, typename bound_check>
coord_def
flood_find<fgrd, bound_check>::find_first_from(
    const coord_def &c,
    const map_mask &vlts)
{
    set_floodseed(c);
    vaults = &vlts;
    return pathfind(RMODE_CONNECTIVITY);
}

template <typename fgrd, typename bound_check>
void flood_find<fgrd, bound_check>::add_point(const coord_def &c)
{
    needed_points.push_back(c);
}

template <typename fgrd, typename bound_check>
bool flood_find<fgrd, bound_check>::points_connected_from(
    const coord_def &sp)
{
    if (needed_points.empty())
        return true;
    set_floodseed(sp);
    pathfind(RMODE_CONNECTIVITY);
    return needed_points.empty();
}

template <typename fgrd, typename bound_check>
bool flood_find<fgrd, bound_check>::any_point_connected_from(
    const coord_def &sp)
{
    if (needed_points.empty())
        return true;
    set_floodseed(sp);
    const size_t sz = needed_points.size();
    pathfind(RMODE_CONNECTIVITY);
    return needed_points.size() < sz;
}

template <typename fgrd, typename bound_check>
bool flood_find<fgrd, bound_check>::has_exit_from(
    const coord_def &sp)
{
    want_exit = true;
    set_floodseed(sp);
    return pathfind(RMODE_CONNECTIVITY) == coord_def(-1, -1);
}

template <typename fgrd, typename bound_check>
bool flood_find<fgrd, bound_check>::path_flood(
    const coord_def &c,
    const coord_def &dc)
{
    if (!bcheck(dc))
    {
        if (want_exit)
        {
            greedy_dist = 100;
            greedy_place = coord_def(-1, -1);
            return true;
        }
        return false;
    }

    if (no_vault && (*vaults)[dc.x][dc.y])
        return false;

    if (!needed_points.empty())
    {
        vector<coord_def>::iterator i = find(needed_points.begin(),
                                             needed_points.end(), dc);
        if (i != needed_points.end())
        {
            needed_points.erase(i);
            if (needed_points.empty())
                return true;
        }
    }

    const dungeon_feature_type feat = fgrid(dc);

    if (feat == NUM_FEATURES)
    {
        if (want_exit)
        {
            greedy_dist = 100;
            greedy_place = coord_def(-1, -1);
            return true;
        }
        return false;
    }

    if (needed_features[ feat ])
    {
        unexplored_place = dc;
        unexplored_dist  = traveled_distance;
        return true;
    }

    if (check_traversable && !feat_is_traversable(feat)
        && !feat_is_trap(feat))
    {
        return false;
    }

    if (!left_vault && vaults && !(*vaults)[dc.x][dc.y])
        left_vault = true;

    good_square(dc);

    return false;
}

#endif
