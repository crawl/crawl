/**
 * @file
 * @brief Translocation spells.
**/

#include "AppHdr.h"

#include "spl-transloc.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "abyss.h"
#include "act-iter.h"
#include "areas.h"
#include "art-enum.h"
#include "artefact.h"
#include "cloud.h"
#include "coordit.h"
#include "delay.h"
#include "directn.h"
#include "dungeon.h"
#include "english.h"
#include "god-abil.h" // fedhas_passthrough for armataur charge
#include "god-conduct.h"
#include "item-prop.h"
#include "items.h"
#include "level-state-type.h"
#include "libutil.h"
#include "los.h"
#include "losglobal.h"
#include "losparam.h"
#include "melee-attack.h" // armataur charge
#include "message.h"
#include "mon-behv.h"
#include "mon-death.h"
#include "mon-place.h"
#include "mon-tentacle.h"
#include "mon-util.h"
#include "movement.h" // armataur charge
#include "nearby-danger.h"
#include "orb.h"
#include "output.h"
#include "prompt.h"
#include "religion.h"
#include "shout.h"
#include "spl-damage.h" // cancel_polar_vortex
#include "spl-monench.h"
#include "spl-util.h"
#include "stash.h"
#include "state.h"
#include "stringutil.h"
#include "target.h"
#include "teleport.h"
#include "terrain.h"
#include "tiledoll.h"
#ifdef USE_TILE
#include "tilepick.h"
#endif
#include "traps.h"
#include "view.h"
#include "viewmap.h"
#include "xom.h"

/**
 * Place a cloud of translocational energy at a player's previous location,
 * to make it easier for players to tell what just happened.
 *
 * @param origin    The player's previous location.
 */
static void _place_tloc_cloud(const coord_def &origin)
{
    if (!cell_is_solid(origin))
        place_cloud(CLOUD_TLOC_ENERGY, origin, 1 + random2(3), &you);
}

spret cast_disjunction(int pow, bool fail)
{
    fail_check();
    int rand = random_range(35, 45) + random2(div_rand_round(pow, 12));
    you.duration[DUR_DISJUNCTION] = min(90 + div_rand_round(pow, 12),
        max(you.duration[DUR_DISJUNCTION] + rand,
        30 + rand));
    disjunction_spell();
    return spret::success;
}

void disjunction_spell()
{
    int steps = you.time_taken;
    invalidate_agrid(true);
    for (int step = 0; step < steps; ++step)
    {
        vector<monster*> mvec;
        for (radius_iterator ri(you.pos(), LOS_RADIUS, C_SQUARE); ri; ++ri)
        {
            monster* mons = monster_at(*ri);
            if (!mons || !you.see_cell(*ri))
                continue;
            mvec.push_back(mons);
        }
        if (mvec.empty())
            return;
        // blink should be isotropic
        shuffle_array(mvec);
        for (auto mons : mvec)
        {
            if (!mons->alive() || mons->no_tele())
                continue;
            coord_def p = mons->pos();
            if (!disjunction_haloed(p))
                continue;

            int dist = grid_distance(you.pos(), p);
            int decay = max(1, (dist - 1) * (dist + 1));
            int chance = pow(0.8, 1.0 / decay) * 1000;
            if (!x_chance_in_y(chance, 1000))
                blink_away(mons, &you, false);
        }
    }
}

/**
 * Attempt to blink the player to a random nearby tile.
 *
 * @param override_stasis       Whether to blink even if the player is under
 *                              stasis (& thus normally unable to).
 * @param max_distance          The maximum distance we are allowed to blink.
 */
void uncontrolled_blink(bool override_stasis, int max_distance)
{
    if (you.no_tele(true) && !override_stasis)
    {
        canned_msg(MSG_STRANGE_STASIS);
        return;
    }

    coord_def target;
    // First try to find a random square not adjacent to the player,
    // then one adjacent if that fails.
    if (!random_near_space(&you, you.pos(), target, false, max_distance)
        && !random_near_space(&you, you.pos(), target, true, max_distance))
    {
        mpr("You feel jittery for a moment.");
        return;
    }

    you.stop_being_constricted(false, "blink");

    canned_msg(MSG_YOU_BLINK);
    const coord_def origin = you.pos();
    move_player_to_grid(target, false);
    _place_tloc_cloud(origin);

    crawl_state.potential_pursuers.clear();
}

/**
 * Let the player choose a destination for their controlled blink or similar
 * effect.
 *
 * @param target[out]   The target found, if any.
 * @param safe_cancel   Whether it's OK to let the player cancel the control
 *                      of the blink (or whether there should be a prompt -
 *                      for e.g. read-identified ?blink)
 * @param verb          What kind of movement is this, exactly?
 *                      (E.g. 'blink', 'hop'.)
 * @param hitfunc       A hitfunc passed to the direction_chooser.
 * @return              True if a target was found; false if the player aborted.
 */
static bool _find_cblink_target(dist &target, bool safe_cancel,
                                string verb, targeter *hitfunc = nullptr, bool physical = false)
{
    while (true)
    {
        // query for location {dlb}:
        direction_chooser_args args;
        args.restricts = DIR_TARGET;
        args.needs_path = false;
        args.top_prompt = uppercase_first(verb) + " to where?";
        args.hitfunc = hitfunc;
        direction(target, args);

        if (crawl_state.seen_hups)
        {
            mprf("Cancelling %s due to HUP.", verb.c_str());
            return false;
        }

        if (!target.isValid || target.target == you.pos())
        {
            const string prompt =
                "Are you sure you want to cancel this " + verb + "?";
            if (!safe_cancel && !yesno(prompt.c_str(), false, 'n'))
            {
                clear_messages();
                continue;
            }

            canned_msg(MSG_OK);
            return false;
        }

        const monster* beholder = you.get_beholder(target.target);
        if (beholder)
        {
            mprf("You cannot %s away from %s!",
                 verb.c_str(),
                 beholder->name(DESC_THE, true).c_str());
            continue;
        }

        const monster* fearmonger = you.get_fearmonger(target.target);
        if (fearmonger)
        {
            mprf("You cannot %s closer to %s!",
                 verb.c_str(),
                 fearmonger->name(DESC_THE, true).c_str());
            continue;
        }

        if (cell_is_solid(target.target))
        {
            clear_messages();
            mprf("You can't %s into that!", verb.c_str());
            continue;
        }

        monster* target_mons = monster_at(target.target);
        if (target_mons && you.can_see(*target_mons))
        {
            mprf("You can't %s onto %s!", verb.c_str(),
                 target_mons->name(DESC_THE).c_str());
            continue;
        }

        if (!check_moveto(target.target, verb, false))
        {
            continue;
            // try again (messages handled by check_moveto)
        }

        if (!you.see_cell_no_trans(target.target))
        {
            clear_messages();
            if (you.trans_wall_blocking(target.target))
                canned_msg(MSG_SOMETHING_IN_WAY);
            else
                canned_msg(MSG_CANNOT_SEE);
            continue;
        }

        if (cancel_harmful_move(physical))
        {
            clear_messages();
            continue;
        }

        return true;
    }
}

void wizard_blink()
{
    // query for location {dlb}:
    direction_chooser_args args;
    args.restricts = DIR_TARGET;
    args.needs_path = false;
    targeter_smite tgt(&you, LOS_RADIUS);
    tgt.obeys_mesmerise = false;
    args.hitfunc = &tgt;

    args.top_prompt = "Blink to where?";
    dist beam;
    direction(beam, args);

    if (!beam.isValid || beam.target == you.pos())
    {
        canned_msg(MSG_OK);
        return;
    }

    if (!in_bounds(beam.target))
    {
        clear_messages();
        mpr("Please don't blink into the map border.");
        return wizard_blink();
    }

    if (monster_at(beam.target))
    {
        clear_messages();
        mpr("Please don't try to blink into monsters.");
        return wizard_blink();
    }

    if (!check_moveto(beam.target, "blink", false))
    {
        return wizard_blink();
        // try again (messages handled by check_moveto)
    }

    // Allow wizard blink to send player into walls, in case the
    // user wants to alter that grid to something else.
    if (cell_is_solid(beam.target))
        env.grid(beam.target) = DNGN_FLOOR;

    move_player_to_grid(beam.target, false);
}

static const int HOP_FUZZ_RADIUS = 2;

class targeter_hop : public targeter_smite
{
private:
    bool incl_unseen;
public:
    targeter_hop(int hop_range, bool include_unseen)
        : targeter_smite(&you, hop_range, 0, HOP_FUZZ_RADIUS, false),
          incl_unseen(include_unseen)
    {
        ASSERT(agent);
        obeys_mesmerise = true;
    }

    aff_type is_affected(coord_def p) override
    {
        if (!valid_aim(aim))
            return AFF_NO;

        if (is_feat_dangerous(env.grid(p), true))
            return AFF_NO; // XX is this handled by the valid blink check?

        const actor* p_act = actor_at(p);
        if (p_act && (incl_unseen || agent->can_see(*p_act)))
            return AFF_NO;

        // terrain details are cached in exp_map_max by set_aim
        return targeter_smite::is_affected(p);
    }

