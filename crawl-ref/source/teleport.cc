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
#include "libutil.h"
#include "losglobal.h"
#include "message.h"
#include "mon-behv.h"
#include "mon-death.h"
#include "mon-place.h"
#include "mon-tentacle.h"
#include "random.h"
#include "terrain.h"
#include "view.h"

bool player::blink_to(const coord_def& dest, bool quiet)
{
    // We rely on the non-generalized move_player_to_cell.
    ASSERT(is_player());

    if (dest == pos())
        return false;

    if (no_tele(true))
    {
        if (!quiet)
            canned_msg(MSG_STRANGE_STASIS);
        return false;
    }

    if (!quiet)
        canned_msg(MSG_YOU_BLINK);

    stop_delay(true);

    const coord_def origin = pos();
    move_player_to_grid(dest, false);

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
    const string verb = (jump ? mons_genus(type) == MONS_FROG ? "hop" : "leap" : "blink");

    if (is_constricted())
    {
        was_constricted = true;
        stop_being_constricted(false, "blinks");
    }

    if (!quiet)
    {
        string message = " " + conj_verb(verb)
                         + (was_constricted ? " free!" : "!");
        simple_monster_message(*this, message.c_str());
    }

    if (!(flags & MF_WAS_IN_VIEW))
        seen_context = jump ? SC_LEAP_IN : SC_TELEPORT_IN;

    const coord_def oldplace = pos();
    if (!move_to_pos(dest, true))
        return false;

    // Leave a cloud.
    if (!props.exists(FAKE_BLINK_KEY) && !cell_is_solid(oldplace))
    {
        place_cloud(jump ? CLOUD_DUST : CLOUD_TLOC_ENERGY,
                    oldplace, 1 + random2(3), this);
    }

    check_redraw(oldplace);
    apply_location_effects(oldplace);

    mons_relocated(this);

    return true;
}

// If the returned value is mon.pos(), then nothing was found.
static coord_def _random_monster_nearby_habitable_space(const monster& mon)
{
    const bool respect_sanctuary = mon.wont_attack();

    coord_def target;
    int tries;

    for (tries = 0; tries < 150; ++tries)
    {
        coord_def delta;
        delta.x = random2(13) - 6;
        delta.y = random2(13) - 6;

        // Check that we don't get something too close to the
        // starting point.
        if (delta.origin())
            continue;

        // Blinks by 1 cell are not allowed.
        if (delta.rdist() == 1)
            continue;

        // Update target.
        target = delta + mon.pos();

        // Check that the target is valid and survivable.
        if (!in_bounds(target))
            continue;

        if (!monster_habitable_grid(&mon, target))
            continue;

        if (respect_sanctuary && is_sanctuary(target))
            continue;

        if (target == you.pos())
            continue;

        if (!cell_see_cell(mon.pos(), target, LOS_NO_TRANS))
            continue;

        // Survived everything, break out (with a good value of target.)
        break;
    }

    if (tries == 150)
        target = mon.pos();

    return target;
}

bool monster_blink(monster* mons, bool ignore_stasis, bool quiet)
{
    if (!ignore_stasis && mons->no_tele())
        return false;

    coord_def near = _random_monster_nearby_habitable_space(*mons);
    return mons->blink_to(near, quiet);
}

bool monster_space_valid(const monster* mons, coord_def target,
                         bool forbid_sanctuary)
{
    if (!in_bounds(target))
        return false;

    // Don't land on top of another monster.
    if (actor_at(target))
        return false;

    if (is_sanctuary(target) && forbid_sanctuary)
        return false;

    if (testbits(env.pgrid(target), FPROP_NO_TELE_INTO))
        return false;

    return monster_habitable_grid(mons, target);
}

static bool _monster_random_space(const monster* mons, coord_def& target,
                                  bool forbid_sanctuary, bool away_from_player)
{
    int tries = 0;
    while (tries++ < 1000)
    {
        target = random_in_bounds();
        if (monster_space_valid(mons, target, forbid_sanctuary)
            && (!away_from_player || !you.see_cell_no_trans(target)))
        {
            return true;
        }
    }

    return false;
}

