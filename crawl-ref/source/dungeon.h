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

#define MAKE_GOOD_ITEM          351

// Should be the larger of GXM/GYM
#define MAP_SIDE  GXM

// This may sometimes be used as map_type[x][y] (for minivaults) and as
// map_type[y][x] for large-scale vaults. Keep an eye out for the associated
// brain-damage. [dshaligram]
typedef char map_type[MAP_SIDE + 1][MAP_SIDE + 1];

void item_colour( item_def &item );

// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: files
 * *********************************************************************** */
void builder(int level_number, char level_type);


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: abyss - debug - dungeon - effects - religion - spells4
 * *********************************************************************** */
int items( int allow_uniques, int force_class, int force_type, 
           bool dont_place, int item_level, int item_race );

// last updated 13mar2001 {gdl}
/* ***********************************************************************
 * called from: dungeon monplace
 * *********************************************************************** */
void give_item(int mid, int level_number);

void init_rod_mp(item_def &item);

// last updated 13mar2001 {gdl}
/* ***********************************************************************
 * called from: dungeon monplace
 * *********************************************************************** */
void define_zombie(int mid, int ztype, int cs, int power);

bool is_wall(int feature);

bool place_specific_trap(unsigned char spec_x, unsigned char spec_y,
                         unsigned char spec_type);

void place_spec_shop(int level_number, unsigned char shop_x,
                         unsigned char shop_y, unsigned char force_s_type,
                         bool representative = false );

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

    static dgn_region absolute(int left, int top, int right, int bottom)
    {
        return dgn_region(left, top, right - left + 1, bottom - top + 1);
    }

    static bool between(int val, int low, int high)
    {
        return (val >= low && val <= high);
    }

    bool contains(const coord_def &p) const
    {
        return (p.x >= pos.x && p.x < pos.x + size.x
                && p.y >= pos.y && p.y < pos.y + size.y);
    }
    
    bool fully_contains(const coord_def &p) const
    {
        return (p.x > pos.x && p.x < pos.x + size.x - 1
                && p.y >= pos.y && p.y < pos.y + size.y - 1);
    }
    
    bool overlaps(const dgn_region &other) const;
    bool overlaps_any(const dgn_region_list &others) const;
};

#endif