    bool set_aim(coord_def a) override
    {
        if (!targeter::set_aim(a))
            return false;

        // targeter_smite works by filling the explosion map. Here we fill just
        // the max explosion map leading to AFF_MAYBE for possible hop targets.
        exp_map_min.init(INT_MAX);
        exp_map_max.init(INT_MAX);
        // somewhat magical value for centre that I have copied from elsewhere
        const coord_def centre(9,9);
        for (radius_iterator ri(a, exp_range_max, C_SQUARE, LOS_NO_TRANS);
             ri; ++ri)
        {
            if (valid_blink_destination(*agent, *ri, false, true, incl_unseen))
                exp_map_max(*ri - a + centre) = 1;
        }
        return true;
    }
};

/**
 * Randomly choose one of the spaces near the given target for the player's hop
 * to land on.
 *
 * @param target    The tile the player wants to land on.
 * @return          A nearby, unoccupied, inhabitable tile.
 */
static coord_def _fuzz_hop_destination(coord_def target)
{
    coord_def chosen;
    int seen = 0;
    targeter_hop tgt(frog_hop_range(), true);
    tgt.set_aim(target); // XX could reuse tgt from the calling function?
    for (auto ti = tgt.affected_iterator(AFF_MAYBE); ti; ++ti)
        if (one_chance_in(++seen))
            chosen = *ti;

    return chosen;
}

int frog_hop_range()
{
    return 2 + you.get_mutation_level(MUT_HOP) * 2; // 4-6
}

/**
 * Attempt to hop the player to a space near a tile of their choosing.
 *
 * @param fail          Whether this came from a mis-invoked ability (& should
 *                      therefore fail after selecting a target)
 * @return              Whether the hop succeeded, aborted, or was miscast.
 */
spret frog_hop(bool fail, dist *target)
{
    dist empty;
    if (!target)
        target = &empty; // XX just convert some of these fn signatures to take dist &
    const int hop_range = frog_hop_range();
    targeter_hop tgt(hop_range, false);

    while (true)
    {
        if (!_find_cblink_target(*target, true, "hop", &tgt, true))
            return spret::abort;

        if (grid_distance(you.pos(), target->target) > hop_range)
        {
            mpr("That's out of range!"); // ! targeting
            continue;
        }
        break;
    }
    target->target = _fuzz_hop_destination(target->target);

    fail_check();

    you.stop_being_constricted(false, "hop");

    // invisible monster that the targeter didn't know to avoid, or similar
    if (target->target.origin())
    {
        mpr("You tried to hop, but there was no room to land!");
        // TODO: what to do here?
        return spret::success; // of a sort
    }

    if (!cell_is_solid(you.pos())) // should be safe.....
        place_cloud(CLOUD_DUST, you.pos(), 2 + random2(3), &you);
    move_player_to_grid(target->target, false);
    crawl_state.cancel_cmd_again();
    crawl_state.cancel_cmd_repeat();
    mpr("Boing!");
    you.increase_duration(DUR_NO_HOP, 12 + random2(13));
    player_did_deliberate_movement();

    return spret::success; // TODO
}

string electric_charge_impossible_reason(bool allow_safe_monsters)
{
    // General movement checks are handled elsewhere.
    int nearby_mons = 0;
    string example_reason = "";
    string fail_reason;
    for (monster_near_iterator mi(&you); mi; ++mi)
    {
        ++nearby_mons;
        if (get_electric_charge_landing_spot(you, mi->pos(), &fail_reason).origin())
        {
            example_reason = make_stringf("you can't charge at %s because %s",
                                          mi->name(DESC_THE).c_str(),
                                          fail_reason.c_str());
        }
        else if (allow_safe_monsters
                 || !mons_is_safe(*mi, false)
                 || mons_class_is_test(mi->type))
        {
            return "";
        }
    }
    if (!nearby_mons)
        return "you can't see anything to charge at.";
    if (nearby_mons == 1)
        return lowercase_string(example_reason);
    return "there's one issue or another keeping you from charging at any nearby foe.";
}

string movement_impossible_reason()
{
    if (you.attribute[ATTR_HELD])
        return make_stringf("You cannot do that while %s.", held_status());
    if (!you.is_motile())
        return "You cannot move."; // MSG_CANNOT_MOVE
    return "";
}

bool valid_electric_charge_target(const actor& agent, coord_def target, string* fail_reason)
{
    string msg;

    const actor* act = actor_at(target);

    // Target must be in range and non-adjacent
    if (agent.pos() == target)
    {
        if (fail_reason)
            *fail_reason = "You can't charge at yourself.";

        return false;
    }
    else if (adjacent(agent.pos(), target))
    {
        if (fail_reason)
            *fail_reason = "You're already next to there.";

        return false;
    }
    else if (grid_distance(agent.pos(), target)
             > spell_range(SPELL_ELECTRIC_CHARGE, 50))
    {
        if (fail_reason)
            *fail_reason = "That's out of range!";

        return false;
    }

    // No charging at things the caster cannot see.
    if (!act || !agent.can_see(*act)
        || agent.is_player() && act->is_monster()
           && fedhas_passthrough(act->as_monster()))
    {
        if (fail_reason)
            *fail_reason = "You can't see anything there to charge at.";

        return false;
    }

    // No charging at friends or firewood.
    if (mons_aligned(act, &agent) || act->is_firewood())
    {
        if (fail_reason)
            *fail_reason = "Why would you want to do that?";

        return false;
    }

    // The remaining checks concern only the player.
    if (agent.is_monster())
        return true;

    const monster* beholder = you.get_beholder(target);
    if (beholder)
    {
        if (fail_reason)
        {
            *fail_reason = make_stringf("You cannot charge away from %s!",
                                        beholder->name(DESC_THE, true).c_str());
        }

        return false;
    }

    const monster* fearmonger = you.get_fearmonger(target);
    if (fearmonger)
    {
        if (fail_reason)
        {
            *fail_reason = make_stringf("You cannot charge closer to %s!",
                                        fearmonger->name(DESC_THE, true).c_str());
        }

        return false;
    }

    return true;
}

// Gets the tile the agent would land on if they tried to charge towards target.
// Returns (0, 0) if this charge is invalid for any reason.
// (fail_reason will get set to an appropriate error message)
coord_def get_electric_charge_landing_spot(const actor& agent, coord_def target,
                                           string* fail_reason)
{
    // Double-check that this is a valid thing to try to charge at at all
    if (!valid_electric_charge_target(agent, target, fail_reason))
        return coord_def(0, 0);

    ray_def ray;
    if (!find_ray(agent.pos(), target, ray, opc_solid))
    {
        if (fail_reason)
            *fail_reason = "There's something in the way.";

        return coord_def(0, 0);
    }

    const int dist_to_targ = grid_distance(agent.pos(), target);
    while (ray.advance())
    {
         // We've reached the spot immediately before our target, which should
        // be our landing spot (if it's valid)
        if (grid_distance(ray.pos(), agent.pos()) == dist_to_targ -1)
        {
            if (agent.is_player() ? is_feat_dangerous(env.grid(ray.pos()))
                                  : !monster_habitable_grid(agent.as_monster(), ray.pos()))
            {
                if (fail_reason)
                {
                    *fail_reason = "There's "
                                   + feature_description_at(ray.pos())
                                   + " in the way.";
                }

                return coord_def(0, 0);
            }

            const monster* mon = monster_at(ray.pos());
            if (mon && agent.can_see(*mon) && mons_class_is_stationary(mon->type))
            {
                if (fail_reason)
                {
                    *fail_reason = mon->name(DESC_THE)
                                   + " is immovably fixed in your path.";
                }

                return coord_def(0, 0);
            }

            // We've already verified that our target is okay, and now we know
            // that our landing spot is also. So we should be done here.
            return ray.pos();
        }
    }

    // Should be unreachable, but return a negative result anyway.
    return coord_def(0, 0);
}

// Tries to push any creature out of the way of an electric charge landing spot,
// using increasingly strong measures. Will usually keep trying until it
// succeeds in creating open space, but in a few situations (mostly involving
// the player being the one in the way) may fail (and return false)
static bool _displace_charge_blocker(actor& agent, coord_def pos)
{
    // Try 10 times to move the obstacle out of our way. It should only be the
    // most contrived and impossible of circumstances that require this, but
    // let's try not to crash just in case.
    for (int tries = 0; tries < 10; ++tries)
    {
        actor* blocker = actor_at(pos);
        if (!blocker)
            return true;

        const coord_def orig = blocker->pos();
        coord_def targ;
        if (random_near_space(blocker, blocker->pos(), targ, true)
            && blocker->blink_to(targ, true))
        {
            if (blocker->is_player())
                mpr("You are hurled out of the way!");

            continue;
        }

        // Don't teleport (or banish!) the player when something tries to Vhi's
        // something immediately behind them. In this case, just make the monster
        // abort.
        if (blocker->is_player())
            return false;

        monster* mon = blocker->as_monster();
        monster_teleport(mon, true);
        if (mon->pos() != orig)
            continue;

        mon->banish(&agent, "electric charge", -1, true);
        if (!mon->alive())
            continue;

        monster_die(*mon, KILL_BANISHED, NON_MONSTER);
    }

    // Against all odds, there's something still here.
    return false;
}

