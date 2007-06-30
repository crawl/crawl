/*
 *  File:       dungeon.cc
 *  Summary:    Functions used when building new levels.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 *
 *  Change History (most recent first):
 *
 *               <1>     -/--/--        LRH             Created
 */


#ifndef DUNGEON_H
#define DUNGEON_H

#include "FixVec.h"
#include "externs.h"
#include "misc.h"
#include "travel.h"
#include "stuff.h"

const int MAKE_GOOD_ITEM = 351;

// Should be the larger of GXM/GYM
#define MAP_SIDE  GXM

// This may sometimes be used as map_type[x][y] (for minivaults) and as
// map_type[y][x] for large-scale vaults. Keep an eye out for the associated
// brain-damage. [dshaligram]
typedef char map_type[MAP_SIDE + 1][MAP_SIDE + 1];

class dgn_region;
typedef std::vector<dgn_region> dgn_region_list;

struct dgn_region
{
    // pos is top-left corner.
    coord_def pos, size;

    dgn_region(int left, int top, int width, int height)
        : pos(left, top), size(width, height)
    {
    }

    dgn_region() : pos(-1, -1), size()
    {
    }

    coord_def end() const
    {
        return pos + size - coord_def(1, 1);
    }

    coord_def random_edge_point() const;
    coord_def random_point() const;

    static dgn_region absolute(int left, int top, int right, int bottom)
    {
        return dgn_region(left, top, right - left + 1, bottom - top + 1);
    }

    static dgn_region absolute(const coord_def &c1, const coord_def &c2)
    {
        return dgn_region(c1.x, c1.y, c2.x, c2.y);
    }

    static bool between(int val, int low, int high)
    {
        return (val >= low && val <= high);
    }

    bool contains(const coord_def &p) const
    {
        return contains(p.x, p.y);
    }

    bool contains(int xp, int yp) const
    {
        return (xp >= pos.x && xp < pos.x + size.x
                && yp >= pos.y && yp < pos.y + size.y);
    }
    
    bool fully_contains(const coord_def &p) const
    {
        return (p.x > pos.x && p.x < pos.x + size.x - 1
                && p.y >= pos.y && p.y < pos.y + size.y - 1);
    }
    
    bool overlaps(const dgn_region &other) const;
    bool overlaps_any(const dgn_region_list &others) const;
};

void builder(int level_number, int level_type);
void define_zombie(int mid, int ztype, int cs, int power);
bool is_wall(int feature);
bool place_specific_trap(int spec_x, int spec_y,  trap_type spec_type);
void place_spec_shop(int level_number, int shop_x, int shop_y,
                     int force_s_type, bool representative = false);
bool unforbidden(const coord_def &c, const dgn_region_list &forbidden);

//////////////////////////////////////////////////////////////////////////
// Map markers

class map_marker
{
public:
    map_marker(map_marker_type type, const coord_def &pos);
    virtual ~map_marker();

    map_marker_type get_type() const { return type; }

    virtual void write(tagHeader &) const;
    virtual void read(tagHeader &);
    virtual map_marker *clone() const = 0;
    virtual std::string describe() const = 0;
    
    static map_marker *read_marker(tagHeader&);

public:
    coord_def pos;

protected:
    map_marker_type type;

    typedef map_marker *(*marker_reader)(tagHeader &, map_marker_type);
    static marker_reader readers[NUM_MAP_MARKER_TYPES];
};

class map_feature_marker : public map_marker
{
public:
    map_feature_marker(const coord_def &pos = coord_def(0, 0),
                       dungeon_feature_type feat = DNGN_UNSEEN);
    map_feature_marker(const map_feature_marker &other);
    void write(tagHeader &) const;
    void read(tagHeader &);
    map_marker *clone() const;
    std::string describe() const;
    static map_marker *read(tagHeader &, map_marker_type);
    
public:
    dungeon_feature_type feat;
};

//////////////////////////////////////////////////////////////////////////
template <typename fgrd, typename bound_check>
class flood_find : public travel_pathfind
{
public:
    flood_find(const fgrd &f, const bound_check &bc);

    void add_feat(int feat);
    void add_point(const coord_def &pos);
    coord_def find_first_from(const coord_def &c, const dgn_region_list &vlts);
    bool points_connected_from(const coord_def &start);
    bool any_point_connected_from(const coord_def &start);
    bool has_exit_from(const coord_def &start);

    bool did_leave_vault() const { return left_vault; }
    
protected:
    bool path_flood(const coord_def &c, const coord_def &dc);
protected:
    bool point_hunt, want_exit;
    bool needed_features[NUM_FEATURES];
    std::vector<coord_def> needed_points;
    bool left_vault;
    dgn_region_list vaults;

    const fgrd &fgrid;
    const bound_check &bcheck;
};

template <typename fgrd, typename bound_check>
flood_find<fgrd, bound_check>::flood_find(const fgrd &f, const bound_check &bc)
    : travel_pathfind(), point_hunt(false), want_exit(false),
      needed_features(), needed_points(), left_vault(true), vaults(),
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
    const dgn_region_list &vlts)
{
    set_floodseed(c);
    vaults = vlts;
    return pathfind(RMODE_EXPLORE);
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
        return (true);
    set_floodseed(sp);
    pathfind(RMODE_EXPLORE);
    return (needed_points.empty());
}

template <typename fgrd, typename bound_check>
bool flood_find<fgrd, bound_check>::any_point_connected_from(
    const coord_def &sp)
{
    if (needed_points.empty())
        return (true);
    set_floodseed(sp);
    const size_t sz = needed_points.size();
    pathfind(RMODE_EXPLORE);
    return (needed_points.size() < sz);
}

template <typename fgrd, typename bound_check>
bool flood_find<fgrd, bound_check>::has_exit_from(
    const coord_def &sp)
{
    want_exit = true;
    set_floodseed(sp);
    return (pathfind(RMODE_EXPLORE) == coord_def(-1, -1));
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
            return (true);
        }
        return (false);
    }

    if (!needed_points.empty())
    {
        std::vector<coord_def>::iterator i =
            std::find(needed_points.begin(), needed_points.end(), dc);
        if (i != needed_points.end())
        {
            needed_points.erase(i);
            if (needed_points.empty())
                return (true);
        }
    }

    const dungeon_feature_type grid = fgrid(dc);
    if (needed_features[ grid ])
    {
        unexplored_place = dc;
        unexplored_dist  = traveled_distance;
        return (true);
    }

    if (!is_traversable(grid)
        && grid != DNGN_SECRET_DOOR
        && !grid_is_trap(grid))
    {
        return (false);
    }

    if (!left_vault && unforbidden(dc, vaults))
        left_vault = true;

    good_square(dc);
    
    return (false);
}
//////////////////////////////////////////////////////////////////////////

#endif
