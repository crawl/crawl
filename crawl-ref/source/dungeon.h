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

bool place_specific_trap(unsigned char spec_x, unsigned char spec_y,
                         trap_type spec_type);

void place_spec_shop(int level_number, unsigned char shop_x,
                     unsigned char shop_y, unsigned char force_s_type,
                     bool representative = false );

bool unforbidden(const coord_def &c, const dgn_region_list &forbidden);

#endif