/**
 * Attempt to charge the player to a target of their choosing.
 *
 * @param fail          Whether this came from a miscast spell (& should
 *                      therefore fail after selecting a target)
 * @return              Whether the charge succeeded, aborted, or was miscast.
 */
spret electric_charge(actor& agent, int powc, bool fail, const coord_def &target)
{
    // Check for unholy weapons, breadswinging, etc
    if (agent.is_player() && !wielded_weapon_check(you.weapon(), "charge"))
        return spret::abort;

    coord_def dest_pos = get_electric_charge_landing_spot(agent, target);

    // Should be impossible, but bail out if there's no valid landing pos
    if (dest_pos.origin())
        return spret::abort;

    // Prompt to make sure the player really wants to attack the monster they're
    // charging at.
    monster* target_mon = monster_at(target);
    if (agent.is_player() && target_mon && stop_attack_prompt(target_mon, false, target))
        return spret::abort;

    // Test dangerous terrain at our destination
    if (agent.is_player() && !check_moveto(dest_pos, "charge"))
        return spret::abort;

    fail_check();

    // This should virtually never happen, since the spell is vetoed for stationary
    // blockers before this; this requires a stationary blocker who is also
    // invisible to the agent.
    monster* dest_mon = monster_at(dest_pos);
    const bool invalid_dest = dest_mon && mons_class_is_stationary(dest_mon->type);
    if (invalid_dest)
    {
        if (agent.is_player())
            mprf("%s is immovably fixed there.", dest_mon->name(DESC_THE).c_str());
        return spret::success;
    }

    if (!agent.attempt_escape()) // prints its own messages
        return spret::success;

    const coord_def orig_pos = agent.pos();
    actor* target_actor = actor_at(target);

    if (agent.is_player())
    {
        crawl_state.cancel_cmd_again();
        crawl_state.cancel_cmd_repeat();

        // Monster cast messages are handled through monspell.txt
        if (silenced(dest_pos))
            mpr("You charge forward in eerie silence!");
        else
            mpr("You charge forward with an electric crackle!");
    }

    // Trying to clear space at our destination by moving actors away from it.
    // Very rarely, this may fail. If it does, abort.
    if (!_displace_charge_blocker(agent, dest_pos))
    {
        if (agent.is_player() || (agent.is_monster() && you.can_see(agent)))
        {
            mprf("...but somehow remain%s in the same place.",
                                                agent.is_monster() ? "s" : "");
        }

        return spret::success;
    }

    // Actually move the agent
    const coord_def initial_pos = agent.pos();
    if (agent.is_player())
        move_player_to_grid(dest_pos, true);
    else
    {
        agent.move_to_pos(dest_pos);
        agent.apply_location_effects(orig_pos);
    }

    noisy(4, agent.pos());
    agent.did_deliberate_movement();
    agent.clear_far_engulf(false, true);

    // Draw a cloud trail behind the charging agent
    ray_def ray;
    if (find_ray(orig_pos, target, ray, opc_solid))
    {
        while (ray.advance() && ray.pos() != target)
        {
            if (!cell_is_solid(ray.pos()) &&
                (!agent.is_player() || !apply_cloud_trail(ray.pos())))
            {
                place_cloud(CLOUD_ELECTRICITY, ray.pos(), 2 + random2(3), &agent);
            }
        }
    }

    if (agent.pos() != dest_pos) // polar vortex and trap nonsense
        return spret::success; // of a sort

    // Maybe we hit a trap and something weird happened.
    if (!target_actor->alive() || !adjacent(agent.pos(), target_actor->pos()))
        return spret::success;

    // manually apply noise
    // this silence check feels kludgy - perhaps could check along the whole route..?
    if (!silenced(target) && target_actor->is_monster())
        behaviour_event(target_actor->as_monster(), ME_ALERT, &agent, agent.pos()); // shout + set you as foe

    // We got webbed/netted at the destination, bail on the attack.
    if (agent.is_player() && you.attribute[ATTR_HELD])
        return spret::success;
    // Todo: Be more comprehensive?
    else if (agent.is_monster() && agent.as_monster()->has_ench(ENCH_HELD))
        return spret::success;

    melee_attack charge_atk(&agent, target_actor);

    // Player Vhi's damage is based on spellpower, distance, and their melee damage
    if (agent.is_player())
        charge_atk.charge_pow = powc + 50 * grid_distance(initial_pos, agent.pos());
    // Monster Vhi's damage is flatly based on their HD
    else
        charge_atk.charge_pow = powc;

    charge_atk.launch_attack_set();

    // Monsters will already use up attack energy via the melee attack itself,
    // so we only need to handle delay for players.
    if (agent.is_player())
    {
        // Normally this is 10 aut (times haste, chei etc), but slow weapons
        // take longer. Most relevant for low-skill players and Dark Maul.
        you.time_taken = max(you.attack_delay().roll(), player_speed());
    }

    return spret::success;
}

/**
 * Attempt to blink the player to a nearby tile of their choosing. Doesn't
 * handle you.no_tele().
 *
 * @param safe_cancel   Whether it's OK to let the player cancel the control
 *                      of the blink (or whether there should be a prompt -
 *                      for e.g. read-identified ?blink)
 * @return              Whether the blink succeeded, aborted, or was miscast.
 */
spret controlled_blink(bool safe_cancel, dist *target)
{
    if (crawl_state.is_replaying_keys())
    {
        crawl_state.cancel_cmd_all("You can't repeat controlled blinks.");
        return spret::abort;
    }

    dist empty;
    if (!target)
        target = &empty;

    targeter_smite tgt(&you, LOS_RADIUS);
    tgt.obeys_mesmerise = true;
    if (!_find_cblink_target(*target, safe_cancel, "blink", &tgt))
        return spret::abort;

    // invisible monster that the targeter didn't know to avoid
    if (monster_at(target->target))
    {
        mpr("Oops! There was something there already!");
        uncontrolled_blink();
        return spret::success; // of a sort
    }

    you.stop_being_constricted(false, "blink");

    _place_tloc_cloud(you.pos());
    move_player_to_grid(target->target, false);

    crawl_state.cancel_cmd_again();
    crawl_state.cancel_cmd_repeat();

    return spret::success;
}

/**
 * Cast the player spell Blink.
 *
 * @param fail              Whether the player miscast the spell.
 * @return                  Whether the spell was successfully cast, aborted,
 *                          or miscast.
 */
spret cast_blink(int pow, bool fail)
{
    // effects that cast the spell through the player, I guess (e.g. xom)
    if (you.no_tele(true))
        return fail ? spret::fail : spret::success; // probably always SUCCESS

    if (cancel_harmful_move(false))
        return spret::abort;

    fail_check();
    uncontrolled_blink();

    you.increase_duration(DUR_BLINK_COOLDOWN,
                          2 + random2(3) + div_rand_round(50 - pow, 10));

    return spret::success;
}

void you_teleport()
{
    // [Cha] here we block teleportation, which will save the player from
    // death from read-id'ing scrolls (in sprint)
    if (you.no_tele())
        canned_msg(MSG_STRANGE_STASIS);
    else if (you.duration[DUR_TELEPORT])
    {
        mpr("You feel strangely stable.");
        you.duration[DUR_TELEPORT] = 0;
    }
    else
    {
        mpr("You feel strangely unstable.");

        int teleport_delay = 3 + random2(3);

        if (player_in_branch(BRANCH_ABYSS))
        {
            mpr("You feel the power of the Abyss delaying your translocation!");
            teleport_delay += 5 + random2(10);
        }
        else if (orb_limits_translocation())
        {
            mprf(MSGCH_ORB, "You feel the Orb delaying your translocation!");
            teleport_delay += 5 + random2(5);
        }

        you.set_duration(DUR_TELEPORT, teleport_delay);
    }
}

// Should return true if we don't want anyone to teleport here.
bool cell_vetoes_teleport(const coord_def cell, bool check_monsters,
                          bool wizard_tele)
{
    // Monsters always veto teleport.
    if (monster_at(cell) && check_monsters)
        return true;

    // As do all clouds; this may change.
    if (cloud_at(cell) && !wizard_tele)
        return true;

    if (cell_is_solid(cell))
        return true;

    return is_feat_dangerous(env.grid(cell), true) && !wizard_tele;
}

