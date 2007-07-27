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

// An object that modifies the behaviour of the direction prompt.
class targeting_behaviour
{
public:
    targeting_behaviour(bool just_looking = false);
    virtual ~targeting_behaviour();
    
    // Returns a keystroke for the prompt.
    virtual int get_key();
    virtual command_type get_command(int key = -1);
    virtual bool should_redraw();

public:
    bool just_looking;
    bool compass;
};

void direction( dist &moves, targeting_type restricts = DIR_NONE,
                targ_mode_type mode = TARG_ANY, bool just_looking = false,
                const char *prompt = NULL, targeting_behaviour *mod = NULL );

bool in_los_bounds(int x, int y);
bool in_viewport_bounds(int x, int y);
bool in_los(int x, int y);
bool in_vlos(int x, int y);
bool in_vlos(const coord_def &pos);

void terse_describe_square(const coord_def &c);
void full_describe_square(const coord_def &c);
void describe_floor();

int dos_direction_unmunge(int doskey);

std::string feature_description(int mx, int my,
                                description_level_type dtype = DESC_CAP_A,
                                bool add_stop = true);
std::string raw_feature_description(dungeon_feature_type grid,
                                    trap_type tr = NUM_TRAPS);
std::string feature_description(dungeon_feature_type grid,
                                trap_type trap = NUM_TRAPS,
                                description_level_type dtype = DESC_CAP_A,
                                bool add_stop = true);

std::vector<dungeon_feature_type> features_by_desc(const base_pattern &pattern);

inline int view2gridX(int vx)
{
    return (crawl_view.vgrdc.x + vx - crawl_view.view_centre().x);
}

inline int view2gridY(int vy)
{
    return (crawl_view.vgrdc.y + vy - crawl_view.view_centre().y);
}

inline coord_def view2grid(const coord_def &pos)
{
    return pos - crawl_view.view_centre() + crawl_view.vgrdc;
}

inline int grid2viewX(int gx)
{
    return (gx - crawl_view.vgrdc.x + crawl_view.view_centre().x);
}

inline int grid2viewY(int gy)
{
    return (gy - crawl_view.vgrdc.y + crawl_view.view_centre().y);
}

inline coord_def grid2view(const coord_def &pos)
{
    return (pos - crawl_view.vgrdc + crawl_view.view_centre());
}

inline coord_def view2show(const coord_def &pos)
{
    return (pos - crawl_view.vlos1 + coord_def(1, 1));
}

inline coord_def grid2show(const coord_def &pos)
{
    return (view2show(grid2view(pos)));
}

#endif
