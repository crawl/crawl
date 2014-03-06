/**
 * @file
 * @brief Functions related to teleportation and blinking.
**/

#include "AppHdr.h"

#include "teleport.h"

#include "cloud.h"
#include "coord.h"
#include "coordit.h"
#include "delay.h"
#include "env.h"
#include "fprop.h"
#include "los.h"
#include "losglobal.h"
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
    ASSERT(is_player());

    if (dest == pos())
        return false;

    if (no_tele(true, true, true))
    {
        if (!quiet)
            canned_msg(MSG_STRANGE_STASIS);
        return false;
    }

    if (!quiet)
        canned_msg(MSG_YOU_BLINK);

    stop_delay(true);

    const coord_def origin = pos();
    move_player_to_grid(dest, false, true);

    if (!cell_is_solid(origin))
        place_cloud(CLOUD_TLOC_ENERGY, origin, 1 + random2(3), this);

    return true;
}

bool monster::blink_to(const coord_def& dest, bool quiet)
{
    // For this call, let the monster choose whether it's blinking
    // or jumping. When blinked by another source, use the other
    // overload.
    return blink_to(dest, quiet, is_jumpy());
}

bool monster::blink_to(const coord_def& dest, bool quiet, bool jump)
{
    if (dest == pos())
        return false;

    bool was_constricted = false;
    const string verb = (jump ? "leap" : "blink");

    if (is_constricted())
    {
        was_constricted = true;

        if (!attempt_escape(2))
        {
            if (!quiet)
            {
                string message = " struggles to " + verb
                                 + " free from constriction.";
                simple_monster_message(this, message.c_str());
            }
            return false;
        }
    }

    if (!quiet)
    {
        string message = " " + conj_verb(verb)
                         + (was_constricted ? " free!" : "!");
        simple_monster_message(this, message.c_str());
    }

    if (!(flags & MF_WAS_IN_VIEW))
        seen_context = jump ? SC_LEAP_IN : SC_TELEPORT_IN;

    const coord_def oldplace = pos();
    if (!move_to_pos(dest, true))
        return false;

    // Leave a cloud.
    if (!cell_is_solid(oldplace))
    {
        place_cloud(jump ? CLOUD_DUST_TRAIL : CLOUD_TLOC_ENERGY,
                    oldplace, 1 + random2(3), this);
    }

    check_redraw(oldplace);
    apply_location_effects(oldplace);

    mons_relocated(this);

    return true;
}

typedef pair<coord_def, int> coord_weight;

// Try to find a "safe" place for moved close or far from the target.
// keep_los indicates that the destination should be in LOS of the target.
//
// XXX: Check the result against in_bounds(), not coord_def::origin(),
// because of a memory problem described below. (isn't this fixed now? -rob)
static coord_def random_space_weighted(actor* moved, actor* target,
                                       bool close, bool keep_los = true,
                                       bool allow_sanct = true)
{
    vector<coord_weight> dests;
    const coord_def tpos = target->pos();

    for (radius_iterator ri(moved->pos(), LOS_NO_TRANS); ri; ++ri)
    {
        if (!valid_blink_destination(moved, *ri, !allow_sanct)
            || (keep_los && !target->see_cell_no_trans(*ri)))
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
    return choice ? *choice : coord_def(0, 0);
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
    // If it's a monster, force them to "blink" rather than "jump"
    if (victim->is_monster())
        victim->as_monster()->blink_to(dest, false, false);
    else
        victim->blink_to(dest);
}

// Blink a monster away from the caster.
bool blink_away(monster* mon, actor* caster, bool from_seen, bool self_cast)
{
    if (from_seen && !mon->can_see(caster))
        return false;
    bool jumpy = self_cast && mon->is_jumpy();
    coord_def dest = random_space_weighted(mon, caster, false, false, true);
    if (dest.origin())
        return false;
    bool success = mon->blink_to(dest, false, jumpy);
    ASSERT(success || mon->is_constricted());
    return success;
}

// Blink the monster away from its foe.
bool blink_away(monster* mon, bool self_cast)
{
    actor* foe = mon->get_foe();
    if (!foe)
        return false;
    return blink_away(mon, foe, true, self_cast);
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
    ASSERT(success || mon->is_constricted());
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
    coord_def dest = random_space_weighted(mon, foe, true, true, true);
    if (dest.origin())
        return;
    bool success = mon->blink_to(dest, false);
    ASSERT(success || mon->is_constricted());
#ifndef DEBUG
    UNUSED(success);
#endif
}

// This only checks the contents of the tile - nothing in between.
// Could compact most of this into a big boolean if you wanted to trade
// readability for dubious speed improvements.
bool valid_blink_destination(const actor* moved, const coord_def& target,
                             bool forbid_sanctuary,
                             bool forbid_unhabitable)
{
    ASSERT(moved);

    if (!in_bounds(target))
        return false;
    if (actor_at(target))
        return false;
    if (forbid_unhabitable && !moved->is_habitable(target))
        return false;
    if (forbid_sanctuary && is_sanctuary(target))
        return false;
    if (!moved->see_cell_no_trans(target))
        return false;

    return true;
}

bool random_near_space(const actor* victim,
                       const coord_def& origin, coord_def& target,
                       bool allow_adjacent, bool forbid_sanctuary,
                       bool forbid_unhabitable)
{
    // This might involve ray tracing (LOS calcs in valid_blink_destination),
    // so cache results to avoid duplicating ray traces.
#define RNS_OFFSET 6
#define RNS_WIDTH (2*RNS_OFFSET + 1)
    FixedArray<bool, RNS_WIDTH, RNS_WIDTH> tried;
    const coord_def tried_o = coord_def(RNS_OFFSET, RNS_OFFSET);
    tried.init(false);

    for (int tries = 0; tries < 150; tries++)
    {
        coord_def p = coord_def(random2(RNS_WIDTH), random2(RNS_WIDTH));
        if (tried(p))
            continue;
        else
            tried(p) = true;

        target = origin + (p - tried_o);

        if (valid_blink_destination(victim, target,
                                    forbid_sanctuary, forbid_unhabitable)
            && (allow_adjacent || distance2(origin, target) > 2))
        {
            return true;
        }
    }

    return false;
}
