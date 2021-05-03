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
#include "god-abil.h" // fedhas_passthrough for palentonga charge
#include "item-prop.h"
#include "items.h"
#include "level-state-type.h"
#include "libutil.h"
#include "los.h"
#include "losglobal.h"
#include "losparam.h"
#include "melee-attack.h" // palentonga charge
#include "message.h"
#include "mon-behv.h"
#include "mon-tentacle.h"
#include "mon-util.h"
#include "movement.h" // palentonga charge
#include "nearby-danger.h"
#include "orb.h"
#include "output.h"
#include "prompt.h"
#include "religion.h"
#include "shout.h"
#include "spl-damage.h" // cancel_tornado
#include "spl-monench.h"
#include "spl-util.h"
#include "stash.h"
#include "state.h"
#include "stringutil.h"
#include "target.h"
#include "teleport.h"
#include "terrain.h"
#include "tiledoll.h"
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
    int rand = random_range(35, 45) + random2(pow / 12);
    you.duration[DUR_DISJUNCTION] = min(90 + pow / 12,
        max(you.duration[DUR_DISJUNCTION] + rand,
        30 + rand));
    contaminate_player(750 + random2(500), true);
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
 */
void uncontrolled_blink(bool override_stasis)
{
    if (you.no_tele(true, true, true) && !override_stasis)
    {
        canned_msg(MSG_STRANGE_STASIS);
        return;
    }

    coord_def target;
    // First try to find a random square not adjacent to the player,
    // then one adjacent if that fails.
    if (!random_near_space(&you, you.pos(), target)
             && !random_near_space(&you, you.pos(), target, true))
    {
        mpr("You feel jittery for a moment.");
        return;
    }

    if (!you.attempt_escape(2)) // prints its own messages
        return;

    canned_msg(MSG_YOU_BLINK);
    const coord_def origin = you.pos();
    move_player_to_grid(target, false);
    _place_tloc_cloud(origin);
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
static bool _find_cblink_target(coord_def &target, bool safe_cancel,
                                string verb, targeter *hitfunc = nullptr)
{
    while (true)
    {
        // query for location {dlb}:
        direction_chooser_args args;
        args.restricts = DIR_TARGET;
        args.needs_path = false;
        args.top_prompt = uppercase_first(verb) + " to where?";
        args.hitfunc = hitfunc;
        dist beam;
        direction(beam, args);

        if (crawl_state.seen_hups)
        {
            mprf("Cancelling %s due to HUP.", verb.c_str());
            return false;
        }

        if (!beam.isValid || beam.target == you.pos())
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

        const monster* beholder = you.get_beholder(beam.target);
        if (beholder)
        {
            mprf("You cannot %s away from %s!",
                 verb.c_str(),
                 beholder->name(DESC_THE, true).c_str());
            continue;
        }

        const monster* fearmonger = you.get_fearmonger(beam.target);
        if (fearmonger)
        {
            mprf("You cannot %s closer to %s!",
                 verb.c_str(),
                 fearmonger->name(DESC_THE, true).c_str());
            continue;
        }

        if (cell_is_solid(beam.target))
        {
            clear_messages();
            mprf("You can't %s into that!", verb.c_str());
            continue;
        }

        monster* target_mons = monster_at(beam.target);
        if (target_mons && you.can_see(*target_mons))
        {
            mprf("You can't %s onto %s!", verb.c_str(),
                 target_mons->name(DESC_THE).c_str());
            continue;
        }

        if (!check_moveto(beam.target, verb, false))
        {
            continue;
            // try again (messages handled by check_moveto)
        }

        if (!you.see_cell_no_trans(beam.target))
        {
            clear_messages();
            if (you.trans_wall_blocking(beam.target))
                canned_msg(MSG_SOMETHING_IN_WAY);
            else
                canned_msg(MSG_CANNOT_SEE);
            continue;
        }

        if (cancel_harmful_move(false))
        {
            clear_messages();
            continue;
        }

        target = beam.target; // Grid in los, no problem.
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
    for (radius_iterator ri(target, HOP_FUZZ_RADIUS, C_SQUARE, LOS_NO_TRANS);
         ri; ++ri)
    {
        if (valid_blink_destination(&you, *ri) && one_chance_in(++seen))
            chosen = *ri;
    }
    return chosen;
}

/**
 * Attempt to hop the player to a space near a tile of their choosing.
 *
 * @param fail          Whether this came from a mis-invoked ability (& should
 *                      therefore fail after selecting a target)
 * @return              Whether the hop succeeded, aborted, or was miscast.
 */
spret frog_hop(bool fail)
{
    const int hop_range = 2 + you.get_mutation_level(MUT_HOP) * 2; // 4-6
    coord_def target;
    targeter_smite tgt(&you, hop_range, 0, HOP_FUZZ_RADIUS);
    tgt.obeys_mesmerise = true;

    if (cancel_harmful_move())
        return spret::abort;

    while (true)
    {
        if (!_find_cblink_target(target, true, "hop", &tgt))
            return spret::abort;
        if (grid_distance(you.pos(), target) > hop_range)
        {
            mpr("That's out of range!"); // ! targeting
            continue;
        }
        break;
    }
    target = _fuzz_hop_destination(target);

    fail_check();

    if (!you.attempt_escape(2)) // XXX: 1?
        return spret::success; // of a sort

    // invisible monster that the targeter didn't know to avoid, or similar
    if (target.origin())
    {
        mpr("You tried to hop, but there was no room to land!");
        // TODO: what to do here?
        return spret::success; // of a sort
    }

    if (!cell_is_solid(you.pos())) // should be safe.....
        place_cloud(CLOUD_DUST, you.pos(), 2 + random2(3), &you);
    move_player_to_grid(target, false);
    crawl_state.cancel_cmd_again();
    crawl_state.cancel_cmd_repeat();
    mpr("Boing!");
    you.increase_duration(DUR_NO_HOP, 12 + random2(13));
    apply_barbs_damage();

    return spret::success; // TODO
}

static bool _check_charge_through(coord_def pos)
{
    if (!you.can_pass_through_feat(env.grid(pos)))
    {
        clear_messages();
        mprf("You can't roll into that!");
        return false;
    }

    return true;
}

static bool _find_charge_target(vector<coord_def> &target_path, int max_range,
                                targeter *hitfunc, dist *target)
{
    // Check for unholy weapons, breadswinging, etc
    if (!wielded_weapon_check(you.weapon(), "roll"))
        return false;

    const bool interactive = target && target->interactive;
    dist targ_local;
    if (!target)
        target = &targ_local;

    // TODO: can't this all be done within a single direction call?
    while (true)
    {
        // query for location {dlb}:
        direction_chooser_args args;
        args.restricts = DIR_TARGET;
        args.mode = TARG_HOSTILE;
        args.prefer_farthest = true;
        args.top_prompt = "Roll where?";
        args.hitfunc = hitfunc;
        direction(*target, args);

        // TODO: deduplicate with _find_cblink_target
        if (crawl_state.seen_hups)
        {
            mpr("Cancelling rolling charge due to HUP.");
            return false;
        }

        if (!target->isValid || target->target == you.pos())
        {
            canned_msg(MSG_OK);
            return false;
        }

        const monster* beholder = you.get_beholder(target->target);
        if (beholder)
        {
            mprf("You cannot roll away from %s!",
                beholder->name(DESC_THE, true).c_str());
            if (interactive)
                continue;
            else
                return false;
        }

        const monster* fearmonger = you.get_fearmonger(target->target);
        if (fearmonger)
        {
            mprf("You cannot roll closer to %s!",
                fearmonger->name(DESC_THE, true).c_str());
            if (interactive)
                continue;
            else
                return false;
        }

        if (!you.see_cell_no_trans(target->target))
        {
            clear_messages();
            if (you.trans_wall_blocking(target->target))
                canned_msg(MSG_SOMETHING_IN_WAY);
            else
                canned_msg(MSG_CANNOT_SEE);
            if (interactive)
                continue;
            else
                return false;
        }

        if (grid_distance(you.pos(), target->target) > max_range)
        {
            mpr("That's out of range!"); // ! targeting
            if (interactive)
                continue;
            else
                return false;
        }

        ray_def ray;
        if (!find_ray(you.pos(), target->target, ray, opc_solid))
        {
            mpr("You can't roll through that!");
            if (interactive)
                continue;
            else
                return false;
        }

        // done with hard vetos; now we're on a mix of prompts and vetos.
        // (Ideally we'd like to split these up and do all the vetos before
        // the prompts, but...)

        target_path.clear();
        bool ok = true;
        while (ray.advance())
        {
            target_path.push_back(ray.pos());
            if (!can_charge_through_mons(ray.pos()))
                break;
            ok = _check_charge_through(ray.pos());
            if (ray.pos() == target->target || !ok)
                break;
        }
        if (!ok)
        {
            if (interactive)
                continue;
            else
                return false;
        }

        // DON'T use beam.target here - we might have used ! targeting to
        // target something behind another known monster
        const monster* target_mons = monster_at(ray.pos());
        const string bad_charge = bad_charge_target(ray.pos());
        if (bad_charge != "")
        {
            mpr(bad_charge.c_str());
            return false;
        }

        if (adjacent(you.pos(), ray.pos()))
        {
            mprf("You're already next to %s!",
                 target_mons->name(DESC_THE).c_str());
            return false;
        }

        // prompt to make sure the player really wants to attack the monster
        // (if extant and not hostile)
        // Intentionally don't use the real attack position here - that's only
        // used for sanctuary,
        // so it's more accurate if we use our current pos, since sanctuary
        // should move with us.
        if (stop_attack_prompt(target_mons, false, target_mons->pos()))
            return false;

        ray.regress();
        // confirm movement for the final square only
        if (!check_moveto(ray.pos(), "charge"))
            return false;

        return true;
    }
}

static void _charge_cloud_trail(const coord_def pos)
{
    if (!apply_cloud_trail(pos))
        place_cloud(CLOUD_DUST, pos, 2 + random2(3), &you);
}

bool palentonga_charge_possible(bool quiet, bool allow_safe_monsters)
{
    // general movement conditions are checked in ability.cc:_check_ability_possible
    targeter_charge tgt(&you, palentonga_charge_range());
    for (monster_near_iterator mi(&you); mi; ++mi)
        if (tgt.valid_aim(mi->pos())
            && (allow_safe_monsters || !mons_is_safe(*mi, false) || mons_class_is_test(mi->type)))
        {
            return true;
        }
    if (!quiet)
        mpr("There's nothing you can charge at!");
    return false;
}

int palentonga_charge_range()
{
    return 3 + you.get_mutation_level(MUT_ROLL);
}

/**
 * Attempt to charge the player to a target of their choosing.
 *
 * @param fail          Whether this came from a mis-invoked ability (& should
 *                      therefore fail after selecting a target)
 * @return              Whether the charge succeeded, aborted, or was miscast.
 */
spret palentonga_charge(bool fail, dist *target)
{
    const coord_def initial_pos = you.pos();

    vector<coord_def> target_path;
    targeter_charge tgt(&you, palentonga_charge_range());
    if (!_find_charge_target(target_path, palentonga_charge_range(), &tgt, target))
        return spret::abort;

    fail_check();

    if (!you.attempt_escape(1)) // prints its own messages
        return spret::success;

    const coord_def target_pos = target_path.back();
    monster* target_mons = monster_at(target_pos);
    if (fedhas_passthrough(target_mons))
        target_mons = nullptr;
    ASSERT(target_mons != nullptr);
    // Are you actually moving forward?
    if (grid_distance(you.pos(), target_pos) > 1 || !target_mons)
        mpr("You roll forward with a clatter of scales!");

    crawl_state.cancel_cmd_again();
    crawl_state.cancel_cmd_repeat();

    const coord_def orig_pos = you.pos();
    for (coord_def pos : target_path)
    {
        monster* sneaky_mons = monster_at(pos);
        if (sneaky_mons && !fedhas_passthrough(sneaky_mons))
        {
            target_mons = sneaky_mons;
            break;
        }
    }
    const coord_def dest_pos = target_path.at(target_path.size() - 2);

    remove_water_hold();
    move_player_to_grid(dest_pos, true);
    noisy(12, you.pos());
    apply_barbs_damage();
    _charge_cloud_trail(orig_pos);
    for (auto it = target_path.begin(); it != target_path.end() - 2; ++it)
        _charge_cloud_trail(*it);

    if (you.pos() != dest_pos) // tornado and trap nonsense
        return spret::success; // of a sort

    // Maybe we hit a trap and something weird happened.
    if (!target_mons->alive() || !adjacent(you.pos(), target_mons->pos()))
        return spret::success;

    // manually apply noise
    behaviour_event(target_mons, ME_ALERT, &you, you.pos()); // shout + set you as foe

    // We got webbed/netted at the destination, bail on the attack.
    if (you.attribute[ATTR_HELD])
        return spret::success;

    const int base_delay =
        div_rand_round(you.time_taken * player_movement_speed(), 10);

    melee_attack charge_atk(&you, target_mons);
    charge_atk.roll_dist = grid_distance(initial_pos, you.pos());
    charge_atk.attack();

    // Normally this is 10 aut (times haste, chei etc), but slow weapons
    // take longer. Most relevant for low-skill players and Dark Maul.
    you.time_taken = max(you.time_taken, base_delay);

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
spret controlled_blink(bool safe_cancel)
{
    if (crawl_state.is_repeating_cmd())
    {
        crawl_state.cant_cmd_repeat("You can't repeat controlled blinks.");
        crawl_state.cancel_cmd_again();
        crawl_state.cancel_cmd_repeat();
        return spret::abort;
    }

    coord_def target;
    targeter_smite tgt(&you, LOS_RADIUS);
    tgt.obeys_mesmerise = true;
    if (!_find_cblink_target(target, safe_cancel, "blink", &tgt))
        return spret::abort;

    if (!you.attempt_escape(2))
        return spret::success; // of a sort

    // invisible monster that the targeter didn't know to avoid
    if (monster_at(target))
    {
        mpr("Oops! There was something there already!");
        uncontrolled_blink();
        return spret::success; // of a sort
    }

    _place_tloc_cloud(you.pos());
    move_player_to_grid(target, false);

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
spret cast_blink(bool fail)
{
    // effects that cast the spell through the player, I guess (e.g. xom)
    if (you.no_tele(false, false, true))
        return fail ? spret::fail : spret::success; // probably always SUCCESS

    if (cancel_harmful_move(false))
        return spret::abort;

    fail_check();
    uncontrolled_blink();
    return spret::success;
}

void you_teleport()
{
    // [Cha] here we block teleportation, which will save the player from
    // death from read-id'ing scrolls (in sprint)
    if (you.no_tele(true, true))
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
static bool _cell_vetoes_teleport(const coord_def cell, bool check_monsters = true,
                                  bool wizard_tele = false)
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
        if (teleportitis)
            return false;

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

        if (_cell_vetoes_teleport(pos, true, wizard_tele))
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
               && (_cell_vetoes_teleport(newpos)
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
                mprf("You are suddenly yanked towards %s nearby monster%s!",
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

        cancel_tornado(true);
        // Leave a purple cloud.
        _place_tloc_cloud(old_pos);

        move_player_to_grid(newpos, false);
        stop_delay(true);
    }

    _handle_teleport_update(large_change, old_pos);
    return !wizard_tele;
}

bool you_teleport_to(const coord_def where_to, bool move_monsters)
{
    // Attempts to teleport the player from their current location to 'where'.
    // Follows this line of reasoning:
    //   1. Check the location (against _cell_vetoes_teleport), if valid,
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

    if (_cell_vetoes_teleport(where))
    {
        if (monster_at(where) && move_monsters && !_cell_vetoes_teleport(where, false))
        {
            // dlua only, don't heed no_tele
            monster* mons = monster_at(where);
            mons->teleport(true);
        }
        else
        {
            for (adjacent_iterator ai(where); ai; ++ai)
            {
                if (!_cell_vetoes_teleport(*ai))
                {
                    where = *ai;
                    break;
                }
                else
                {
                    if (monster_at(*ai) && move_monsters
                            && !_cell_vetoes_teleport(*ai, false))
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

spret cast_portal_projectile(int pow, bool fail)
{
    fail_check();
    if (!you.duration[DUR_PORTAL_PROJECTILE])
        mpr("You begin teleporting projectiles to their destination.");
    else
        mpr("You renew your portal.");
    // Calculate the accuracy bonus based on current spellpower.
    you.attribute[ATTR_PORTAL_PROJECTILE] = pow;
    you.increase_duration(DUR_PORTAL_PROJECTILE, 3 + random2(pow / 2) + random2(pow / 5), 50);
    return spret::success;
}

string weapon_unprojectability_reason()
{
    if (!you.weapon())
        return "";
    const item_def &it = *you.weapon();
    // These all cause attack prompts, which are awkward to handle.
    // TODO: support these!
    static const vector<int> forbidden_unrands = {
        UNRAND_POWER,
        UNRAND_DEVASTATOR,
        UNRAND_VARIABILITY,
        UNRAND_SINGING_SWORD,
        UNRAND_TORMENT,
        UNRAND_ARC_BLADE,
    };
    for (int urand : forbidden_unrands)
    {
        if (is_unrandom_artefact(it, urand))
        {
            return make_stringf("%s would react catastrophically with paradoxical space!",
                                you.weapon()->name(DESC_THE, false, false, false, false, ISFLAG_KNOW_PLUSES).c_str());
        }
    }
    return "";
}

spret cast_manifold_assault(int pow, bool fail, bool real)
{
    vector<monster*> targets;
    for (monster_near_iterator mi(&you, LOS_NO_TRANS); mi; ++mi)
    {
        if (mi->friendly() || mi->neutral())
            continue; // this should be enough to avoid penance?
        if (mons_is_firewood(**mi) || mons_is_projectile(**mi))
            continue;
        if (!you.can_see(**mi))
            continue;
        targets.emplace_back(*mi);
    }

    if (targets.empty())
    {
        if (real)
            mpr("You can't see anything to attack.");
        return spret::abort;
    }

    if (real)
    {
        const string unproj_reason = weapon_unprojectability_reason();
        if (unproj_reason != "")
        {
            mprf("%s", unproj_reason.c_str());
            return spret::abort;
        }
    }

    if (!real)
        return spret::success;

    if (!wielded_weapon_check(you.weapon()))
        return spret::abort;

    fail_check();

    mpr("Space momentarily warps into an impossible shape!");

    const int initial_time = you.time_taken;

    shuffle_array(targets);
    const size_t max_targets = 2 + div_rand_round(pow, 50);
    for (size_t i = 0; i < max_targets && i < targets.size(); i++)
    {
        // Somewhat hacky: reset attack delay before each attack so that only the final
        // attack ends up actually setting time taken. (No quadratic effects.)
        you.time_taken = initial_time;

        melee_attack atk(&you, targets[i]);
        atk.is_projected = true;
        atk.attack();

        if (you.hp <= 0 || you.pending_revival)
            break;
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

spret cast_golubrias_passage(const coord_def& where, bool fail)
{
    if (player_in_branch(BRANCH_GAUNTLET))
    {
        mprf(MSGCH_ORB, "A magic seal in the Gauntlet prevents you from "
                "opening a passage!");
        return spret::abort;
    }

    // randomize position a bit to make it not as useful to use on monsters
    // chasing you, as well as to not give away hidden trap positions
    int tries = 0;
    int tries2 = 0;
    // Less accurate when the orb is interfering.
    const int range = orb_limits_translocation() ? 4 : 2;
    coord_def randomized_where = where;
    coord_def randomized_here = you.pos();
    do
    {
        tries++;
        randomized_where = where;
        randomized_where.x += random_range(-range, range);
        randomized_where.y += random_range(-range, range);
    }
    while ((!in_bounds(randomized_where)
            || env.grid(randomized_where) != DNGN_FLOOR
            || monster_at(randomized_where)
            || !you.see_cell(randomized_where)
            || you.trans_wall_blocking(randomized_where)
            || randomized_where == you.pos())
           && tries < 100);

    do
    {
        tries2++;
        randomized_here = you.pos();
        randomized_here.x += random_range(-range, range);
        randomized_here.y += random_range(-range, range);
    }
    while ((!in_bounds(randomized_here)
            || env.grid(randomized_here) != DNGN_FLOOR
            || monster_at(randomized_here)
            || !you.see_cell(randomized_here)
            || you.trans_wall_blocking(randomized_here)
            || randomized_here == randomized_where)
           && tries2 < 100);

    if (tries >= 100 || tries2 >= 100)
    {
        if (you.trans_wall_blocking(randomized_where))
            mpr("You cannot create a passage on the other side of the transparent wall.");
        else
            // XXX: bleh, dumb message
            mpr("Creating a passage of Golubria requires sufficient empty space.");
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

    if (orb_limits_translocation())
        mprf(MSGCH_ORB, "The Orb disrupts the stability of your passage!");

    trap->reveal();
    trap2->reveal();

    return spret::success;
}

static int _disperse_monster(monster& mon, int pow)
{
    if (mon.no_tele())
        return false;

    if (mon.check_willpower(pow) > 0)
        monster_blink(&mon);
    else
        monster_teleport(&mon, true);

    // Moving the monster may have killed it in apply_location_effects.
    if (mon.alive() && mon.check_willpower(pow) <= 0)
        mon.confuse(&you, 1 + random2avg(pow / 10, 2));

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

int gravitas_range(int pow)
{
    return pow >= 80 ? 3 : 2;
}


#define GRAVITY "by gravitational forces"

static void _attract_actor(const actor* agent, actor* victim,
                           const coord_def pos, int pow, int strength)
{
    ASSERT(victim); // XXX: change to actor &victim
    const bool fedhas_prot = victim->is_monster()
                                && god_protects(agent, victim->as_monster());

    ray_def ray;
    if (!find_ray(victim->pos(), pos, ray, opc_solid))
    {
        // This probably shouldn't ever happen, but just in case:
        if (you.can_see(*victim))
        {
            mprf("%s violently %s moving!",
                 victim->name(DESC_THE).c_str(),
                 victim->conj_verb("stop").c_str());
        }
        if (fedhas_prot)
        {
            simple_god_message(
                make_stringf(" protects %s from harm.",
                    agent->is_player() ? "your" : "a").c_str(), GOD_FEDHAS);
        }
        else
        {
            victim->hurt(agent, roll_dice(strength / 2, pow / 20),
                         BEAM_MMISSILE, KILLED_BY_BEAM, "", GRAVITY);
        }
        return;
    }

    const coord_def starting_pos = victim->pos();
    for (int i = 0; i < strength; i++)
    {
        ray.advance();
        const coord_def newpos = ray.pos();

        if (!victim->can_pass_through_feat(env.grid(newpos)))
        {
            victim->collide(newpos, agent, pow);
            break;
        }
        else if (actor* act_at_space = actor_at(newpos))
        {
            if (victim != act_at_space)
                victim->collide(newpos, agent, pow);
            break;
        }
        else if (!victim->is_habitable(newpos))
            break;
        else
            victim->move_to_pos(newpos);

        if (victim->is_monster() && !fedhas_prot)
        {
            behaviour_event(victim->as_monster(),
                            ME_ANNOY, agent, agent ? agent->pos()
                                                   : coord_def(0, 0));
        }

        if (victim->pos() == pos)
            break;
    }
    if (starting_pos != victim->pos())
    {
        victim->apply_location_effects(starting_pos);
        if (victim->is_monster())
            mons_relocated(victim->as_monster());
    }
}

bool fatal_attraction(const coord_def& pos, const actor *agent, int pow)
{
    vector <actor *> victims;

    for (actor_near_iterator ai(pos, LOS_SOLID); ai; ++ai)
    {
        if (*ai == agent || ai->is_stationary() || ai->pos() == pos)
            continue;

        const int range = (pos - ai->pos()).rdist();
        if (range > gravitas_range(pow))
            continue;

        victims.push_back(*ai);
    }

    if (victims.empty())
        return false;

    near_to_far_sorter sorter = {you.pos()};
    sort(victims.begin(), victims.end(), sorter);

    for (actor * ai : victims)
    {
        const int range = (pos - ai->pos()).rdist();
        const int strength = ((pow + 100) / 20) / (range*range);

        _attract_actor(agent, ai, pos, pow, strength);
    }

    return true;
}

spret cast_gravitas(int pow, const coord_def& where, bool fail)
{
    if (cell_is_solid(where))
    {
        canned_msg(MSG_UNTHINKING_ACT);
        return spret::abort;
    }

    fail_check();

    monster* mons = monster_at(where);

    mprf("Gravity reorients around %s.",
         mons                      ? mons->name(DESC_THE).c_str() :
         feat_is_solid(env.grid(where)) ? feature_description(env.grid(where),
                                                         NUM_TRAPS, "",
                                                         DESC_THE)
                                                         .c_str()
                                   : "empty space");
    fatal_attraction(where, &you, pow);
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

/**
  * Attempt to pull nearby monsters toward the player.
 */
void attract_monsters()
{
    vector<monster *> targets;
    for (monster_near_iterator mi(you.pos(), LOS_NO_TRANS); mi; ++mi)
        if (!mi->friendly() && _can_beckon(**mi))
            targets.push_back(*mi);

    near_to_far_sorter sorter = {you.pos()};
    sort(targets.begin(), targets.end(), sorter);

    for (monster *mi : targets) {
        const int orig_dist = grid_distance(you.pos(), mi->pos());
        if (orig_dist <= 1)
            continue;

        ray_def ray;
        if (!find_ray(mi->pos(), you.pos(), ray, opc_solid))
            continue;

        const int max_move = 3;
        for (int i = 0; i < max_move && i < orig_dist - 1; i++)
            ray.advance();

        while (!_can_move_mons_to(*mi, ray.pos()) && ray.pos() != mi->pos())
            ray.regress();

        if (ray.pos() == mi->pos())
            continue;

        const coord_def old_pos = mi->pos();
        if (!mi->move_to_pos(ray.pos()))
            continue;

        mprf("%s is pulled toward you!", mi->name(DESC_THE).c_str());

        mi->apply_location_effects(old_pos);
        mons_relocated(mi);
    }
}

spret word_of_chaos(int pow, bool fail)
{
    vector<monster *> visible_targets;
    vector<monster *> targets;
    for (monster_near_iterator mi(you.pos(), LOS_NO_TRANS); mi; ++mi)
    {
        if (!mons_is_tentacle_or_tentacle_segment(mi->type)
            && !mons_class_is_stationary(mi->type)
            && !mons_is_conjured(mi->type)
            && !mi->friendly())
        {
            if (you.can_see(**mi))
                visible_targets.push_back(*mi);
            targets.push_back(*mi);
        }
    }

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

    mprf("You speak a word of chaos!");
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