static void _handle_teleport_update(bool large_change, const coord_def old_pos)
{
    if (large_change)
    {
        viewwindow();
        update_screen();
        for (monster_iterator mi; mi; ++mi)
        {
            const bool see_cell = you.see_cell(mi->pos());

            if (mi->foe == MHITYOU && !see_cell && !you.penance[GOD_ASHENZARI])
            {
                mi->foe_memory = 0;
                behaviour_event(*mi, ME_EVAL);
            }
            else if (see_cell)
                behaviour_event(*mi, ME_EVAL);
        }
    }

#ifdef USE_TILE
    if (you.has_innate_mutation(MUT_MERTAIL))
    {
        const dungeon_feature_type new_grid = env.grid(you.pos());
        const dungeon_feature_type old_grid = env.grid(old_pos);
        if (feat_is_water(old_grid) && !feat_is_water(new_grid)
            || !feat_is_water(old_grid) && feat_is_water(new_grid))
        {
            init_player_doll();
        }
    }
#else
    UNUSED(old_pos);
#endif

    you.clear_far_engulf();
}

static bool _teleport_player(bool wizard_tele, bool teleportitis,
                             string reason="")
{
    if (!wizard_tele && !teleportitis
        && (crawl_state.game_is_sprint() || you.no_tele())
            && !player_in_branch(BRANCH_ABYSS))
    {
        if (!reason.empty())
            mpr(reason);
        canned_msg(MSG_STRANGE_STASIS);
        return false;
    }

    // After this point, we're guaranteed to teleport. Kill the appropriate
    // delays. Teleportitis needs to check the target square first, though.
    if (!teleportitis)
        interrupt_activity(activity_interrupt::teleport);

    // Update what we can see at the current location as well as its stash,
    // in case something happened in the exact turn that we teleported
    // (like picking up/dropping an item).
    viewwindow();
    update_screen();
    StashTrack.update_stash(you.pos());

    if (player_in_branch(BRANCH_ABYSS) && !wizard_tele)
    {
        if (!reason.empty())
            mpr(reason);
        abyss_teleport();
        if (you.pet_target != MHITYOU)
            you.pet_target = MHITNOT;

        return true;
    }

    coord_def pos(1, 0);
    const coord_def old_pos = you.pos();
    bool      large_change  = false;

    if (wizard_tele)
    {
        while (true)
        {
            level_pos lpos;
            bool chose = show_map(lpos, false, false);
            pos = lpos.pos;
            redraw_screen();
            update_screen();

            // If we've received a HUP signal then the user can't choose a
            // location, so cancel the teleport.
            if (crawl_state.seen_hups)
            {
                mprf(MSGCH_ERROR, "Controlled teleport interrupted by HUP signal, "
                                  "cancelling teleport.");
                return false;
            }

            dprf("Target square (%d,%d)", pos.x, pos.y);

            if (!chose || pos == you.pos())
                return false;

            break;
        }

        if (!you.see_cell(pos))
            large_change = true;

        if (cell_vetoes_teleport(pos, true, wizard_tele))
        {
            mprf(MSGCH_WARN, "Even you can't go there right now. Sorry!");
            return false;
        }
        else
            move_player_to_grid(pos, false);
    }
    else
    {
        coord_def newpos;
        int tries = 500;
        do
        {
            newpos = random_in_bounds();
        }
        while (--tries > 0
               && (cell_vetoes_teleport(newpos)
                   || testbits(env.pgrid(newpos), FPROP_NO_TELE_INTO)));

        // Running out of tries shouldn't happen; no message. Return false so
        // it doesn't count as a random teleport for Xom purposes.
        if (tries == 0)
            return false;
        // Teleportitis requires a monster in LOS of the new location, else
        // it silently fails.
        else if (teleportitis)
        {
            int mons_near_target = 0;
            for (monster_near_iterator mi(newpos, LOS_NO_TRANS); mi; ++mi)
                if (mons_is_threatening(**mi) && mons_attitude(**mi) == ATT_HOSTILE)
                    mons_near_target++;
            if (!mons_near_target)
            {
                dprf("teleportitis: no monster near target");
                return false;
            }
            else if (you.no_tele())
            {
                if (!reason.empty())
                    mpr(reason);
                canned_msg(MSG_STRANGE_STASIS);
                return false;
            }
            else
            {
                interrupt_activity(activity_interrupt::teleport);
                if (!reason.empty())
                    mpr(reason);
                mprf("You are yanked towards %s nearby monster%s!",
                     mons_near_target > 1 ? "some" : "a",
                     mons_near_target > 1 ? "s" : "");
            }
        }

        if (newpos == old_pos)
            mpr("Your surroundings flicker for a moment.");
        else if (you.see_cell(newpos))
            mpr("Your surroundings seem slightly different.");
        else
        {
            mpr("Your surroundings suddenly seem different.");
            large_change = true;
        }

        cancel_polar_vortex(true);
        // Leave a purple cloud.
        _place_tloc_cloud(old_pos);

        move_player_to_grid(newpos, false);
        stop_delay(true);
    }

    crawl_state.potential_pursuers.clear();

    _handle_teleport_update(large_change, old_pos);
    return !wizard_tele;
}

bool you_teleport_to(const coord_def where_to, bool move_monsters)
{
    // Attempts to teleport the player from their current location to 'where'.
    // Follows this line of reasoning:
    //   1. Check the location (against cell_vetoes_teleport), if valid,
    //      teleport the player there.
    //   2. If not because of a monster, and move_monster, teleport that
    //      monster out of the way, then teleport the player there.
    //   3. Otherwise, iterate over adjacent squares. If a sutiable position is
    //      found (or a monster can be moved out of the way, with move_monster)
    //      then teleport the player there.
    //   4. If not, give up and return false.

    const coord_def old_pos = you.pos();
    coord_def where = where_to;
    coord_def old_where = where_to;

    // Don't bother to calculate a possible new position if it's out of bounds.
    if (!in_bounds(where))
        return false;

    if (cell_vetoes_teleport(where))
    {
        if (monster_at(where) && move_monsters && !cell_vetoes_teleport(where, false))
        {
            // dlua only, don't heed no_tele
            monster* mons = monster_at(where);
            mons->teleport(true);
        }
        else
        {
            for (adjacent_iterator ai(where); ai; ++ai)
            {
                if (!cell_vetoes_teleport(*ai))
                {
                    where = *ai;
                    break;
                }
                else
                {
                    if (monster_at(*ai) && move_monsters
                            && !cell_vetoes_teleport(*ai, false))
                    {
                        monster* mons = monster_at(*ai);
                        mons->teleport(true);
                        where = *ai;
                        break;
                    }
                }
            }
            // Give up, we can't find a suitable spot.
            if (where == old_where)
                return false;
        }
    }

    // If we got this far, we're teleporting the player.
    _place_tloc_cloud(old_pos);

    bool large_change = you.see_cell(where);

    move_player_to_grid(where, false);

    _handle_teleport_update(large_change, old_pos);
    return true;
}

void you_teleport_now(bool wizard_tele, bool teleportitis, string reason)
{
    const bool randtele = _teleport_player(wizard_tele, teleportitis, reason);

    // Xom is amused by teleports that land you in a dangerous place, unless
    // the player is in the Abyss and teleported to escape from all the
    // monsters chasing him/her, since in that case the new dangerous area is
    // almost certainly *less* dangerous than the old dangerous area.
    if (randtele && !player_in_branch(BRANCH_ABYSS)
        && player_in_a_dangerous_place())
    {
        xom_is_stimulated(200);
    }
}

spret cast_dimensional_bullseye(int pow, monster *target, bool fail)
{
    if (target == nullptr || !you.can_see(*target))
    {
        canned_msg(MSG_NOTHING_THERE);
        // You cannot place a bullseye on invisible enemies, so just abort
        return spret::abort;
    }

    if (stop_attack_prompt(target, false, you.pos()))
        return spret::abort;

    fail_check();

    // We can only have a bullseye on one target a time, so remove the old one
    // if it's still active
    if (you.props.exists(BULLSEYE_TARGET_KEY))
    {
        monster* old_targ =
            monster_by_mid(you.props[BULLSEYE_TARGET_KEY].get_int());

        if (old_targ)
            old_targ->del_ench(ENCH_BULLSEYE_TARGET);
    }

    mprf("You create a dimensional link between your ranged weaponry and %s.",
         target->name(DESC_THE).c_str());

    // So we can automatically end the status if the target dies or becomes
    // friendly
    target->add_ench(ENCH_BULLSEYE_TARGET);

    you.props[BULLSEYE_TARGET_KEY].get_int() = target->mid;
    int dur = random_range(5 + div_rand_round(pow, 5),
                           7 + div_rand_round(pow, 4));
    you.set_duration(DUR_DIMENSIONAL_BULLSEYE, dur);
    return spret::success;
}

