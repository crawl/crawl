/*
 * File:     teleport.cc
 * Summary:  Functions related to teleportation and blinking.
 */

#include "AppHdr.h"

#include "teleport.h"

#include "cloud.h"
#include "coord.h"
#include "env.h"
#include "fprop.h"
#include "los.h"
#include "player.h"
#include "random.h"
#include "state.h"
#include "terrain.h"

bool random_near_space(const coord_def& origin, coord_def& target,
                       bool allow_adjacent, bool restrict_los,
                       bool forbid_dangerous, bool forbid_sanctuary)
{
    // This might involve ray tracing (via num_feats_between()), so
    // cache results to avoid duplicating ray traces.
#define RNS_OFFSET 6
#define RNS_WIDTH (2*RNS_OFFSET + 1)
    FixedArray<bool, RNS_WIDTH, RNS_WIDTH> tried;
    const coord_def tried_o = coord_def(RNS_OFFSET, RNS_OFFSET);
    tried.init(false);

    // Is the monster on the other side of a transparent wall?
    const bool trans_wall_block  = trans_wall_blocking(origin);
    const bool origin_is_player  = (you.pos() == origin);
    int min_walls_between = 0;

    // Skip ray tracing if possible.
    if (trans_wall_block && !crawl_state.arena)
    {
        // XXX: you.pos() is invalid in the arena.
        min_walls_between = num_feats_between(origin, you.pos(),
                                              DNGN_CLEAR_ROCK_WALL,
                                              DNGN_CLEAR_PERMAROCK_WALL);
    }

    for (int tries = 0; tries < 150; tries++)
    {
        coord_def p = coord_def(random2(RNS_WIDTH), random2(RNS_WIDTH));
        if (tried(p))
            continue;
        else
            tried(p) = true;

        target = origin + (p - tried_o);

        // Origin is not 'near'.
        if (target == origin)
            continue;

        if (!in_bounds(target)
            || restrict_los && !you.see_cell(target)
            || grd(target) < DNGN_SHALLOW_WATER
            || actor_at(target)
            || !allow_adjacent && distance(origin, target) <= 2
            || forbid_sanctuary && is_sanctuary(target))
        {
            continue;
        }

        // Don't pick grids that contain a dangerous cloud.
        if (forbid_dangerous)
        {
            const int cloud = env.cgrid(target);

            if (cloud != EMPTY_CLOUD
                && is_damaging_cloud(env.cloud[cloud].type, true))
            {
                continue;
            }
        }

        if (!trans_wall_block && !origin_is_player)
            return (true);

        // If the monster is on a visible square which is on the other
        // side of one or more translucent walls from the player, then it
        // can only blink through translucent walls if the end point
        // is either not visible to the player, or there are at least
        // as many translucent walls between the player and the end
        // point as between the player and the start point.  However,
        // monsters can still blink through translucent walls to get
        // away from the player, since in the absence of translucent
        // walls monsters can blink to places which are not in either
        // the monster's nor the player's LOS.
        if (!origin_is_player && !you.see_cell(target))
            return (true);

        // Player can't randomly pass through translucent walls.
        if (origin_is_player)
        {
            if (you.see_cell_no_trans(target))
                return (true);

            continue;
        }

        int walls_passed = num_feats_between(target, origin,
                                             DNGN_CLEAR_ROCK_WALL,
                                             DNGN_CLEAR_PERMAROCK_WALL,
                                             true, true);
        if (walls_passed == 0)
            return (true);

        // Player can't randomly pass through translucent walls.
        if (origin_is_player)
            continue;

        int walls_between = 0;
        if (!crawl_state.arena)
            walls_between = num_feats_between(target, you.pos(),
                                              DNGN_CLEAR_ROCK_WALL,
                                              DNGN_CLEAR_PERMAROCK_WALL);

        if (walls_between >= min_walls_between)
            return (true);
    }

    return (false);
}


