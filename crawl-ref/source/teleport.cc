/*
 * File:     teleport.cc
 * Summary:  Functions related to teleportation and blinking.
 */

#include "AppHdr.h"

#include "teleport.h"

#include "cloud.h"
#include "coord.h"
#include "coordit.h"
#include "env.h"
#include "fprop.h"
#include "item_use.h"
#include "los.h"
#include "monster.h"
#include "mon-stuff.h"
#include "player.h"
#include "random.h"
#include "random-weight.h"
#include "state.h"
#include "stuff.h"
#include "terrain.h"

bool player::blink_to(const coord_def& dest, bool quiet)
{
    // We rely on the non-generalized move_player_to_cell.
    ASSERT(this == &you);

    if (dest == pos())
        return (false);

    if (item_blocks_teleport(true, true))
    {
        if (!quiet)
            canned_msg(MSG_STRANGE_STASIS);
        return (false);
    }

    if (!quiet)
        canned_msg(MSG_YOU_BLINK);

    const coord_def origin = pos();
    move_player_to_grid(dest, false, true);

    place_cloud(CLOUD_TLOC_ENERGY, origin, 1 + random2(3), KC_YOU);

    return (true);
}

bool monster::blink_to(const coord_def& dest, bool quiet)
{
    if (dest == pos())
        return (false);

    const bool jump = type == MONS_JUMPING_SPIDER;

    if (!quiet)
        simple_monster_message(this, jump ? " leaps!" : " blinks!");

    if (!(flags & MF_WAS_IN_VIEW))
        seen_context = "thin air";

    const coord_def oldplace = pos();
    if (!move_to_pos(dest))
        return (false);

    // Leave a purple cloud.
    if (!jump)
        place_cloud(CLOUD_TLOC_ENERGY, oldplace, 1 + random2(3),
                    kill_alignment());

    check_redraw(oldplace);
    apply_location_effects(oldplace);

    mons_relocated(this);

    return (true);
}


typedef std::pair<coord_def, int> coord_weight;

// Try to find a "safe" place for moved close or far from the target.
// keep_los indicates that the destination should be in view of the target.
//
// XXX: Check the result against in_bounds(), not coord_def::origin(),
// beceause of a memory problem described below. (isn't this fixed now? -rob)
static coord_def random_space_weighted(actor* moved, actor* target,
                                       bool close, bool keep_los = true,
                                       bool allow_sanct = true)
{
    std::vector<coord_weight> dests;
    const coord_def tpos = target->pos();

    for (radius_iterator ri(moved->get_los_no_trans()); ri; ++ri)
    {
        if (!moved->is_habitable(*ri) || actor_at(*ri)
            || keep_los && !target->see_cell_no_trans(*ri)
            || !allow_sanct && is_sanctuary(*ri))
        {
            continue;
        }
        int weight;
        int dist = (tpos - *ri).rdist();
        if (close)
            weight = (LOS_RADIUS - dist) * (LOS_RADIUS - dist);
        else
            weight = dist;
        if (weight < 0)
            weight = 0;
        dests.push_back(coord_weight(*ri, weight));
    }

    coord_def* choice = random_choose_weighted(dests);
    return (choice ? *choice : coord_def(0, 0));
}

// Blink the victim closer to the monster at target.
void blink_other_close(actor* victim, const coord_def &target)
{
    actor* caster = actor_at(target);
    if (!caster)
        return;
    if (is_sanctuary(you.pos()))
        return;
    coord_def dest = random_space_weighted(victim, caster, true);
    if (!in_bounds(dest))
        return;
    victim->blink_to(dest);
}

// Blink the monster away from its foe.
void blink_away(monster* mon)
{
    actor* foe = mon->get_foe();
    if (!foe || !mon->can_see(foe))
        return;
    coord_def dest = random_space_weighted(mon, foe, false, false);
    if (dest.origin())
        return;
    bool success = mon->blink_to(dest);
    ASSERT(success);
#ifndef DEBUG
    UNUSED(success);
#endif
}

// Blink the monster within range but at distance to its foe.
void blink_range(monster* mon)
{
    actor* foe = mon->get_foe();
    if (!foe || !mon->can_see(foe))
        return;
    coord_def dest = random_space_weighted(mon, foe, false, true);
    if (dest.origin())
        return;
    bool success = mon->blink_to(dest);
    ASSERT(success);
#ifndef DEBUG
    UNUSED(success);
#endif
}

// Blink the monster close to its foe.
void blink_close(monster* mon)
{
    actor* foe = mon->get_foe();
    if (!foe || !mon->can_see(foe))
        return;
    coord_def dest = random_space_weighted(mon, foe, true);
    if (dest.origin())
        return;
    bool success = mon->blink_to(dest);
    ASSERT(success);
#ifndef DEBUG
    UNUSED(success);
#endif
}

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
    const bool trans_wall_block  = you.trans_wall_blocking(origin);
    const bool origin_is_player  = (you.pos() == origin);
    int min_walls_between = 0;

    // Skip ray tracing if possible.
    if (trans_wall_block && !crawl_state.game_is_arena())
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
        if (!crawl_state.game_is_arena())
        {
            walls_between = num_feats_between(target, you.pos(),
                                              DNGN_CLEAR_ROCK_WALL,
                                              DNGN_CLEAR_PERMAROCK_WALL);
        }

        if (walls_between >= min_walls_between)
            return (true);
    }

    return (false);
}