string weapon_unprojectability_reason(const item_def* wpn)
{
    if (!wpn)
        return "";

    // These don't work properly when performing attacks against non-adjacent
    // targets. Maybe support them in future?
    static const vector<int> forbidden_unrands = {
        UNRAND_POWER,
        UNRAND_ARC_BLADE,
    };
    for (int urand : forbidden_unrands)
    {
        if (is_unrandom_artefact(*wpn, urand))
        {
            return make_stringf("%s would react catastrophically with paradoxical space!",
                                you.weapon()->name(DESC_THE, false, false, false, false, ISFLAG_KNOW_PLUSES).c_str());
        }
    }
    return "";
}

// Mildly hacky: If this was triggered via Autumn Katana, katana_defender is the
//               target it first triggered on. If nullptr, this is a normal cast.
spret cast_manifold_assault(actor& agent, int pow, bool fail, bool real,
                            actor* katana_defender)
{
    bool found_unsafe_target = false;
    vector<actor*> targets;
    for (actor_near_iterator ai(&agent, LOS_NO_TRANS); ai; ++ai)
    {
        monster* mon = ai->is_monster() ? ai->as_monster() : nullptr;
        if (mons_aligned(&agent, *ai) || mon && mon->neutral())
            continue; // this should be enough to avoid penance?
        if (mon && (mon->is_firewood() || mons_is_projectile(*mon)))
            continue;
        if (!agent.can_see(**ai)
            || (agent.is_monster() && !monster_los_is_valid(agent.as_monster(), *ai)))
        {
            continue;
        }

        // If this was triggered by the Autumn Katana, don't hit the original
        // target a second time
        if (*ai == katana_defender)
            continue;

        // If the player is casting, make a melee attack to test if we'd
        // ordinarily need a prompt to hit this target, and ignore all such
        // targets entirely.
        //
        // We only perform this test for real casts, because otherwise the game
        // prints a misleading message to the player first (about there being
        // no targets in range)
        if (agent.is_player() && real)
        {
            melee_attack atk(&you, *ai);
            if (!atk.would_prompt_player())
                targets.emplace_back(*ai);
            else
                found_unsafe_target = true;
        }
        else
            targets.emplace_back(*ai);
    }

    if (targets.empty())
    {
        if (agent.is_player() && !katana_defender)
        {
            if (real && !found_unsafe_target)
                mpr("You can't see anything to attack.");
            else if (real && found_unsafe_target)
                mpr("You can't see anything you can safely attack.");
        }
        return spret::abort;
    }

    const item_def *weapon = agent.weapon();

    if (real)
    {
        const string unproj_reason = weapon_unprojectability_reason(weapon);
        if (unproj_reason != "")
        {
            if (agent.is_player())
                mpr(unproj_reason);
            return spret::abort;
        }
    }

    if (!real)
        return spret::success;

    if (agent.is_player() && !katana_defender && !wielded_weapon_check(weapon))
        return spret::abort;

    fail_check();

    if ((agent.is_player() || you.can_see(agent)) && !katana_defender)
    {
        if (weapon && is_unrandom_artefact(*weapon, UNRAND_AUTUMN_KATANA))
            mprf("Space folds impossibly around %s blade!", agent.name(DESC_ITS).c_str());
        else
            mpr("Space momentarily warps into an impossible shape!");
    }

    shuffle_array(targets);
    // UC is worse at launching multiple manifold assaults, since
    // shapeshifters have a much easier time casting it.
    const size_t max_targets = weapon ? 4 + div_rand_round(pow, 25)
                                      : 2 + div_rand_round(pow, 50);
    const int animation_delay = 80 / max_targets;

    if ((Options.use_animations & UA_BEAM) != UA_NONE)
    {
        for (size_t i = 0; i < max_targets && i < targets.size(); i++)
        {
            flash_tile(targets[i]->pos(), LIGHTMAGENTA, animation_delay,
                       TILE_BOLT_MANIFOLD_ASSAULT);
            view_clear_overlays();
        }
    }

    for (size_t i = 0; i < max_targets && i < targets.size(); i++)
    {
        melee_attack atk(&agent, targets[i]);
        atk.is_projected = true;
        if (katana_defender)
        {
            if (you.offhand_weapon() && is_unrandom_artefact(*you.offhand_weapon(), UNRAND_AUTUMN_KATANA))
                atk.set_weapon(you.offhand_weapon(), true);
            // Only the katana can attack through space!
            atk.attack();
        }
        // Only rev up once, no matter how many targets you hit.
        else
            atk.launch_attack_set(i == 0);

        if (i == 0)
            you.time_taken = you.attack_delay().roll();

        // Stop further attacks if we somehow died in the process.
        // (Unclear how this is possible?)
        if (agent.is_player() && (you.hp <= 0 || you.pending_revival)
            || agent.is_monster() && !agent.alive())
        {
            break;
        }
    }

    return spret::success;
}

spret cast_apportation(int pow, bolt& beam, bool fail)
{
    const coord_def where = beam.target;

    if (!cell_see_cell(you.pos(), where, LOS_SOLID))
    {
        canned_msg(MSG_SOMETHING_IN_WAY);
        return spret::abort;
    }

    // Let's look at the top item in that square...
    // And don't allow apporting from shop inventories.
    // Using visible_igrd takes care of deep water/lava where appropriate.
    const int item_idx = you.visible_igrd(where);
    if (item_idx == NON_ITEM || !in_bounds(where))
    {
        mpr("You don't see anything to apport there.");
        return spret::abort;
    }

    item_def& item = env.item[item_idx];

    // Nets can be apported when they have a victim trapped.
    if (item_is_stationary(item) && !item_is_stationary_net(item))
    {
        mpr("You cannot apport that!");
        return spret::abort;
    }

    fail_check();

    // We need to modify the item *before* we move it, because
    // move_top_item() might change the location, or merge
    // with something at our position.
    if (item_is_orb(item))
    {
        fake_noisy(30, where);

        // There's also a 1-in-3 flat chance of apport failing.
        if (one_chance_in(3))
        {
            orb_pickup_noise(where, 30,
                "The Orb shrieks and becomes a dead weight against your magic!",
                "The Orb lets out a furious burst of light and becomes "
                    "a dead weight against your magic!");
            return spret::success;
        }
        else // Otherwise it's just a noisy little shiny thing
        {
            orb_pickup_noise(where, 30,
                "The Orb shrieks as your magic touches it!",
                "The Orb lets out a furious burst of light as your magic touches it!");
            start_orb_run(CHAPTER_ANGERED_PANDEMONIUM, "Now pick up the Orb and get out of here!");
        }
    }

    // If we apport a net, free the monster under it.
    if (item_is_stationary_net(item))
    {
        free_stationary_net(item_idx);
        if (monster* mons = monster_at(where))
            mons->del_ench(ENCH_HELD, true);
    }

    beam.is_tracer = true;
    beam.aimed_at_spot = true;
    beam.affects_nothing = true;
    beam.fire();

    // The item's current location is not part of the apportion path
    beam.path_taken.pop_back();

    // The actual number of squares it needs to traverse to get to you.
    int dist = beam.path_taken.size();

    // The maximum number of squares the item will actually move, always
    // at least one square. Always has a chance to move the entirety of default
    // LOS (7), but only becomes certain at max power (50).
    int max_dist = max(1, min(LOS_RADIUS, random2(8) + div_rand_round(pow, 7)));

    dprf("Apport dist=%d, max_dist=%d", dist, max_dist);

    // path_taken does not include the player's position either, but we do want
    // to check that. Treat -1 as the player's pos; 0 is 1 away from player.
    int location_on_path = max(-1, dist - max_dist);
    coord_def new_spot = (location_on_path < 0)
                                        ? you.pos()
                                        : beam.path_taken[location_on_path];

    // Try to find safe terrain for the item, including the player's position
    // if max_dist < 0. At this point, location_on_path is guaranteed to be
    // less than dist.
    while (location_on_path < dist)
    {
        if (!feat_eliminates_items(env.grid(new_spot)))
            break;
        location_on_path++;
        if (location_on_path == dist)
        {
            // we've checked every position in beam.path_taken within max_dist
            mpr("Not with that terrain in the way!");
            return spret::success; // of a sort
        }
        new_spot = beam.path_taken[location_on_path];
    }
    dprf("Apport: new spot is %d/%d", new_spot.x, new_spot.y);

    // Actually move the item.
    mprf("Yoink! You pull the item%s towards yourself.",
         (item.quantity > 1) ? "s" : "");

    move_top_item(where, new_spot);

    // Mark the item as found now.
    origin_set(new_spot);

    return spret::success;
}

bool golubria_valid_cell(coord_def p, bool just_check)
{
    return in_bounds(p)
           && env.grid(p) == DNGN_FLOOR
           && (!monster_at(p) || just_check && !you.can_see(*monster_at(p)))
           && cell_see_cell(you.pos(), p, LOS_NO_TRANS);
}