void mons_relocated(monster* mons)
{
    if (mons_is_tentacle_head(mons_base_type(*mons)))
        destroy_tentacles(mons); // If the main body teleports get rid of the tentacles
    else if (mons->is_child_monster())
        destroy_tentacle(mons); // If a tentacle/segment is relocated just kill the tentacle
    else if (mons->type == MONS_ELDRITCH_TENTACLE
             || mons->type == MONS_ELDRITCH_TENTACLE_SEGMENT)
    {
        // Kill an eldritch tentacle and all its segments.
        monster* tentacle = mons->type == MONS_ELDRITCH_TENTACLE
                            ? mons : monster_by_mid(mons->tentacle_connect);

        // this should take care of any tentacles
        monster_die(*tentacle, KILL_RESET, -1, true);
    }
}

void monster_teleport(monster* mons, bool instan, bool silent, bool away_from_player,
                      const actor* agent)
{
    ASSERT(mons); // XXX: change to monster &mons
    bool was_seen = !silent && you.can_see(*mons);

    if (!instan)
    {
        if (mons->del_ench(ENCH_TP))
        {
            if (!silent)
                simple_monster_message(*mons, " seems more stable.");
        }
        else
        {
            if (!silent)
                simple_monster_message(*mons, " looks slightly unstable.");

            mons->add_ench(mon_enchant(ENCH_TP, 0, agent,
                                       random_range(20, 30)));
        }

        return;
    }

    coord_def newpos;

    if (!_monster_random_space(mons, newpos, !mons->wont_attack(), away_from_player))
    {
        // If we're trying to teleport away from the player and failed, try once
        // more without that restriction before giving up.
        if (!away_from_player
            || !_monster_random_space(mons, newpos, !mons->wont_attack(), false))
        {
            simple_monster_message(*mons, " flickers for a moment.");
            return;
        }
    }

    if (!silent)
        simple_monster_message(*mons, " disappears!");

    const coord_def oldplace = mons->pos();

    // Move it to its new home.
    mons->move_to_pos(newpos);

    const bool now_visible = you.see_cell(newpos);
    if (!silent && now_visible)
    {
        if (was_seen)
            simple_monster_message(*mons, " reappears nearby!");
        else
        {
            // Even if it doesn't interrupt an activity (the player isn't
            // delayed, the monster isn't hostile) we still want to give
            // a message.
            activity_interrupt_data ai(mons, SC_TELEPORT_IN);
            if (!interrupt_activity(activity_interrupt::see_monster, ai))
                simple_monster_message(*mons, " appears out of thin air!");
        }
    }

    if (mons->visible_to(&you) && now_visible)
        handle_seen_interrupt(mons);

    // Leave a purple cloud.
    // XXX: If silent is true, this is not an actual teleport, but
    //      the game moving a monster out of the way.
    if (!silent && !cell_is_solid(oldplace))
        place_cloud(CLOUD_TLOC_ENERGY, oldplace, 1 + random2(3), mons);

    mons->check_redraw(oldplace);
    mons->apply_location_effects(oldplace);

    mons_relocated(mons);

    shake_off_monsters(mons);
}

