/*
 *  File:       direct.cc
 *  Summary:    Functions used when picking squares.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 *
 *  Change History (most recent first):
 *
 *               <1>     -/--/--        LRH             Created
 */


#ifndef DIRECT_H
#define DIRECT_H

#include "externs.h"
#include "enum.h"

// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: acr - debug - effects - it_use3 - item_use - spells1 -
 *              spells2 - spells3 - spells4
 * *********************************************************************** */

void direction( struct dist &moves, targeting_type restricts = DIR_NONE,
                int mode = TARG_ANY, bool just_looking = false,
                const char *prompt = NULL );

bool in_los_bounds(int x, int y);
bool in_viewport_bounds(int x, int y);
bool in_los(int x, int y);
bool in_vlos(int x, int y);

int dos_direction_unmunge(int doskey);

std::string feature_description(int mx, int my);
std::string feature_description(int grid);

std::vector<dungeon_feature_type> features_by_desc(const text_pattern &pattern);

inline int view2gridX(int vx)
{
    return (you.x_pos + vx - VIEW_CX);
}

inline int view2gridY(int vy)
{
    return (you.y_pos + vy - VIEW_CY);
}

inline coord_def view2grid(const coord_def &pos)
{
    return coord_def( view2gridX(pos.x), view2gridY(pos.y) );
}

inline int grid2viewX(int gx)
{
    return (gx - you.x_pos + VIEW_CX);
}

inline int grid2viewY(int gy)
{
    return (gy - you.y_pos + VIEW_CY);
}

inline coord_def grid2view(const coord_def &pos)
{
    return coord_def( grid2viewX(pos.x), grid2viewY(pos.y) );
}

#endif