spret cast_golubrias_passage(int pow, const coord_def& where, bool fail)
{
    if (player_in_branch(BRANCH_GAUNTLET))
    {
        mprf(MSGCH_ORB, "A magic seal in the Gauntlet prevents you from "
                "opening a passage!");
        return spret::abort;
    }

    if (grid_distance(where, you.pos())
        > spell_range(SPELL_GOLUBRIAS_PASSAGE, pow))
    {
        mpr("That's out of range!");
        return spret::abort;
    }

    if (cell_is_solid(where))
    {
        mpr("You can't create a passage there!");
        return spret::abort;
    }

    int tries = 0;
    int tries2 = 0;
    const int range = GOLUBRIA_FUZZ_RANGE;
    coord_def randomized_where = where;
    coord_def randomized_here = you.pos();
    do
    {
        tries++;
        randomized_where = where;
        randomized_where.x += random_range(-range, range);
        randomized_where.y += random_range(-range, range);
    }
    while ((!golubria_valid_cell(randomized_where)
            || randomized_where == you.pos())
           && tries < 100);

    do
    {
        tries2++;
        randomized_here = you.pos();
        randomized_here.x += random_range(-range, range);
        randomized_here.y += random_range(-range, range);
    }
    while ((!golubria_valid_cell(randomized_here)
            || randomized_here == randomized_where)
           && tries2 < 100);

    if (tries >= 100 || tries2 >= 100)
    {
        if (you.trans_wall_blocking(randomized_where))
        {
            mpr("You cannot create a passage on the other side of the "
                "transparent wall.");
        }
        else
        {
            // XXX: bleh, dumb message
            mpr("Creating a passage of Golubria requires sufficient empty "
                "space.");
        }

        return spret::abort;
    }

    fail_check();
    place_specific_trap(randomized_where, TRAP_GOLUBRIA);
    place_specific_trap(randomized_here, TRAP_GOLUBRIA);
    env.level_state |= LSTATE_GOLUBRIA;

    trap_def *trap = trap_at(randomized_where);
    trap_def *trap2 = trap_at(randomized_here);
    if (!trap || !trap2)
    {
        mpr("Something buggy happened.");
        return spret::abort;
    }

    trap->reveal();
    trap2->reveal();

    return spret::success;
}

static int _disperse_monster(monster& mon, int pow)
{
    if (mon.no_tele())
        return false;

    if (mon.check_willpower(&you, pow) > 0)
        monster_blink(&mon);
    else
        monster_teleport(&mon, true);

    // Moving the monster may have killed it in apply_location_effects.
    if (mon.alive() && mon.check_willpower(&you, pow) <= 0)
        mon.confuse(&you, 1 + random2avg(1 + div_rand_round(pow, 10), 2));

    return true;
}

spret cast_dispersal(int pow, bool fail)
{
    fail_check();
    const int radius = spell_range(SPELL_DISPERSAL, pow);
    if (!apply_monsters_around_square([pow] (monster& mon) {
            return _disperse_monster(mon, pow);
        }, you.pos(), radius))
    {
        mpr("The air shimmers briefly around you.");
    }
    return spret::success;
}

void pull_monsters_inward(const coord_def& center, int radius)
{
    vector<coord_def> empty[LOS_RADIUS];
    vector<monster*> targ;

    // Build lists of valid monsters and empty spaces, sorted in order of distance
    for (distance_iterator di(center, true, false, radius); di; ++di)
    {
        if (!you.see_cell_no_trans(*di))
            continue;

        const int dist = grid_distance(center, *di);
        monster* mon = monster_at(*di);
        if (mon && !mon->is_stationary() && !mons_is_projectile(*mon))
            targ.push_back(mon);
        else if (!mon && !feat_is_solid(env.grid(*di)))
            empty[dist].push_back(*di);
    }

    // Move each monster to a nearer space, if we can
    for (monster* mon : targ)
    {
        bool moved = false;
        for (int dist = 0; dist <= radius; ++dist)
        {
            if (grid_distance(mon->pos(), center) <= dist)
                break;

            for (unsigned int i = 0; i < empty[dist].size(); ++i)
            {
                const coord_def new_pos = empty[dist][i];

                if (monster_habitable_grid(mon, new_pos))
                {
                    const coord_def old_pos = mon->pos();
                    mon->move_to_pos(new_pos);
                    mon->apply_location_effects(old_pos);
                    mons_relocated(mon);

                    empty[dist].erase(empty[dist].begin() + i);

                    if (grid_distance(center, old_pos) < 2)
                        empty[grid_distance(center, old_pos) - 1].push_back(old_pos);

                    moved = true;

                    break;
                }
            }

            if (moved)
                break;
        }
    }
}

int gravitas_radius(int pow)
{
    return 2 + (pow / 48);
}

spret cast_gravitas(int pow, const coord_def& where, bool fail)
{
    fail_check();

    monster* mons = monster_at(where);
    const int radius = gravitas_radius(pow);

    // XXX: If this is ever usable from elsewhere than the item, change this.
    mpr("You rattle the tambourine.");

    mprf("Waves of gravity draw inward%s%s.",
         mons || feat_is_solid(env.grid(where)) ? " around " : "",
         mons ? mons->name(DESC_THE).c_str() :
                feat_is_solid(env.grid(where)) ? feature_description(env.grid(where),
                                                                     NUM_TRAPS, "",
                                                                     DESC_THE)
                                                                    .c_str() : "");

    draw_ring_animation(where, radius, LIGHTMAGENTA);
    pull_monsters_inward(where, radius);

    // Bind all hostile monsters in place and damage them
    // (friendlies are exempt from this part)
    int dur = (random_range(2, 4) + div_rand_round(pow, 25)) * BASELINE_DELAY;
    for (distance_iterator di(where, false, false, radius); di; ++di)
    {
        if (!you.see_cell_no_trans(*di))
            continue;

        if (monster* mon = monster_at(*di))
        {
            if (mon->wont_attack() || mons_is_projectile(*mon))
                continue;

            int dmg = zap_damage(ZAP_GRAVITAS, pow, false).roll();
            dmg = mon->apply_ac(dmg);

            if (you.can_see(*mon))
                mprf("%s is pinned by gravity.", mon->name(DESC_THE).c_str());
            mon->hurt(&you, dmg);
            mon->add_ench(mon_enchant(ENCH_BOUND, 0, &you, dur));
            behaviour_event(mon, ME_WHACK, &you, you.pos());
        }
    }

    return spret::success;
}

static bool _can_beckon(const actor &beckoned)
{
    return !beckoned.is_stationary()  // don't move statues, etc
        && !mons_is_tentacle_or_tentacle_segment(beckoned.type); // a mess...
}

/**
 * Where is the closest point along the given path to its source that the given
 * actor can be moved to?
 *
 * @param beckoned      The actor to be moved.
 * @param path          The path for the actor to be moved along
 * @return              The closest point for the actor to be moved to;
 *                      guaranteed to be on the path or its original location.
 */
static coord_def _beckon_destination(const actor &beckoned, const bolt &path)
{
    if (!_can_beckon(beckoned))
        return beckoned.pos();

    for (coord_def pos : path.path_taken)
    {
        if (actor_at(pos) || !beckoned.is_habitable(pos))
            continue; // actor could be caster, or a bush

        return pos;
    }

    return beckoned.pos(); // failed to find any point along the path
}

/**
 * Attempt to move the beckoned creature to the spot on the path closest to its
 * beginning (that is, to the caster of the effect). Also handles some
 * messaging.
 *
 * @param beckoned  The creature being moved.
 * @param path      The path to move the creature along.
 * @return          Whether the beckoned creature actually moved.
 */
bool beckon(actor &beckoned, const bolt &path)
{
    const coord_def dest = _beckon_destination(beckoned, path);
    if (dest == beckoned.pos())
        return false;

    const coord_def old_pos = beckoned.pos();
    if (!beckoned.move_to_pos(dest))
        return false;

    mprf("%s %s suddenly forward!",
         beckoned.name(DESC_THE).c_str(),
         beckoned.conj_verb("hurl").c_str());

    beckoned.apply_location_effects(old_pos); // traps, etc.
    if (beckoned.is_monster())
        mons_relocated(beckoned.as_monster()); // cleanup tentacle segments

    return true;
}

static bool _can_move_mons_to(const monster &mons, coord_def pos)
{
    return mons.can_pass_through_feat(env.grid(pos))
           && !actor_at(pos)
           && mons.is_habitable(pos);
}

/// Attempt to pull a monster toward the player.
void attract_monster(monster &mon, int max_move)
{
    const int orig_dist = grid_distance(you.pos(), mon.pos());
    if (orig_dist <= 1)
        return;

    ray_def ray;
    if (!find_ray(mon.pos(), you.pos(), ray, opc_solid))
        return;

    for (int i = 0; i < max_move && i < orig_dist - 1; i++)
        ray.advance();

    while (!_can_move_mons_to(mon, ray.pos()) && ray.pos() != mon.pos())
        ray.regress();

    if (ray.pos() == mon.pos())
        return;

    const coord_def old_pos = mon.pos();
    if (!mon.move_to_pos(ray.pos()))
        return;

    mprf("%s is attracted toward you.", mon.name(DESC_THE).c_str());

    _place_tloc_cloud(old_pos);
    _place_tloc_cloud(ray.pos());
    mon.apply_location_effects(old_pos);
    mons_relocated(&mon);
}