// Try to find a "safe" place for moved close or far from the target.
// keep_los indicates that the destination should be in LOS of the target.
//
// XXX: Check the result against in_bounds(), not coord_def::origin(),
// because of a memory problem described below. (isn't this fixed now? -rob)
static coord_def random_space_weighted(actor* moved, actor* target,
                                       bool close, bool keep_los = true,
                                       bool allow_sanct = true,
                                       int dist_max = LOS_RADIUS)
{
    vector<coord_weight> dests;
    const coord_def tpos = target->pos();

    int max_weight = 0;

    // Calculate weight of our starting pos, so we can compare if the weights of
    // other positions will improve it.
    int stay_weight;
    int stay_dist = (tpos - moved->pos()).rdist();
    if (close)
        stay_weight = (LOS_RADIUS - stay_dist) * (LOS_RADIUS - stay_dist);
    else
        stay_weight = stay_dist;
    if (stay_weight < 0)
        stay_weight = 0;

    for (radius_iterator ri(moved->pos(), dist_max, C_SQUARE, LOS_NO_TRANS); ri; ++ri)
    {
        if (!valid_blink_destination(*moved, *ri, !allow_sanct)
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

        if (weight > max_weight)
            max_weight = weight;

        dests.emplace_back(*ri, weight);
    }

    // Prevent choosing spots below a minimum weight threshold, so long as at
    // least one spot exists above that threshold. (Mostly to make sure that
    // blink away does actually move things away, if it's possible to do so)
    if (max_weight > stay_weight)
    {
        for (unsigned int i = 0; i < dests.size(); ++i)
        {
            if (dests[i].second <= stay_weight)
                dests[i].second = 0;
        }
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

// Blink the player away from a given monster
bool blink_player_away(monster* caster)
{
    coord_def dest = random_space_weighted(&you, caster, false, false, true);
    if (dest.origin())
        return false;
    bool success = you.blink_to(dest, false);
    ASSERT(success || you.is_constricted());
    return success;
}

// Blink a monster away from the caster.
bool blink_away(monster* mon, actor* caster, bool from_seen, bool self_cast,
                int max_dist)
{
    ASSERT(mon); // XXX: change to monster &mon
    ASSERT(caster); // XXX: change to actor &caster

    if (from_seen && !mon->can_see(*caster))
        return false;
    bool jumpy = self_cast && mon->is_jumpy();
    coord_def dest = random_space_weighted(mon, caster, false, false, true, max_dist);
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
void blink_range(monster &mon)
{
    actor* foe = mon.get_foe();
    if (!foe || !mon.can_see(*foe))
        return;
    coord_def dest = random_space_weighted(&mon, foe, false, true);
    if (dest.origin())
        return;
    bool success = mon.blink_to(dest);
    ASSERT(success || mon.is_constricted());
#ifndef ASSERTS
    UNUSED(success);
#endif
}

// Blink the monster close to its foe.
void blink_close(monster &mon)
{
    actor* foe = mon.get_foe();
    if (!foe || !mon.can_see(*foe))
        return;
    coord_def dest = random_space_weighted(&mon, foe, true, true, true);
    if (dest.origin())
        return;
    bool success = mon.blink_to(dest, false);
    ASSERT(success || mon.is_constricted());
#ifndef ASSERTS
    UNUSED(success);
#endif
}

// This only checks the contents of the tile - nothing in between.
// Could compact most of this into a big boolean if you wanted to trade
// readability for dubious speed improvements.
bool valid_blink_destination(const actor &moved, const coord_def& target,
                             bool forbid_sanctuary,
                             bool forbid_unhabitable,
                             bool incl_unseen)
{
    if (!in_bounds(target))
        return false;
    actor *targ_act = actor_at(target);
    if (targ_act
        && (incl_unseen || moved.can_see(*targ_act)
            || env.map_knowledge(targ_act->pos()).invisible_monster()))
    {
        return false;
    }
    if (forbid_unhabitable)
    {
        if (!moved.is_habitable(target))
            return false;
        if (moved.is_player() && is_feat_dangerous(env.grid(target), true))
            return false;
    }
    if (forbid_sanctuary && is_sanctuary(target))
        return false;
    if (!moved.see_cell_no_trans(target))
        return false;

    return true;
}

// This is for purposes of the targeter UI only, and will include destinations
// that contain invisible monsters which the player cannot see (even though
// a real blink will never move them there).
vector<coord_def> find_blink_targets()
{
    vector<coord_def> result;
    for (radius_iterator ri(you.pos(), LOS_NO_TRANS); ri; ++ri)
        if (valid_blink_destination(you, *ri, false, true, false))
            result.push_back(*ri);

    return result;
}

bool random_near_space(const actor* victim,
                       const coord_def& origin, coord_def& target,
                       bool allow_adjacent, int max_distance,
                       bool forbid_sanctuary,
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
        coord_def p;
        p.x = random2(RNS_WIDTH);
        p.y = random2(RNS_WIDTH);
        if (tried(p))
            continue;
        else
            tried(p) = true;

        target = origin + (p - tried_o);

        if (grid_distance(origin, target) <= max_distance
            && valid_blink_destination(*victim, target,
                                       forbid_sanctuary, forbid_unhabitable)
            && (allow_adjacent || grid_distance(origin, target) > 1))
        {
            return true;
        }
    }

    return false;
}