/**
  * Attempt to pull nearby monsters toward the player.
 */
void attract_monsters(int delay)
{
    vector<monster *> targets;
    for (monster_near_iterator mi(you.pos(), LOS_NO_TRANS); mi; ++mi)
        if (!mi->friendly() && !(*mi)->no_tele())
            targets.push_back(*mi);

    near_to_far_sorter sorter = {you.pos()};
    sort(targets.begin(), targets.end(), sorter);

    for (monster *mi : targets)
        attract_monster(*mi, div_rand_round(3 * delay, BASELINE_DELAY));
}

vector<monster *> find_chaos_targets(bool just_check)
{
    vector<monster *> targets;
    for (monster_near_iterator mi(you.pos(), LOS_NO_TRANS); mi; ++mi)
    {
        if (!mons_is_tentacle_or_tentacle_segment(mi->type)
            && !mons_class_is_stationary(mi->type)
            && !mi->is_peripheral()
            && !mi->friendly())
        {
            if (!just_check || you.can_see(**mi))
                targets.push_back(*mi);
        }
    }

    return targets;
}

spret word_of_chaos(int pow, bool fail)
{
    vector<monster *> visible_targets = find_chaos_targets(true);
    vector<monster *> targets = find_chaos_targets();
    if (visible_targets.empty())
    {
        if (!yesno("You cannot see any enemies that you can affect. Speak a "
                   "word of chaos anyway?", true, 'n'))
        {
            canned_msg(MSG_OK);
            return spret::abort;
        }
    }

    fail_check();
    shuffle_array(targets);

    mpr("You speak a word of chaos!");
    for (auto mons : targets)
    {
        if (mons->no_tele())
            continue;

        blink_away(mons, &you, false);
        if (x_chance_in_y(pow, 500))
            ensnare(mons);
        if (x_chance_in_y(pow, 500))
            do_slow_monster(*mons, &you, 20 + random2(pow));
        if (x_chance_in_y(pow, 500))
        {
            mons->add_ench(mon_enchant(ENCH_FEAR, 0, &you));
            behaviour_event(mons, ME_SCARE, &you);
        }
    }

    you.increase_duration(DUR_WORD_OF_CHAOS_COOLDOWN, 15 + random2(10));
    drain_player(50, false, true);
    return spret::success;
}

spret blinkbolt(int power, bolt &beam, bool fail)
{
    if (cell_is_solid(beam.target))
    {
        canned_msg(MSG_UNTHINKING_ACT);
        return spret::abort;
    }

    monster* mons = monster_at(beam.target);
    if (!mons || !you.can_see(*mons))
    {
        mpr("You see nothing there to target!");
        return spret::abort;
    }

    if (mons_aligned(mons, &you) || mons->is_firewood())
    {
        canned_msg(MSG_UNTHINKING_ACT);
        return spret::abort;
    }

    const monster* beholder = you.get_beholder(beam.target);
    if (beholder)
    {
        mprf("You cannot blinkbolt away from %s!",
            beholder->name(DESC_THE, true).c_str());
        return spret::abort;
    }

    const monster* fearmonger = you.get_fearmonger(beam.target);
    if (fearmonger)
    {
        mprf("You cannot blinkbolt closer to %s!",
            fearmonger->name(DESC_THE, true).c_str());
        return spret::abort;
    }

    if (!player_tracer(ZAP_BLINKBOLT, power, beam))
        return spret::abort;

    fail_check();

    you.stop_being_constricted(false, "bolt");

    beam.thrower = KILL_YOU_MISSILE;
    zappy(ZAP_BLINKBOLT, power, false, beam);
    beam.name = "shock of your passage";
    beam.fire();
    you.duration[DUR_BLINKBOLT_COOLDOWN] = 50 + random2(150);

    return spret::success;
}

static bool _valid_piledriver_target(monster* targ)
{
    return targ && !targ->wont_attack() && !targ->is_stationary()
           && you.can_see(*targ);
}

// Returns a vector of how many connected pushable monstes are in a row here,
// for Piledriver.
//
// Returns an empty vector if there is some reason the line isn't pushable.
static vector<monster*> _get_monster_line(coord_def start, bool actual)
{
    ASSERT(monster_at(start));

    const coord_def delta = start - you.pos();
    vector<monster*> move_targets;
    move_targets.push_back(monster_at(start));
    coord_def pos = start + delta;
    // Iterate to find how many connected monsters are in a row here.
    while (monster* mon = monster_at(pos))
    {
        // If the player can't see the monster here, don't leak its position to
        // the targeter.
        if (!mon || !actual && !you.can_see(*mon))
            break;

        // Can't move anything with a stationary monster (or friend) in its cluster.
        if (mon->is_stationary() || mon->wont_attack())
            return vector<monster*>();

        // If the line extends past the player's sight, it definitely isn't usable.
        if (!you.see_cell_no_trans(pos))
            return vector<monster*>();

        // Add this monster to the line and advance one step
        move_targets.push_back(mon);
        pos += delta;
    }

    return move_targets;
}

bool piledriver_target_exists()
{
    for (adjacent_iterator ai(you.pos()); ai; ++ai)
    {
        if (piledriver_path_distance(*ai, false) > 0)
            return true;
    }

    return false;
}

/**
 * Calculates the total distance between the player and whatever obstacle would
 * stop a piledriver aimed in a given direction.
 *
 * @param target  What cell to aim the piledriver at.
 * @param actual  Whether the player is casting the spell for real (and this
 *                should account for even invisible monsters).
 *
 * @return Total distance of a piledriver path in this direction.
 *         0 if this is not currently a valid direction for piledriver.
 */
int piledriver_path_distance(const coord_def& target, bool actual)
{
    // Skip directions with no valid starter monster.
    monster* targ = monster_at(target);
    if (!_valid_piledriver_target(targ))
        return 0;

    // Check how many pushable monsters are in a row here.
    vector<monster*> line = _get_monster_line(target, actual);
    if (line.empty())
        return 0;

    // Now see if a collidable obstacle is both within the player's sight
    // and piledriver's maximum range.
    const coord_def delta = target - you.pos();
    coord_def pos = you.pos() + (delta * (line.size() + 1));
    int move_dist = 0;
    while (true)
    {
        // Abort if we leave the player's LoS without finding something to hit.
        if (!you.see_cell_no_trans(pos)
            || grid_distance(target, pos) > spell_range(SPELL_PILEDRIVER, 100))
        {
            return 0;
        }

        // Found something to hit; this is where we stop.
        if (cell_is_solid(pos)
            || monster_at(pos) && (actual || you.can_see(*monster_at(pos))))
        {
            break;
        }

        ++move_dist;
        pos += delta;
    }

    // We didn't find a collidable object in LoS
    if (move_dist == 0)
        return 0;

    // Now, finally test that every space in a row can be occupied by each
    // monster in sequence (and the player) and stop when this isn't true.
    for (int i = 1; i <= move_dist; ++i)
    {
        for (size_t j = 0; j < line.size(); ++j)
            if (!monster_habitable_grid(line[j], line[j]->pos() + (delta * i)))
                return 0;

        if (is_feat_dangerous(env.grid(you.pos() + (delta * i))))
            return 0;
    }

    return move_dist + line.size();
}

dice_def piledriver_collision_damage(int pow, int dist, bool random)
{
    if (!random)
        return dice_def(1 + (dist * 2), 2 + (pow + 10) / 15);
    else
        return dice_def(1 + (dist * 2), 2 + (div_rand_round(pow + 10, 15)));
}

spret cast_piledriver(const coord_def& target, int pow, bool fail)
{
    // Calculate all possible valid targets first, so we can prompt the player
    // about anything they *might* hit.

    int length = piledriver_path_distance(target, false);
    vector<coord_def> path;
    const coord_def delta = target - you.pos();
    const coord_def impact = target + (delta * length);

    vector<monster*> mons = _get_monster_line(target, false);
    if (warn_about_bad_targets(SPELL_PILEDRIVER, {impact}))
        return spret::abort;

    fail_check();

    // Now that they've confirmed, find what we're *actually* moving.
    length = piledriver_path_distance(target, true);

    // There must be something invisible blocking all possible paths, so 'fail'.
    if (length == 0)
    {
        mprf("Space begins to contract around you, but something blocks your path.");
        return spret::success;
    }

    mons = _get_monster_line(target, true);
    mprf("Space contracts around you and %s and then re-expands violently!",
            mons.size() == 1 ? mons[0]->name(DESC_THE).c_str()
                             : make_stringf("%s other creatures",
                                    number_in_words(mons.size()).c_str()).c_str());

    // Animate the player and their victim flying forward together
    bolt anim;
    anim.source = you.pos();
    anim.target = target;
    anim.flavour = BEAM_VISUAL;
    anim.range = length;
    anim.fire();

    // Move both everything to their destination first.
    const int move_dist = length - mons.size();
    const coord_def old_pos = you.pos();
    for (int i = (int)mons.size() - 1; i >= 0; --i)
        mons[i]->move_to_pos(old_pos + delta * (move_dist + i + 1));
    you.move_to_pos(old_pos + (delta * move_dist));

    // Apply collision damage (scaling with distance covered)
    const int dmg = piledriver_collision_damage(pow, move_dist, true).roll();
    mons.back()->collide(target + (delta * length), &you, dmg);

    // Now trigger location effects (to avoid dispersal traps causing all sorts
    // of problems with keeping the two of us together in the middle)
    for (size_t i = 0; i < mons.size(); ++i)
    {
        if (mons[i]->alive())
            mons[i]->apply_location_effects(target + (delta * i));
    }
    you.apply_location_effects(old_pos);

    return spret::success;
}

dice_def gavotte_impact_damage(int pow, int dist, bool random)
{
    if (!random)
        return dice_def(2, (pow * 3 / 4 + 35) * (dist + 5) / 20 + 1);
    else
        return dice_def(2, div_rand_round((pow * 3 / 4 + 35) * (dist + 5), 20) + 1);
}

static void _maybe_penance_for_collision(god_conduct_trigger conducts[3], actor& victim)
{
    if (victim.is_monster() && victim.alive())
    {
        //potentially penance
        set_attack_conducts(conducts, *victim.as_monster(),
            you.can_see(*victim.as_monster()));
    }
}

static void _push_actor(actor& victim, coord_def dir, int dist, int pow)
{
    const bool immune = never_harm_monster(&you, victim.as_monster());

    god_conduct_trigger conducts[3];

    if (victim.is_monster() && !immune)
    {
        behaviour_event(victim.as_monster(), ME_ALERT, &you, you.pos());
        victim.as_monster()->speed_increment -= 10;
    }

    const coord_def starting_pos = victim.pos();
    for (int i = 1; i <= dist; ++i)
    {
        const coord_def next_pos = starting_pos + (dir * i);

        if (!victim.can_pass_through_feat(env.grid(next_pos)) && i > 1
            && !victim.is_player())
        {
            victim.collide(next_pos, &you, gavotte_impact_damage(pow, i, true).roll());
            _maybe_penance_for_collision(conducts, victim);
            break;
        }
        else if (actor* act_at_space = actor_at(next_pos))
        {
            if (i > 1 && &victim != act_at_space && !victim.is_player()
                && !act_at_space->is_player())
            {
                victim.collide(next_pos, &you, gavotte_impact_damage(pow, i, true).roll());
                _maybe_penance_for_collision(conducts, victim);
                _maybe_penance_for_collision(conducts, *act_at_space);
            }
            break;
        }
        else if (!victim.is_habitable(next_pos))
            break;
        else
            victim.move_to_pos(next_pos);
    }

    if (starting_pos != victim.pos())
    {
        victim.apply_location_effects(starting_pos);
        if (victim.is_monster())
            mons_relocated(victim.as_monster());
    }
}

spret cast_gavotte(int pow, const coord_def dir, bool fail)
{
    vector<monster*> affected = gavotte_affected_monsters(dir);
    if (!affected.empty())
    {
        vector<coord_def> spots;
        for (unsigned int i = 0; i < affected.size(); ++i)
            spots.push_back(affected[i]->pos());

        if (warn_about_bad_targets(SPELL_GELLS_GAVOTTE, spots))
            return spret::abort;
    }

    fail_check();

    // XXX: Surely there's a better way to do this
    string dir_msg = "???";
    if (dir.x == 0 && dir.y == -1)
        dir_msg = "north";
    else if (dir.x == 0 && dir.y == 1)
        dir_msg = "south";
    else if (dir.x == -1 && dir.y == 0)
        dir_msg = "west";
    else if (dir.x == 1 && dir.y == 0)
        dir_msg = "east";
    else if (dir.x == -1 && dir.y == -1)
        dir_msg = "northwest";
    else if (dir.x == 1 && dir.y == -1)
        dir_msg = "northeast";
    else if (dir.x == -1 && dir.y == 1)
        dir_msg = "southwest";
    else if (dir.x == 1 && dir.y == 1)
        dir_msg = "southeast";

    mprf("Gravity reorients to the %s!", dir_msg.c_str());

    if (you.stasis())
        canned_msg(MSG_STRANGE_STASIS);

    // Gather all actors we will be moving
    vector<actor*> targs;

    // Don't move stationary players (or formicids)
    if (!you.is_stationary() && !you.stasis())
        targs.push_back(&you);

    for (monster_near_iterator mi(you.pos()); mi; ++mi)
    {
        if (!mi->is_stationary() && you.see_cell_no_trans(mi->pos()))
            targs.push_back(*mi);
    }

    // Sort by closest to the pull direction
    sort(targs.begin( ), targs.end( ), [dir](actor* a, actor* b)
    {
        return (a->pos().x * dir.x) + (a->pos().y * dir.y)
               > (b->pos().x * dir.x) + (b->pos().y * dir.y);
    });

    // Push all monsters, in order
    for (unsigned int i = 0; i < targs.size(); ++i)
    {
        // Some circumstances, such as lost souls sacrificing themselves, can
        // result in monsters dying before it even comes time to push them.
        if (targs[i]->alive())
            _push_actor(*targs[i], dir, GAVOTTE_DISTANCE, pow);
    }

    you.increase_duration(DUR_GAVOTTE_COOLDOWN, random_range(5, 9) - div_rand_round(pow, 50));

    return spret::success;
}

// Note: this is only used for the targeting display and prompts about
// potentially harming allies. As such, it relies on player-known info and may
// not reflect the exact results of the spell.
static bool _gavotte_will_wall_slam(const monster* mon, coord_def dir)
{
    // Scan in our push direction. We want to find at least one tile of open
    // space before the nearest solid feature or stationary monster. Non-stationary
    // monsters are 'free'
    int steps = GAVOTTE_DISTANCE;
    coord_def pos = mon->pos();
    while (steps)
    {
        pos += dir;

        // It is possible to move out of bounds if we're near the level boundary
        // and the player has not seen some of the terrain between this monster
        // and the level edge (meaning it assumes that terrain is non-collidable)
        if (!in_bounds(pos))
            return steps < GAVOTTE_DISTANCE;

        // Can never collide with the player (for the player's sake)
        if (pos == you.pos())
            return false;

        // If we are moving out of the player's line of sight, use map knowledge
        // to estimate collisions with walls the player has already seen, but do
        // not leak info about unseen terrain.
        const bool seen = you.see_cell(pos);
        dungeon_feature_type feat = seen ? env.grid(pos)
                                         : env.map_knowledge(pos).feat();

        // Assume that monsters can pass through unknown terrain
        if (feat == DNGN_UNSEEN)
            feat = DNGN_FLOOR;

        // If there is an obstructing feature here - or the player THINKS there
        // is, at least - consider this monster affected if it moved at least
        // one space before hitting it.
        if (!mon->can_pass_through_feat(feat))
            return steps < GAVOTTE_DISTANCE;

        bool mons_in_way = false;
        bool mons_in_way_is_stationary = false;

        // Find any visible monster that may be in the way (or our memory of such)
        if (seen)
        {
            monster* mon_at_pos = monster_at(pos);
            if (mon_at_pos && you.can_see(*mon_at_pos))
            {
                mons_in_way = true;
                mons_in_way_is_stationary = mon_at_pos->is_stationary();
            }
        }
        else
        {
            monster_info* mon_at_pos = env.map_knowledge(pos).monsterinfo();
            if (mon_at_pos)
            {
                mons_in_way = true;
                mons_in_way_is_stationary = mons_class_is_stationary(mon_at_pos->type);
            }
        }

        // If we're about to hit a blocker, check whether we will have moved at
        // least one space before doing so. Skip over mobile monsters as 'free'
        // spaces (since we can all pile up against a wall)
        if (mons_in_way)
        {
            if (mons_in_way_is_stationary)
                return steps < GAVOTTE_DISTANCE;
            else
                steps++;
        }

        steps--;
    }

    return false;
}

vector<monster*> gavotte_affected_monsters(const coord_def dir)
{
    vector<monster*> affected;

    for (monster_near_iterator mi(you.pos()); mi; ++mi)
    {
        if (!mi->is_stationary() && you.see_cell_no_trans(mi->pos())
            && you.can_see(**mi))
        {
            if (_gavotte_will_wall_slam(*mi, dir))
                affected.push_back(*mi);
        }
    }

    return affected;
}

spret cast_teleport_other(const coord_def& target, int power, bool fail)
{
    fail_check();

    bolt beam;
    beam.source = target;
    beam.target = target;
    beam.set_agent(&you);

    return zapping(ZAP_TELEPORT_OTHER, power, beam, false, nullptr, fail);
}
