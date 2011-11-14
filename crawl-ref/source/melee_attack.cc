/**
 * @file
 * @brief melee_attack class and associated melee_attack methods
 */

#include "AppHdr.h"

#include "melee_attack.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <algorithm>

#include "externs.h"

#include "areas.h"
#include "artefact.h"
#include "attitude-change.h"
#include "beam.h"
#include "cloud.h"
#include "coordit.h"
#include "database.h"
#include "debug.h"
#include "delay.h"
#include "directn.h"
#include "effects.h"
#include "env.h"
#include "exercise.h"
#include "map_knowledge.h"
#include "feature.h"
#include "fineff.h"
#include "fprop.h"
#include "food.h"
#include "goditem.h"
#include "invent.h"
#include "items.h"
#include "itemname.h"
#include "itemprop.h"
#include "item_use.h"
#include "libutil.h"
#include "macro.h"
#include "makeitem.h"
#include "message.h"
#include "misc.h"
#include "mon-behv.h"
#include "mon-cast.h"
#include "mon-clone.h"
#include "mon-place.h"
#include "terrain.h"
#include "mgen_data.h"
#include "coord.h"
#include "mon-stuff.h"
#include "mon-util.h"
#include "mutation.h"
#include "ouch.h"
#include "player.h"
#include "random-var.h"
#include "religion.h"
#include "godconduct.h"
#include "shopping.h"
#include "skills.h"
#include "species.h"
#include "spl-clouds.h"
#include "spl-miscast.h"
#include "spl-summoning.h"
#include "spl-util.h"
#include "state.h"
#include "stuff.h"
#include "transform.h"
#include "traps.h"
#include "travel.h"
#include "hints.h"
#include "view.h"
#include "shout.h"
#include "xom.h"

// Odd helper function, why is this declared like this?
#define DID_AFFECT() \
{ \
    if (miscast_level == 0) \
        miscast_level = -1; \
    return; \
}

/*
 **************************************************
 *             BEGIN PUBLIC FUNCTIONS             *
 **************************************************
*/
melee_attack::melee_attack(actor *attk, actor *defn, bool allow_unarmed,
                           int attack_num, int effective_attack_num)
    :  // Call attack's constructor
    attack::attack(attk, defn, allow_unarmed),

    perceived_attack(false), obvious_effect(false), attack_number(attack_num),
    effective_attack_number(effective_attack_num),
    skip_chaos_message(false), special_damage_flavour(BEAM_NONE),
    can_do_unarmed(allow_unarmed), stab_attempt(false), stab_bonus(0),
    miscast_level(-1), miscast_type(SPTYP_NONE), miscast_target(NULL)
{
    attack_occurred = false;
    weapon          = attacker->weapon(attack_number);
    damage_brand    = attacker->damage_brand(attack_number);
    wpn_skill       = weapon ? weapon_skill(*weapon) : SK_UNARMED_COMBAT;
    to_hit          = calc_to_hit();

    attacker_armour_tohit_penalty =
        attacker->armour_tohit_penalty(true);
    attacker_shield_tohit_penalty =
        attacker->shield_tohit_penalty(true);

    can_do_unarmed =
        attacker->fights_well_unarmed(attacker_armour_tohit_penalty
                                   + attacker_shield_tohit_penalty);

    if (attacker->atype() == ACT_MONSTER)
    {
        mon_attack_def mon_attk = mons_attack_spec(attacker->as_monster(),
                                               attack_number);

        attk_type       = mon_attk.type;
        attk_flavour    = mon_attk.flavour;
        attk_damage     = mon_attk.damage;
    }

    shield = attacker->shield();
    defender_shield = defender ? defender->shield() : defender_shield;

    if (weapon && weapon->base_type == OBJ_WEAPONS && is_artefact(*weapon))
    {
        artefact_wpn_properties(*weapon, art_props);
        if (is_unrandom_artefact(*weapon))
            unrand_entry = get_unrand_entry(weapon->special);
    }

    if (weapon)
    {
        hands = hands_reqd(*weapon, attacker->body_size());

        switch (single_damage_type(*weapon))
        {
        case DAM_BLUDGEON:
        case DAM_WHIP:
            noise_factor = 125;
            break;
        case DAM_SLICE:
            noise_factor = 100;
            break;
        case DAM_PIERCE:
            noise_factor = 75;
            break;
        }
    }

    hand_half_bonus = (can_do_unarmed
                       && !shield
                       && weapon
                       && !weapon->cursed()
                       && hands == HANDS_HALF);

    attacker_visible   = attacker->observable();
    attacker_invisible = (!attacker_visible && you.see_cell(attacker->pos()));
    defender_visible   = defender && defender->observable();
    defender_invisible = (!defender_visible && defender
                          && you.see_cell(defender->pos()));
    needs_message      = (attacker_visible || defender_visible);

    attacker_body_armour_penalty = attacker->adjusted_body_armour_penalty(1);
    attacker_shield_penalty = attacker->adjusted_shield_penalty(1);

    if (defender && defender->submerged() && can_do_unarmed)
    {
        // Unarmed attacks from tentacles are the only ones that can
        // reach submerged monsters.
        can_do_unarmed = (attacker->damage_type() == DVORP_TENTACLE);
    }
}

bool melee_attack::handle_phase_attempted()
{
    if (attacker->atype() == ACT_PLAYER && defender->atype() == ACT_MONSTER)
    {
        if (weapon && is_unrandom_artefact(*weapon)
            && weapon->special == UNRAND_DEVASTATOR)
        {
            targetter_smite hitfunc(attacker, 1, 1, 1);
            hitfunc.set_aim(defender->pos());
            if (stop_attack_prompt(hitfunc, "attack"))
                return (false);
        }
        else if (stop_attack_prompt(defender->as_monster(), false,
                                    attacker->pos()))
        {
            return (false);
        }
    }

    if (attacker != defender)
    {
        // Allow setting of your allies' target, etc.
        attacker->attacking(defender);

        check_autoberserk();
    }

    // The attacker loses nutrition.
    attacker->make_hungry(3, true);

    // Initial attack causes energy to be used for all attacks.  No
    // additional energy is used for unarmed attacks.
    if (effective_attack_number == 0)
        attacker->lose_energy(EUT_ATTACK);

    // Xom thinks fumbles are funny...
    if (attacker->fumbles_attack())
    {
        // ... and thinks fumbling when trying to hit yourself is just
        // hilarious.
        xom_is_stimulated(attacker == defender ? 200 : 10);

        if (damage_brand == SPWPN_CHAOS)
            chaos_affects_attacker();

        return (false);
    }
    // Non-fumbled self-attacks due to confusion are still pretty funny, though.
    else if (attacker == defender && attacker->confused())
    {
        // And is still hilarious if it's the player.
        xom_is_stimulated(attacker->atype() == ACT_PLAYER ? 200 : 100);
    }

    attack_occurred = true;

    // Based on pre-unification code, this looks to be the appropriate place
    // to check unrand effects, but this will result in behavior where unrand
    // effects will occur regardless of whether the attacker hits or misses
    if (check_unrand_effects())
        return (false);

    /* TODO Permanently remove this? Commented out for temporary removal
     *
     * The only scenario this handles that isn't handled elsewhere (later on)
     * is identifying a ranged, cursed weapon wielded by a monster...which
     * seems like an information leak and very much a special-case. -Cryptic
    if (attacker->atype() == ACT_MONSTER)
    {
        item_def *weap = attacker->as_monster()->mslot_item(MSLOT_WEAPON);
        if (weap && you.can_see(attacker) && weap->cursed()
            && is_range_weapon(*weap))
        {
            set_ident_flags(*weap, ISFLAG_KNOW_CURSE);
        }
    }
     */

    // Check for player practicing dodging
    if (one_chance_in(3) && defender == &you)
        practise(EX_MONSTER_MAY_HIT);

    return (true);
}

bool melee_attack::handle_phase_blocked()
{
    damage_done = 0;

    // Defending monster protects itself from attacks using the wall
    // it's in. Zotdef: allow a 5% chance of a hit anyway
    if (defender->atype() == ACT_MONSTER && cell_is_solid(defender->pos())
        && mons_wall_shielded(defender->as_monster())
        && (!crawl_state.game_is_zotdef() || !one_chance_in(20)))
    {
        std::string feat_name = raw_feature_description(grd(defender->pos()));

        if (attacker->atype() == ACT_PLAYER)
        {
            // Player incurs their attack delay, of course.
            // TODO: potentially remove this; since this code has been pushed
            // back in the time line of attack sequencing, we may have already
            // incured the attacker's delay...
            you.time_taken = calc_attack_delay();

            if (you.can_see(defender))
            {
                mprf("The %s protects %s from harm.",
                     feat_name.c_str(),
                     defender->name(DESC_THE).c_str());
            }
            else
            {
                mprf("You hit the %s.", feat_name.c_str());
            }
        }
        else
        {
            if (!mons_near(defender->as_monster()))
            {
                simple_monster_message(attacker->as_monster(),
                                       " hits something.");
            }
            else if (you.can_see(attacker))
            {
                mprf("%s tries to hit %s, but is blocked by the %s.",
                     attacker->name(DESC_THE).c_str(),
                     defender->name(DESC_THE).c_str(),
                     feat_name.c_str());
            }
        }

        // Give chaos weapon a chance to affect the attacker
        if (damage_brand == SPWPN_CHAOS)
            chaos_affects_attacker();
    }

    return (true);
}

bool melee_attack::handle_phase_dodged()
{
    const int ev = defender->melee_evasion(attacker);
    const int ev_nophase = defender->melee_evasion(attacker, EV_IGNORE_PHASESHIFT);

    if (ev_margin + (ev - ev_nophase) > 0)
    {
        if (needs_message && !defender_invisible)
        {
            mprf("%s momentarily %s out as %s "
                 "attack passes through %s%s",
                 defender->name(DESC_THE).c_str(),
                 defender->conj_verb("phase").c_str(),
                 atk_name(DESC_ITS).c_str(),
                 defender->pronoun(PRONOUN_OBJECTIVE).c_str(),
                 attack_strength_punctuation().c_str());
        }
    }
    else
    {
        if (needs_message && !defender_invisible)
        {
            // TODO: Unify these, placed player_warn_miss here so I can remove
            // player_attack
            if (attacker->atype() == ACT_PLAYER)
            {
                player_warn_miss();
            }
            else
            {
                mprf("%s%s misses %s%s",
                     atk_name(DESC_THE).c_str(),
                     evasion_margin_adverb().c_str(),
                     defender_name().c_str(),
                     attack_strength_punctuation().c_str());
            }
        }
    }

    if (attacker != defender &&
        defender->atype() == ACT_PLAYER &&
        grid_distance(you.pos(), attacker->as_monster()->pos()) == 1)
    {
        // Check for defender Spines
        do_spines();

        // Spines can kill!
        if (!attacker->alive())
            return (false);
    }

    return (true);
}

/* An attack has been determined to have hit something
 *
 * Applies attack delays for both players and monsters (independently),
 * Handles to-hit effects for both attackers and defenders,
 * Determines shield_block and passess off execution to handle_phase_blocked,
 * Determines damage and passes off execution to handle_phase_damaged
 */
bool melee_attack::handle_phase_hit()
{
    did_hit = true;
    perceived_attack = true;
    bool hit_woke_orc = false;

    if (attacker->atype() == ACT_PLAYER)
    {
        if (crawl_state.game_is_hints())
            Hints.hints_melee_counter++;

        // Apply attack delay
        you.time_taken = calc_attack_delay();
        // Check for stab (and set stab_attempt and stab_bonus)
        player_stab_check();

        // TODO: Remove this (placed here so I can get rid of player_attack)
        if (you.religion == GOD_BEOGH
            && mons_genus(defender->mons_species()) == MONS_ORC
            && !defender->is_summoned()
            && !defender->as_monster()->is_shapeshifter()
            && !player_under_penance() && you.piety >= piety_breakpoint(2)
            && mons_near(defender->as_monster()) && defender->asleep())
        {
            hit_woke_orc = true;
        }
    }
    else
    {
        // Monsters lose additional energy only for the first two weapon
        // attacks; subsequent hits are free.
        if (effective_attack_number < 1)
        {
            final_attack_delay = calc_attack_delay();
            if (damage_brand == SPWPN_SPEED)
                final_attack_delay = final_attack_delay / 2 + 1;

            // speed adjustment for weapon-using monsters
            if (final_attack_delay > 0)
            {
                const int atk_speed =
                    attacker->as_monster()->action_energy(EUT_ATTACK);

                // only get one third penalty/bonus for second weapons.
                if (effective_attack_number > 0)
                {
                    final_attack_delay =
                        div_rand_round((2 * atk_speed + final_attack_delay), 3);
                }

                int delta =
                    div_rand_round((final_attack_delay - 10 + (atk_speed - 10)), 2);
                if (delta > 0)
                    attacker->as_monster()->speed_increment -= delta;
            }
        }
    }

    if (attacker != defender && attacker->self_destructs())
        return (did_hit = perceived_attack = true);

    const bool shield_blocked = attack_shield_blocked(true);

    if (shield_blocked)
    {
        if (!handle_phase_blocked())
            return (false);
    }
    else
    {
        // Slimify does no damage and serves as an on-hit effect, handle it
        if (attacker->atype() == ACT_PLAYER && you.duration[DUR_SLIMIFY]
            && mon_can_be_slimified(defender->as_monster()))
        {
            // Bail out after sliming so we don't get aux unarmed and
            // attack a fellow slime.
            damage_done = 0;
            slimify_monster(defender->as_monster());
            you.duration[DUR_SLIMIFY] = 0;

            return (true);
        }

        // This does more than just calculate the damage, it also sets up
        // messages, etc.
        damage_done = calc_damage();

        // Check if some hit-effect killed the monster
        if (attacker->atype() == ACT_PLAYER && player_monattk_hit_effects())
        	return (true);

        if (damage_done > 0)
        {
            if (!handle_phase_damaged())
                return (false);

            // TODO: Remove this, (placed here to remove player_attack)
            if (attacker->atype() == ACT_PLAYER && hit_woke_orc)
            {
                // Call function of orcs first noticing you, but with
                // beaten-up conversion messages (if applicable).
                beogh_follower_convert(defender->as_monster(), true);
            }
        }
        else
        {
            attack_verb = attacker->atype() == ACT_PLAYER
                                    ? attack_verb
                                    : attacker->conj_verb(mons_attack_verb());

            // TODO: Clean this up if possible, checking atype for do / does is ugly
            mprf("%s %s %s but %s no damage.",
                     attacker->name(DESC_THE).c_str(),
                     attack_verb.c_str(),
                     defender->name(DESC_THE).c_str(),
                     attacker->atype() == ACT_PLAYER ? "do" : "does");
        }
    }

    if(defender->props.exists("helpless"))
       defender->props.erase("helpless");

    return (true);
}

bool melee_attack::handle_phase_damaged()
{
    bool shroud_broken = false;

    if (defender->can_bleed()
        && !defender->is_summoned()
        && !defender->submerged())
    {
        int blood = modify_blood_amount(damage_done, attacker->damage_type());
        if (blood > defender->stat_hp())
            blood = defender->stat_hp();

        bleed_onto_floor(defender->pos(), defender->type, blood, true);
    }

    // TODO: Move this somewhere else, this is a terrible place for a
    // block-like (prevents all damage) effect.
    if (attacker != defender &&
        defender->atype() == ACT_PLAYER &&
        you.duration[DUR_SHROUD_OF_GOLUBRIA] && !one_chance_in(3))
    {
        // Chance of the shroud falling apart increases based on the
        // strain of it, i.e. the damage it is redirecting.
        if (x_chance_in_y(damage_done, 10+damage_done))
        {
            // Delay the message for the shroud breaking until after
            // the attack message.
            shroud_broken = true;
            you.duration[DUR_SHROUD_OF_GOLUBRIA] = 0;
        }
        else
        {
            if (needs_message)
            {
                mprf("Your shroud bends %s attack away%s",
                     atk_name(DESC_ITS).c_str(),
                     attack_strength_punctuation().c_str());
            }
            did_hit = false;
            damage_done = 0;

            return false;
        }
    }

    announce_hit();
    // Inflict stored damage
    damage_done = inflict_damage(damage_done);
    // Check for weapon brand & inflict that damage too
    apply_damage_brand();

    // TODO: Unify these, added here so we can get rid of player_attack
    if (attacker->atype() == ACT_PLAYER)
    {
        if (damage_done)
            player_exercise_combat_skills();

        // Always upset monster regardless of damage.
        // However, successful stabs inhibit shouting.
        behaviour_event(defender->as_monster(), ME_WHACK, MHITYOU,
                        coord_def(), !stab_attempt);

        // [ds] Monster may disappear after behaviour event.
        if (!defender->alive())
            return (true);

        // ugh, inspecting attack_verb here is pretty ugly
        if (damage_done && attack_verb == "trample")
            do_knockback();

        player_sustain_passive_damage();

        // Thirsty stabbing vampires get to draw blood.
        if (you.species == SP_VAMPIRE && you.hunger_state < HS_SATIATED
            && stab_attempt && stab_bonus > 0)
        {
            _player_vampire_draws_blood(defender->as_monster(),
                                        damage_done, true);
        }

        if (defender->alive())
        {
            print_wounds(defender->as_monster());

            // Actually apply the bleeding effect, this can come from an
            // aux claw or a main hand claw attack and up to now has not
            // actually happened.
            const int degree = you.has_usable_claws();
            if (apply_bleeding && defender->can_bleed()
                && degree > 0 && damage_done > 0)
            {
                defender->as_monster()->bleed(attacker,
                                              3 + roll_dice(degree, 3),
                                              degree);
            }
        }
    }
    else
    {
        const bool chaos_attack = damage_brand == SPWPN_CHAOS
                                  || (attk_flavour == AF_CHAOS
                                      && attacker != defender);

        if (defender->atype() == ACT_PLAYER)
            practise(EX_MONSTER_WILL_HIT);

        if (decapitate_hydra(damage_done, attacker->damage_type(attack_number)))
            return (true);

        special_damage = 0;
        special_damage_message.clear();
        special_damage_flavour = BEAM_NONE;

        if (attacker != defender && attk_type == AT_TRAMPLE)
            do_knockback();

        // Monsters attacking themselves don't get attack flavour.
        // The message sequences look too weird.  Also, stealing
        // attacks aren't handled until after the damage msg.
        if (attacker != defender && attk_flavour != AF_STEAL)
            mons_apply_attack_flavour();

        if (needs_message && !special_damage_message.empty())
            mprf("%s", special_damage_message.c_str());

        // Defender banished.  Bail since the defender is still alive in the
        // Abyss.
        if (defender->is_banished())
        {
            if (chaos_attack && attacker->alive())
                chaos_affects_attacker();

            do_miscast();
            return (false);
        }

        inflict_damage(special_damage, special_damage_flavour, true);

        if (!defender->alive())
        {
            if (chaos_attack && attacker->alive())
                chaos_affects_attacker();

            do_miscast();
            return (true);
        }

        // Yredelemnul's injury mirroring can kill the attacker.
        // Also, bail if the monster is attacking itself without a
        // weapon, since intrinsic monster attack flavours aren't
        // applied for self-attacks.
        if (!attacker->alive() || (attacker == defender && !weapon))
        {
            if (miscast_target == defender)
                do_miscast();
            return (false);
        }

        if (!defender->alive())
        {
            if (chaos_attack && attacker->alive())
                chaos_affects_attacker();

            do_miscast();
            return (true);
        }

        if (chaos_attack && attacker->alive())
            chaos_affects_attacker();

        if (miscast_target == defender)
            do_miscast();

        // Yredelemnul's injury mirroring can kill the attacker.
        if (!attacker->alive())
            return (false);

        if (miscast_target == attacker)
            do_miscast();

        // Miscast might have killed the attacker.
        if (!attacker->alive())
            return (false);

        if (attk_flavour == AF_STEAL)
            mons_apply_attack_flavour();
    }

    if (shroud_broken)
        mpr("Your shroud falls apart!", MSGCH_WARN);

    return (true);
}

bool melee_attack::handle_phase_killed()
{
    // Wyrmbane needs to be notified of deaths, including ones due to its
    // Dragon slaying brand, but other users of melee_effects() don't want
    // to possibly be called twice. Adding another entry for a single
    // artefact would be overkill, so here we call it by hand:
    if (unrand_entry && weapon && weapon->special == UNRAND_WYRMBANE)
        unrand_entry->fight_func.melee_effects(weapon, attacker, defender,
                                               true, special_damage);

    _monster_die(defender->as_monster(), KILL_YOU, NON_MONSTER);

    return (true);
}

bool melee_attack::handle_phase_end()
{
    if (attacker->atype() == ACT_PLAYER && can_do_unarmed
        && adjacent(defender->pos(), attacker->pos()))
    {
    	// returns whether an aux attack successfully took place
    	player_aux_unarmed();
    }

    // Check for passive mutation effects.
    if (defender->atype() == ACT_PLAYER && defender->alive()
        && attacker != defender)
    {
        mons_do_eyeball_confusion();
        do_passive_freeze();
        mons_emit_foul_stench();
    }

    return(true);
}

bool melee_attack::attack()
{
    if(!handle_phase_attempted())
        return (false);

    // Stuff for god conduct, this has to remain here for scope reasons.
    god_conduct_trigger conducts[3];
    disable_attack_conducts(conducts);

    if (attacker->atype() == ACT_PLAYER && attacker != defender)
    {
        set_attack_conducts(conducts, defender->as_monster());

        if (you.penance[GOD_ELYVILON] && god_hates_your_god(GOD_ELYVILON))
        {
            simple_god_message(" blocks your attack.", GOD_ELYVILON);
            dec_penance(GOD_ELYVILON, 1);

            return (false);
        }
    }

    // Apparently I'm insane for believing that we can still stay general past
    // this point in the combat code, mebe I am! --Cryptic

    // Calculate various ev values and begin to check them to determine the
    // correct handle_phase_ handler.
    coord_def where = defender->pos();
    const int ev = defender->melee_evasion(attacker);
    const int ev_helpless = defender_invisible ? ev
        : defender->melee_evasion(attacker, EV_IGNORE_HELPLESS);

    ev_margin = test_hit(to_hit, ev);

    // Check for a stab (helpless or petrifying)
    if (to_hit >= ev && ev_helpless > ev)
    {
        defender->props["helpless"] = true;
        ev_margin = 1;

        handle_phase_hit();
    }
    else if (ev_margin >= 0)
    {
        if (attacker != defender && attack_warded_off())
        {
            // A warded-off attack takes half the normal energy.
            attacker->gain_energy(EUT_ATTACK, 2);

            perceived_attack = true;
            return (false);
        }

        handle_phase_hit();
    }
    else
    {
        handle_phase_dodged();
    }

    // Remove sanctuary if - through some attack - it was violated.
    if (env.sanctuary_time > 0 && attack_occurred && !cancel_attack
        && attacker != defender && !attacker->confused())
    {
        if (is_sanctuary(attacker->pos()) || is_sanctuary(defender->pos()))
        {
            if (attacker->atype() == ACT_PLAYER
                || attacker->as_monster()->friendly())
            {
                remove_sanctuary(true);
            }
        }
    }

    if (attacker->atype() == ACT_PLAYER)
    {
        if (damage_brand == SPWPN_CHAOS)
            chaos_affects_attacker();

        do_miscast();

        // Odd place to do this, we'll see if we want to move it
        // On a more up-to-date thought, this is already done in handle_phase_
        // end so we'll comment it out for now to see if it stops working
        //if (can_do_unarmed && where == defender->pos())
        //    player_aux_unarmed();
    }
    else
    {
        // Invisible monster might have interrupted butchering.
        if (you_are_delayed() && defender->atype() == ACT_PLAYER
            && perceived_attack && !attacker_visible)
        {
            handle_interrupted_swap(false, true);
        }
    }

    adjust_noise();
    handle_noise(defender->pos());

    // If an enemy attacked a friend, set the pet target if it isn't set
    // already, but not if sanctuary is in effect (pet target must be
    // set explicitly by the player during sanctuary).
    if (perceived_attack && attacker->alive()
        && (defender->atype() == ACT_PLAYER
            || defender->as_monster()->friendly())
        && !attacker->as_monster()->wont_attack()
        && you.pet_target == MHITNOT
        && env.sanctuary_time <= 0)
    {
        if (defender->atype() == ACT_PLAYER)
            interrupt_activity(AI_MONSTER_ATTACKS, attacker->as_monster());
        else
            you.pet_target = attacker->mindex();
    }

    if (!defender->alive())
    	handle_phase_killed();

    // This may invalidate both the attacker and defender.
    fire_final_effects();

    enable_attack_conducts(conducts);

    return (attack_occurred);
}

// TODO: Unify attack_type and unarmed_attack_type, the former includes the latter
// however formerly, attack_type was mons_attack_type and used exclusively for monster use.
void melee_attack::adjust_noise()
{
    if (attk_type == AT_WEAP_ONLY)
    {
        int weap = attacker->as_monster()->inv[MSLOT_WEAPON];
        if (weap == NON_ITEM)
            attk_type = AT_NONE;
        else if (is_range_weapon(mitm[weap]))
            attk_type = AT_SHOOT;
        else
            attk_type = AT_HIT;
    }

    if (weapon == NULL && attacker->atype() == ACT_MONSTER)
    {
        switch (attk_type)
        {
        case AT_HEADBUTT:
        case AT_TENTACLE_SLAP:
        case AT_TAIL_SLAP:
        case AT_TRAMPLE:
        case AT_TRUNK_SLAP:
            noise_factor = 150;
            break;

        case AT_HIT:
        case AT_PUNCH:
        case AT_KICK:
        case AT_CLAW:
        case AT_GORE:
        case AT_SNAP:
        case AT_SPLASH:
        case AT_CHERUB:
            noise_factor = 125;
            break;

        case AT_BITE:
        case AT_PECK:
        case AT_CONSTRICT:
            noise_factor = 100;
            break;

        case AT_STING:
        case AT_SPORE:
        case AT_ENGULF:
            noise_factor = 75;
            break;

        case AT_TOUCH:
            noise_factor = 0;
            break;

        case AT_WEAP_ONLY:
            noise_factor = noise_factor;
            break;

        // To prevent compiler warnings.
        case AT_NONE:
        case AT_RANDOM:
        case AT_SHOOT:
            die("Invalid attack flavour for noise_factor");
            break;

        default:
            die("%d Unhandled attack flavour for noise_factor", attk_type);
            break;
        }

        switch(attk_flavour)
        {
        case AF_FIRE:
            noise_factor += 50;
            break;

        case AF_ELEC:
            noise_factor += 100;
            break;

        default:
            break;
        }
    }
    else if (weapon == NULL)
    {
        noise_factor = 150;
    }
}

void melee_attack::check_autoberserk()
{
    if (weapon
        && art_props[ARTP_ANGRY] >= 1
        && !one_chance_in(1 + art_props[ARTP_ANGRY]))
    {
        attacker->go_berserk(false);
    }
}

bool melee_attack::check_unrand_effects()
{
    // If bashing the defender with a wielded unrandart launcher, don't use
    // unrand_entry->fight_func, since that's the function used for
    // launched missiles.
    if (unrand_entry && unrand_entry->fight_func.melee_effects
        && weapon && fires_ammo_type(*weapon) == MI_NONE)
    {
        // Recent merge added damage_done to this method call
        unrand_entry->fight_func.melee_effects(weapon, attacker, defender,
                                               !defender->alive(), damage_done);
        return (!defender->alive());
    }

    return (false);
}

void melee_attack::player_aux_setup(unarmed_attack_type atk)
{
    noise_factor = 100;
    aux_attack.clear();
    aux_verb.clear();
    damage_brand = SPWPN_NORMAL;
    aux_damage = 0;

    switch (atk)
    {
    case UNAT_KICK:
        aux_attack = aux_verb = "kick";
        aux_damage = 5;

        if (player_mutation_level(MUT_HOOVES))
        {
            // Max hoof damage: 10.
            aux_damage += player_mutation_level(MUT_HOOVES) * 5 / 3;
        }
        else if (you.has_usable_talons())
        {
            aux_verb = "claw";

            // Max talon damage: 8.
            aux_damage += player_mutation_level(MUT_TALONS);
        }
        else if (player_mutation_level(MUT_TENTACLE_SPIKE))
        {
            aux_verb = "pierce";

            // Max spike damage: 8.
            aux_damage += player_mutation_level(MUT_TENTACLE_SPIKE);
        }

        break;

    case UNAT_HEADBUTT:
        aux_damage = 5;

        if (player_mutation_level(MUT_BEAK)
            && (!player_mutation_level(MUT_HORNS) || coinflip()))
        {
            aux_attack = aux_verb = "peck";
            aux_damage++;
            noise_factor = 75;
        }
        else
        {
            aux_attack = aux_verb = "headbutt";
            // Minotaurs used to get +5 damage here, now they get
            // +6 because of the horns.
            aux_damage += player_mutation_level(MUT_HORNS) * 3;

            item_def* helmet = you.slot_item(EQ_HELMET);
            if (helmet && is_hard_helmet(*helmet))
            {
                aux_damage += 2;
                if (get_helmet_desc(*helmet) == THELM_DESC_SPIKED
                    || get_helmet_desc(*helmet) == THELM_DESC_HORNED)
                {
                    aux_damage += 3;
                }
            }
        }
        break;

    case UNAT_TAILSLAP:
        aux_attack = aux_verb = "tail-slap";

        aux_damage = 6 * you.has_usable_tail();

        noise_factor = 125;

        if (player_mutation_level(MUT_STINGER) > 0)
        {
            aux_damage += player_mutation_level(MUT_STINGER) * 2 - 1;
            damage_brand = SPWPN_VENOM;
        }

        break;

    case UNAT_PUNCH:
        aux_attack = aux_verb = "punch";
        aux_damage = 5 + you.skill_rdiv(SK_UNARMED_COMBAT, 1, 3);

        if (you.form == TRAN_BLADE_HANDS)
        {
            aux_verb = "slash";
            aux_damage += 6;
            noise_factor = 75;
        }
        else if (you.has_usable_claws())
        {
            aux_verb = "claw";
            aux_damage += roll_dice(you.has_usable_claws(), 3);
        }
        else if (you.has_usable_tentacles())
        {
            // From 1 to 3 bonus damage, so not as good as claws.
            aux_verb = "tentacle-slap";
            aux_damage += you.has_usable_tentacles();
            noise_factor = 125;
        }

        break;

    case UNAT_BITE:
        aux_attack = aux_verb = "bite";
        aux_damage += you.has_usable_fangs() * 2;
        aux_damage += div_rand_round(std::max(you.strength()-10, 0), 5);
        noise_factor = 75;

        // prob of vampiric bite:
        // 1/4 when non-thirsty, 1/2 when thirsty, 100% when
        // bloodless
        if (_vamp_wants_blood_from_monster(defender->as_monster())
            && (you.hunger_state == HS_STARVING
                || you.hunger_state < HS_SATIATED && coinflip()
                || you.hunger_state >= HS_SATIATED && one_chance_in(4)))
        {
            damage_brand = SPWPN_VAMPIRICISM;
        }

        if (player_mutation_level(MUT_ACIDIC_BITE))
        {
            damage_brand = SPWPN_ACID;
            aux_damage += roll_dice(2,4);
        }

        break;

    case UNAT_PSEUDOPODS:
        aux_attack = aux_verb = "bludgeon";
        aux_damage += 4 * you.has_usable_pseudopods();
        noise_factor = 125;
        break;

        // Tentacles both gives you a main attack (replacing punch)
        // and this secondary, high damage attack.
    case UNAT_TENTACLES:
        aux_attack = aux_verb = "squeeze";
        aux_damage = 4 * you.has_usable_tentacles();
        noise_factor = 100; // quieter than slapping
        break;

    default:
        die("unknown aux attack type");
        break;
    }
}


unarmed_attack_type melee_attack::player_aux_choose_baseattack()
{
    unarmed_attack_type baseattack =
        random_choose(UNAT_HEADBUTT, UNAT_KICK, UNAT_PUNCH, UNAT_NO_ATTACK,
                       -1);

    // No punching with a shield or 2-handed wpn, except staves.
    // Octopodes aren't affected by this, though!
    if (you.species != SP_OCTOPODE && baseattack == UNAT_PUNCH && !you.has_usable_offhand())
        baseattack = UNAT_NO_ATTACK;

    // Nagas turn kicks into headbutts.
    if (you.species == SP_NAGA && baseattack == UNAT_KICK)
        baseattack = UNAT_HEADBUTT;

    // Octopodes turn kicks into punches.
    if (you.species == SP_OCTOPODE && baseattack == UNAT_KICK
        && (!player_mutation_level(MUT_TENTACLE_SPIKE) || coinflip()))
    {
        baseattack = UNAT_PUNCH;
    }

    if (_tran_forbid_aux_attack(baseattack))
        baseattack = UNAT_NO_ATTACK;

    return (baseattack);
}

bool melee_attack::player_aux_test_hit()
{
    // XXX We're clobbering did_hit
    did_hit = false;

    const int evasion = defender->melee_evasion(attacker);
    const int helpful_evasion =
        defender->melee_evasion(attacker, EV_IGNORE_HELPLESS);

    if (you.penance[GOD_ELYVILON]
        && god_hates_your_god(GOD_ELYVILON, you.religion))
    {
        simple_god_message(" blocks your attack.", GOD_ELYVILON);
        dec_penance(GOD_ELYVILON, 1);
        return (false);
    }

    bool auto_hit = one_chance_in(30);

    if (!auto_hit && to_hit >= evasion && helpful_evasion > evasion
        && defender_visible)
    {
        defender->props["helpless"] = true;
    }

    if (to_hit >= evasion || auto_hit)
        return (true);

    const int phaseless_evasion =
        defender->melee_evasion(attacker, EV_IGNORE_PHASESHIFT);

    if (to_hit >= phaseless_evasion && defender_visible)
    {
        mprf("Your %s passes through %s as %s momentarily phases out.",
            aux_attack.c_str(),
            defender->name(DESC_THE).c_str(),
            defender->pronoun(PRONOUN).c_str());
    }
    else
    {
        mprf("Your %s misses %s.", aux_attack.c_str(),
             defender->name(DESC_THE).c_str());
    }

    return (false);
}

// Returns true to end the attack round.
bool melee_attack::player_aux_unarmed()
{
    unwind_var<brand_type> save_brand(damage_brand);

    /*
     * baseattack is the auxiliary unarmed attack the player gets
     * for unarmed combat skill, which doesn't require a mutation to use
     * but still needs to pass the other checks in _extra_aux_attack().
     */
    unarmed_attack_type baseattack = UNAT_NO_ATTACK;

    baseattack = player_aux_choose_baseattack();

    for (int i = UNAT_FIRST_ATTACK; i <= UNAT_LAST_ATTACK; ++i)
    {
        if (!defender->alive())
            break;

        unarmed_attack_type atk = static_cast<unarmed_attack_type>(i);

        if (!_extra_aux_attack(atk, (baseattack == atk)))
            continue;

        // Determine and set damage and attack words.
        player_aux_setup(atk);

        to_hit = random2(calc_your_to_hit_unarmed(atk,
                         damage_brand == SPWPN_VAMPIRICISM));

        handle_noise(defender->pos());
        alert_nearby_monsters();

        // [ds] kraken can flee when near death, causing the tentacle
        // the player was beating up to "die" and no longer be
        // available to answer questions beyond this point.
        // handle_noise stirs up all nearby monsters with a stick, so
        // the player may be beating up a tentacle, but the main body
        // of the kraken still gets a chance to act and submerge
        // tentacles before we get here.
        if (!defender->alive())
            return (true);

        if (player_aux_test_hit())
        {
            // Upset the monster.
            behaviour_event(defender->as_monster(), ME_WHACK, MHITYOU);
            if (!defender->alive())
                return (true);

            if (attack_shield_blocked(true))
                continue;
            if (player_aux_apply(atk))
                return (true);
        }
    }

    return (false);
}

bool melee_attack::player_aux_apply(unarmed_attack_type atk)
{
    const int slaying = slaying_bonus(PWPN_DAMAGE);
    did_hit = true;

    aux_damage  = player_aux_stat_modify_damage(aux_damage);

    aux_damage  = random2(aux_damage);

    aux_damage  = player_apply_fighting_skill(aux_damage, true);
    aux_damage += (slaying > -1) ? random2(1 + slaying)
                                 : -random2(1 - slaying);
    aux_damage  = player_apply_misc_modifiers(aux_damage);

    // Clear stab bonus which will be set for the primary weapon attack.
    stab_bonus  = 0;

    const int pre_ac_dmg = aux_damage;
    const int post_ac_dmg = apply_defender_ac(aux_damage);

    aux_damage = post_ac_dmg;
    aux_damage = inflict_damage(aux_damage, BEAM_MISSILE);
    damage_done = aux_damage;

    switch(atk)
    {
        case UNAT_PUNCH:
            apply_bleeding = true;
            break;

        case UNAT_HEADBUTT:
        {
            const int horns = player_mutation_level(MUT_HORNS);
            const int stun = bestroll(std::min(damage_done, 7), 1 + horns);

            defender->as_monster()->speed_increment -= stun;
            break;
        }

        case UNAT_KICK:
        {
            const int hooves = player_mutation_level(MUT_HOOVES);

            if (hooves && pre_ac_dmg > post_ac_dmg)
            {
                const int dmg = bestroll(pre_ac_dmg - post_ac_dmg, hooves);
                // do some of the previously ignored damage in extra-damage
                damage_done += inflict_damage(dmg, BEAM_MISSILE);
            }

            break;
        }

        default:
            break;
    }

    if (damage_done > 0)
    {
        player_announce_aux_hit();

        if (damage_brand == SPWPN_ACID)
        {
            mprf("%s is splashed with acid.",
                 defender->name(DESC_THE).c_str());
                 corrode_monster(defender->as_monster(), &you);
        }

        // TODO: remove this? Unarmed poison attacks?
        if (damage_brand == SPWPN_VENOM && coinflip())
            poison_monster(defender->as_monster(), &you);

        // Normal vampiric biting attack, not if already got stabbing special.
        if (damage_brand == SPWPN_VAMPIRICISM && you.species == SP_VAMPIRE
            && (!stab_attempt || stab_bonus <= 0))
        {
            _player_vampire_draws_blood(defender->as_monster(), damage_done);
        }

        if (atk == UNAT_TAILSLAP && you.species == SP_GREY_DRACONIAN
            && grd(you.pos()) == DNGN_DEEP_WATER
            && feat_is_water(grd(defender->as_monster()->pos())))
        {
            do_knockback(true);
        }
    }
    else // no damage was done
    {
        mprf("You %s %s%s.",
             aux_verb.c_str(),
             defender->name(DESC_THE).c_str(),
             you.can_see(defender) ? ", but do no damage" : "");
    }

    if(defender->props.exists("helpless"))
        defender->props.erase("helpless");

    if (defender->as_monster()->hit_points < 1)
    {
        _monster_die(defender->as_monster(), KILL_YOU, NON_MONSTER);

        return (true);
    }

    return (false);
}

void melee_attack::player_announce_aux_hit()
{
    mprf("You %s %s%s%s",
         aux_verb.c_str(),
         defender->name(DESC_THE).c_str(),
         debug_damage_number().c_str(),
         attack_strength_punctuation().c_str());
}

std::string melee_attack::player_why_missed()
{
    const int ev = defender->melee_evasion(attacker);
    const int combined_penalty =
        attacker_armour_tohit_penalty + attacker_shield_tohit_penalty;
    if (to_hit < ev && to_hit + combined_penalty >= ev)
    {
        const bool armour_miss =
            (attacker_armour_tohit_penalty
             && to_hit + attacker_armour_tohit_penalty >= ev);
        const bool shield_miss =
            (attacker_shield_tohit_penalty
             && to_hit + attacker_shield_tohit_penalty >= ev);

        const item_def *armour = you.slot_item(EQ_BODY_ARMOUR, false);
        const std::string armour_name =
            (armour? armour->name(DESC_BASENAME) : std::string("armour"));

        if (armour_miss && !shield_miss)
            return "Your " + armour_name + " prevents you from hitting ";
        else if (shield_miss && !armour_miss)
            return "Your shield prevents you from hitting ";
        else
            return ("Your shield and " + armour_name
                    + " prevent you from hitting ");
    }

    return ("You" + evasion_margin_adverb() + " miss ");
}

void melee_attack::player_warn_miss()
{
    did_hit = false;
    // Upset only non-sleeping monsters if we missed.
    if (!defender->asleep())
        behaviour_event(defender->as_monster(), ME_WHACK, MHITYOU);

    mprf("%s%s.",
    	 player_why_missed().c_str(),
    	 defender->name(DESC_THE).c_str());
}

int melee_attack::player_stat_modify_damage(int damage)
{
    int dammod = 78;
    const int dam_stat_val = calc_stat_to_dam_base();

    if (dam_stat_val > 11)
        dammod += (random2(dam_stat_val - 11) * 2);
    else if (dam_stat_val < 9)
        dammod -= (random2(9 - dam_stat_val) * 3);

    damage *= dammod;
    damage /= 78;

    return (damage);
}

int melee_attack::player_aux_stat_modify_damage(int damage)
{
    int dammod = 20;
    const int dam_stat_val = calc_stat_to_dam_base();

    if (dam_stat_val > 10)
        dammod += random2(dam_stat_val - 9);
    else if (dam_stat_val < 10)
        dammod -= random2(11 - dam_stat_val);

    damage *= dammod;
    damage /= 20;

    return (damage);
}

int melee_attack::player_apply_weapon_skill(int damage)
{
    if (weapon && (weapon->base_type == OBJ_WEAPONS
                   && !is_range_weapon(*weapon)
                   || weapon->base_type == OBJ_STAVES))
    {
        damage *= 2500 + (random2(you.skill(wpn_skill, 100) + 1));
        damage /= 2500;
    }

    return (damage);
}

int melee_attack::player_apply_fighting_skill(int damage, bool aux)
{
    const int base = aux? 40 : 30;

    damage *= base * 100 + (random2(you.skill(SK_FIGHTING, 100) + 1));
    damage /= base * 100;

    return (damage);
}

int melee_attack::player_apply_misc_modifiers(int damage)
{
    if (you.duration[DUR_MIGHT] || you.duration[DUR_BERSERK])
        damage += 1 + random2(10);

    if (you.species != SP_VAMPIRE && you.hunger_state == HS_STARVING)
        damage -= random2(5);

    // not additive, statues are supposed to be bad with tiny toothpicks but
    // deal crushing blows with big weapons
    if (you.form == TRAN_STATUE)
        damage = div_rand_round(damage * 3, 2);

    return (damage);
}

int melee_attack::player_apply_weapon_bonuses(int damage)
{
    if (weapon && (weapon->base_type == OBJ_WEAPONS
                   && !is_range_weapon(*weapon)
                   || item_is_rod(*weapon)))
    {
        int wpn_damage_plus = weapon->plus2;

        if (item_is_rod(*weapon))
            wpn_damage_plus = (short)weapon->props["rod_enchantment"];

        wpn_damage_plus += slaying_bonus(PWPN_DAMAGE);

        damage += (wpn_damage_plus > -1) ? (random2(1 + wpn_damage_plus))
                                         : (-random2(1 - wpn_damage_plus));

        if (get_equip_race(*weapon) == ISFLAG_DWARVEN
            && you.species == SP_DEEP_DWARF)
        {
            damage += random2(3);
        }

        if (get_equip_race(*weapon) == ISFLAG_ORCISH
            && you.species == SP_HILL_ORC)
        {
            if (you.religion == GOD_BEOGH && !player_under_penance())
            {
                const int orig_damage = damage;

                if (you.piety > 80 || coinflip())
                    damage++;

                damage +=
                    random2avg(
                        div_rand_round(
                            std::min(static_cast<int>(you.piety), 180), 33), 2);

                dprf("Damage: %d -> %d, Beogh bonus: %d",
                     orig_damage, damage, damage - orig_damage);
            }

            if (coinflip())
                damage++;
        }

        // Demonspawn get a damage bonus for demonic weapons.
        if (you.species == SP_DEMONSPAWN && is_demonic(*weapon))
            damage += random2(3);

        if (get_weapon_brand(*weapon) == SPWPN_SPEED)
            damage = div_rand_round(damage * 9, 10);
    }

    return (damage);
}

void melee_attack::player_weapon_auto_id()
{
    if (weapon
        && weapon->base_type == OBJ_WEAPONS
        && !is_range_weapon(*weapon)
        && !item_ident(*weapon, ISFLAG_KNOW_PLUSES)
        && x_chance_in_y(you.skill(wpn_skill, 100), 10000))
    {
        set_ident_flags(*weapon, ISFLAG_KNOW_PLUSES);
        mprf("You are wielding %s.", weapon->name(DESC_A).c_str());
        more();
        you.wield_change = true;
    }
}

int melee_attack::player_stab_weapon_bonus(int damage)
{
    if (weapon && weapon->base_type == OBJ_WEAPONS
        && (weapon->sub_type == WPN_CLUB
            || weapon->sub_type == WPN_SPEAR
            || weapon->sub_type == WPN_TRIDENT
            || weapon->sub_type == WPN_DEMON_TRIDENT
            || weapon->sub_type == WPN_TRISHULA))
    {
        goto ok_weaps;
    }

    switch (wpn_skill)
    {
    case SK_SHORT_BLADES:
    {
        int bonus = (you.dex() * (you.skill(SK_STABBING, 100) + 100)) / 500;

        if (weapon->sub_type != WPN_DAGGER)
            bonus /= 2;

        bonus   = stepdown_value(bonus, 10, 10, 30, 30);
        damage += bonus;
    }
    // fall through
    ok_weaps:
    case SK_LONG_BLADES:
        damage *= 10 + you.skill_rdiv(SK_STABBING) /
                       (stab_bonus + (wpn_skill == SK_SHORT_BLADES ? 0 : 2));
        damage /= 10;
        // fall through
    default:
        damage *= 12 + you.skill_rdiv(SK_STABBING, 1, stab_bonus);
        damage /= 12;
        break;
    }

    return (damage);
}

int melee_attack::player_stab(int damage)
{
    // The stabbing message looks better here:
    if (stab_attempt)
    {
        // Construct reasonable message.
        stab_message();

        practise(EX_WILL_STAB);
    }
    else
    {
        stab_bonus = 0;
        // Ok.. if you didn't backstab, you wake up the neighborhood.
        // I can live with that.
        alert_nearby_monsters();
    }

    if (stab_bonus)
    {
        // Let's make sure we have some damage to work with...
        damage = std::max(1, damage);

        damage = player_stab_weapon_bonus(damage);
    }

    return (damage);
}

void melee_attack::set_attack_verb()
{
    int weap_type = WPN_UNKNOWN;

    if (!weapon)
        weap_type = WPN_UNARMED;
    else if (item_is_staff(*weapon))
        weap_type = WPN_STAFF;
    else if (item_is_rod(*weapon))
        weap_type = WPN_CLUB;
    else if (weapon->base_type == OBJ_WEAPONS)
        weap_type = weapon->sub_type;

    // All weak hits look the same, except for when the attacker
    // has a non-weapon in hand. - bwr
    // Exception: vampire bats only bite to allow for drawing blood.
    if (damage_done < HIT_WEAK
        && (you.species != SP_VAMPIRE || !player_in_bat_form())
        && you.species != SP_FELID)
    {
        if (weap_type != WPN_UNKNOWN)
            attack_verb = "hit";
        else
            attack_verb = "clumsily bash";
    }

    // Take transformations into account, if no weapon is wielded.
    if (weap_type == WPN_UNARMED
        && you.form != TRAN_NONE
        && you.form != TRAN_APPENDAGE)
    {
        switch (you.form)
        {
        case TRAN_SPIDER:
        case TRAN_BAT:
        case TRAN_PIG:
            if (damage_done < HIT_WEAK)
                attack_verb = "hit";
            else if (damage_done < HIT_STRONG)
                attack_verb = "bite";
            else
                attack_verb = "maul";
            break;
        case TRAN_BLADE_HANDS:
            if (damage_done < HIT_WEAK)
                attack_verb = "hit";
            else if (damage_done < HIT_MED)
                attack_verb = "slash";
            else if (damage_done < HIT_STRONG)
                attack_verb = "slice";
            else
                attack_verb = "shred";
            break;
        case TRAN_STATUE:
        case TRAN_LICH:
            if (you.has_usable_claws())
            {
                if (damage_done < HIT_WEAK)
                    attack_verb = "scratch";
                else if (damage_done < HIT_MED)
                    attack_verb = "claw";
                else if (damage_done < HIT_STRONG)
                    attack_verb = "mangle";
                else
                    attack_verb = "eviscerate";
                break;
            }
            else if (you.has_usable_tentacles())
            {
                if (damage_done < HIT_WEAK)
                    attack_verb = "tentacle-slap";
                else if (damage_done < HIT_MED)
                    attack_verb = "bludgeon";
                else if (damage_done < HIT_STRONG)
                    attack_verb = "batter";
                else
                    attack_verb = "thrash";
                break;
            }
            // or fall-through
        case TRAN_ICE_BEAST:
            if (damage_done < HIT_WEAK)
                attack_verb = "hit";
            else if (damage_done < HIT_MED)
                attack_verb = "punch";
            else
                attack_verb = "pummel";
            break;
        case TRAN_DRAGON:
            if (damage_done < HIT_WEAK)
                attack_verb = "hit";
            else if (damage_done < HIT_MED)
                attack_verb = "claw";
            else if (damage_done < HIT_STRONG)
                attack_verb = "bite";
            else
                attack_verb = "maul";
            break;
        case TRAN_NONE:
        case TRAN_APPENDAGE:
            break;
        } // transformations
    }

    // Take normal hits into account.  If the hit is from a weapon with
    // more than one damage type, randomly choose one damage type from
    // it.
    monster_type defender_genus = mons_genus(defender->as_monster()->type);
    switch (weapon ? single_damage_type(*weapon) : -1)
    {
    case DAM_PIERCE:
        if (damage_done < HIT_MED)
            attack_verb = "puncture";
        else if (damage_done < HIT_STRONG)
            attack_verb = "impale";
        else
        {
            if (defender->atype() == ACT_MONSTER
                && defender_visible
                && defender_genus == MONS_HOG)
            {
                attack_verb = "split";
                verb_degree = "like the proverbial pig";
            }
            else
            {
                const char* pierce_desc[][2] = {{"spit", "like a pig"},
                                                {"skewer", "like a kebab"}};
                const int choice = random2(ARRAYSZ(pierce_desc));
                attack_verb = pierce_desc[choice][0];
                verb_degree = pierce_desc[choice][1];
            }
        }
        break;

    case DAM_SLICE:
        if (damage_done < HIT_MED)
            attack_verb = "slash";
        else if (damage_done < HIT_STRONG)
            attack_verb = "slice";
        else if (defender_genus == MONS_OGRE)
        {
            attack_verb = "dice";
            verb_degree = "like an onion";
        }
        else if (defender_genus == MONS_SKELETON_SMALL)
        {
            attack_verb = "fracture";
            verb_degree = "into splinters";
        }
        else
        {
            const char* pierce_desc[][2] = {{"open",    "like a pillowcase"},
                                            {"slice",   "like a ripe choko"},
                                            {"cut",     "into ribbons"}};
            const int choice = random2(ARRAYSZ(pierce_desc));
            attack_verb = pierce_desc[choice][0];
            verb_degree = pierce_desc[choice][1];
        }
        break;

    case DAM_BLUDGEON:
        if (damage_done < HIT_MED)
            attack_verb = one_chance_in(4) ? "thump" : "sock";
        else if (damage_done < HIT_STRONG)
            attack_verb = "bludgeon";
        else if (defender_genus == MONS_SKELETON_SMALL)
        {
            attack_verb = "shatter";
            verb_degree = "into splinters";
        }
        else
        {

            const char* pierce_desc[][2] = {{"crush",   "like a grape"},
                                            {"beat",    "like a drum"},
                                            {"hammer",  "like a gong"},
                                            {"pound",   "like an anvil"}};
            const int choice = random2(ARRAYSZ(pierce_desc));
            attack_verb = pierce_desc[choice][0];
            verb_degree = pierce_desc[choice][1];
        }
        break;

    case DAM_WHIP:
        if (damage_done < HIT_MED)
            attack_verb = "whack";
        else if (damage_done < HIT_STRONG)
            attack_verb = "thrash";
        else
        {
            switch(defender->holiness())
            {
            case MH_HOLY:
            case MH_NATURAL:
            case MH_DEMONIC:
                attack_verb = "punish";
                verb_degree = "causing immense pain";
                break;
            default:
                attack_verb = "devastate";
            }
        }
        break;

    case -1: // unarmed
        if (you.damage_type() == DVORP_CLAWING)
        {
            if (damage_done < HIT_WEAK)
                attack_verb = "scratch";
            else if (damage_done < HIT_MED)
                attack_verb = "claw";
            else if (damage_done < HIT_STRONG)
                attack_verb = "mangle";
            else
                attack_verb = "eviscerate";
        }
        else if (you.damage_type() == DVORP_TENTACLE)
        {
            if (damage_done < HIT_WEAK)
                attack_verb = "tentacle-slap";
            else if (damage_done < HIT_MED)
                attack_verb = "bludgeon";
            else if (damage_done < HIT_STRONG)
                attack_verb = "batter";
            else
                attack_verb = "thrash";
        }
        else
        {
            if (damage_done < HIT_MED)
                attack_verb = "punch";
            else
                attack_verb = "pummel";
        }
        break;

    case WPN_UNKNOWN:
    default:
        attack_verb = "hit";
        break;
    }
}

void melee_attack::player_exercise_combat_skills()
{
    int damage = 10; // Default for unarmed.
    if (weapon && (weapon->base_type == OBJ_WEAPONS
                      && !is_range_weapon(*weapon)
                   || weapon->base_type == OBJ_STAVES))
    {
        damage = property(*weapon, PWPN_DAMAGE);
    }

    const bool helpless = defender->cannot_fight();

    // Slow down the practice of low-damage weapons.
    if (helpless || x_chance_in_y(damage, 20))
        practise(helpless ? EX_WILL_HIT_HELPLESS : EX_WILL_HIT, wpn_skill);
}

/*
 * Applies god conduct for weapon ego
 *
 * Using haste as a chei worshiper, or holy/unholy weapons
 */
void melee_attack::player_weapon_upsets_god()
{
    if (weapon && weapon->base_type == OBJ_WEAPONS)
    {
        if (is_holy_item(*weapon))
            did_god_conduct(DID_HOLY, 1);
        else if (is_demonic(*weapon))
            did_god_conduct(DID_UNHOLY, 1);
        else if (get_weapon_brand(*weapon) == SPWPN_SPEED
                || weapon->sub_type == WPN_QUICK_BLADE)
        {
            did_god_conduct(DID_HASTY, 1);
        }
    }
}

// Returns true if the combat round should end here.
bool melee_attack::player_monattk_hit_effects()
{
    player_weapon_upsets_god();

    // Thirsty vampires will try to use a stabbing situation to draw blood.
    if (you.species == SP_VAMPIRE && you.hunger_state < HS_SATIATED
        && damage_done > 0 && stab_attempt && stab_bonus > 0
        && _player_vampire_draws_blood(defender->as_monster(),
                                       damage_done, true))
    {
        // No further effects.
    }
    else if (you.species == SP_VAMPIRE
             && damage_brand == SPWPN_VAMPIRICISM
             && you.weapon()
             && _player_vampire_draws_blood(defender->as_monster(),
                                            damage_done, false, 5))
    {
        // No further effects.
    }

    if (!defender->alive())
        return (true);

    // These effects apply only to monsters that are still alive:

    // Returns true if a head was cut off *and* the wound was cauterized,
    // in which case the cauterization was the ego effect, so don't burn
    // the hydra some more.
    //
    // Also returns true if the hydra's last head was cut off, in which
    // case nothing more should be done to the hydra.
    if (decapitate_hydra(damage_done))
        return (!defender->alive());

    // These two (staff damage and damage brand) are mutually exclusive!
    player_apply_staff_damage();

    if (needs_message && !special_damage_message.empty())
    {
        mprf("%s", special_damage_message.c_str());
        // Don't do a message-only miscast right after a special damage.
        if (miscast_level == 0)
            miscast_level = -1;
    }

    dprf("Special damage to %s: %d, flavour: %d",
         defender->name(DESC_THE).c_str(),
         special_damage, special_damage_flavour);

    special_damage = inflict_damage(special_damage);

    if (stab_attempt && stab_bonus > 0 && weapon
        && weapon->base_type == OBJ_WEAPONS && weapon->sub_type == WPN_CLUB
        && damage_done + special_damage > random2(defender->get_experience_level())
        && !defender->as_monster()->has_ench(ENCH_CONFUSION)
        && mons_class_is_confusable(defender->type))
    {
        if (defender->as_monster()->add_ench(mon_enchant(ENCH_CONFUSION, 0,
            &you, 20+random2(30)))) // 1-3 turns
        {
            mprf("%s is stunned!", defender->name(DESC_THE).c_str());
        }
    }

    return (false);
}

void melee_attack::_monster_die(monster* mons, killer_type killer,
                                int killer_index)
{
    if (invalid_monster(mons))
        return; // Already died some other way.

    monster_die(mons, killer, killer_index);
}

int melee_attack::fire_res_apply_cerebov_downgrade(int res)
{
    if (weapon && weapon->special == UNRAND_CEREBOV)
        --res;

    return (res);
}

void melee_attack::drain_defender()
{
    if (defender->atype() == ACT_MONSTER && one_chance_in(3))
        return;

    special_damage = 1 + random2(damage_done)
                         / (2 + defender->res_negative_energy());

    if (defender->drain_exp(attacker, true))
    {
        if (defender->atype() == ACT_PLAYER)
            obvious_effect = true;
        else if (defender_visible)
        {
            special_damage_message =
                make_stringf(
                    "%s %s %s!",
                    atk_name(DESC_THE).c_str(),
                    attacker->conj_verb("drain").c_str(),
                    defender_name().c_str());
        }

        attacker->god_conduct(DID_NECROMANCY, 2);
    }
}

void melee_attack::rot_defender(int amount, int immediate)
{
    if (defender->rot(attacker, amount, immediate, true))
    {
        if (defender->atype() == ACT_MONSTER && defender_visible)
        {
            special_damage_message =
                make_stringf(
                    "%s %s!",
                    def_name(DESC_THE).c_str(),
                    amount > 0 ? "rots" : "looks less resilient");
        }
    }
}

bool melee_attack::distortion_affects_defender()
{
    //jmf: blink frogs *like* distortion
    // I think could be amended to let blink frogs "grow" like
    // jellies do {dlb}
    if (defender->atype() == ACT_MONSTER
        && mons_genus(defender->as_monster()->type) == MONS_BLINK_FROG)
    {
        if (one_chance_in(5))
        {
            if (defender_visible)
            {
                special_damage_message =
                    make_stringf("%s %s in the distortional energy.",
                                 def_name(DESC_THE).c_str(),
                                 defender->conj_verb("bask").c_str());
            }

            defender->heal(1 + random2avg(7, 2), true); // heh heh
        }
        return (false);
    }

    if (one_chance_in(3))
    {
        if (defender_visible)
        {
            special_damage_message =
                make_stringf(
                    "Space bends around %s.",
                def_name(DESC_THE).c_str());
        }
        special_damage += 1 + random2avg(7, 2);
        return (false);
    }

    if (one_chance_in(3))
    {
        if (defender_visible)
        {
            special_damage_message =
                make_stringf(
                    "Space warps horribly around %s!",
                    def_name(DESC_THE).c_str());
        }
        special_damage += 3 + random2avg(24, 2);
        return (false);
    }

    if (one_chance_in(3))
    {
        if (defender_visible)
            obvious_effect = true;
        defender->blink();
        return (false);
    }

    // Used to be coinflip() || coinflip() for players, just coinflip()
    // for monsters; this is a compromise. Note that it makes banishment
    // a touch more likely for players, and a shade less likely for
    // monsters.
    if (!one_chance_in(3))
    {
        if (defender_visible)
            obvious_effect = true;
        defender->teleport(coinflip(), one_chance_in(5));
        return (false);
    }

    if (you.level_type != LEVEL_ABYSS && coinflip())
    {
        if (defender->atype() == ACT_PLAYER && attacker_visible
            && weapon != NULL && !is_unrandom_artefact(*weapon)
            && !is_special_unrandom_artefact(*weapon))
        {
            // If the player is being sent to the Abyss by being attacked
            // with a distortion weapon, then we have to ID it before
            // the player goes to Abyss, while the weapon object is
            // still in memory.
            if (is_artefact(*weapon))
                artefact_wpn_learn_prop(*weapon, ARTP_BRAND);
            else
                set_ident_flags(*weapon, ISFLAG_KNOW_TYPE);
        }
        else if (defender_visible)
            obvious_effect = true;

        defender->banish(attacker->name(DESC_PLAIN, true));
        return (true);
    }

    return (false);
}

void melee_attack::antimagic_affects_defender()
{
    if (defender->atype() == ACT_PLAYER)
    {
        int mp_loss = std::min(you.magic_points, random2(damage_done * 2));
        if (!mp_loss)
            return;
        mpr("You feel your power leaking away.", MSGCH_WARN);
        dec_mp(mp_loss);
        obvious_effect = true;
    }
    else if (defender->as_monster()->can_use_spells()
             && !defender->as_monster()->is_priest()
             && !mons_class_flag(defender->type, M_FAKE_SPELLS))
    {
        defender->as_monster()->add_ench(mon_enchant(ENCH_ANTIMAGIC, 0,
                                attacker, // doesn't matter
                                random2(damage_done * 2) * BASELINE_DELAY));
        special_damage_message =
                    apostrophise(defender->name(DESC_THE))
                    + " magic leaks into the air.";
        obvious_effect = true;
    }
}

void melee_attack::pain_affects_defender()
{
    if (defender->res_negative_energy())
        return;

    if (x_chance_in_y(attacker->skill(SK_NECROMANCY) + 1, 8))
    {
        if (defender_visible)
        {
            special_damage_message =
                make_stringf("%s %s in agony.",
                             defender->name(DESC_THE).c_str(),
                             defender->conj_verb("writhe").c_str());
        }
        special_damage += random2(1 + attacker->skill_rdiv(SK_NECROMANCY));
    }

    attacker->god_conduct(DID_NECROMANCY, 4);
}

// TODO: Move this somewhere sane
enum chaos_type
{
    CHAOS_CLONE,
    CHAOS_POLY,
    CHAOS_POLY_UP,
    CHAOS_MAKE_SHIFTER,
    CHAOS_MISCAST,
    CHAOS_RAGE,
    CHAOS_HEAL,
    CHAOS_HASTE,
    CHAOS_INVIS,
    CHAOS_SLOW,
    CHAOS_PARALYSIS,
    CHAOS_PETRIFY,
    NUM_CHAOS_TYPES
};

// XXX: We might want to vary the probabilities for the various effects
// based on whether the source is a weapon of chaos or a monster with
// AF_CHAOS.
void melee_attack::chaos_affects_defender()
{
    const bool mon        = defender->atype() == ACT_MONSTER;
    const bool immune     = mon && mons_immune_magic(defender->as_monster());
    const bool is_natural = mon && defender->holiness() == MH_NATURAL;
    const bool is_shifter = mon && defender->as_monster()->is_shapeshifter();
    const bool can_clone  = mon && mons_clonable(defender->as_monster(), true);
    const bool can_poly   = is_shifter || (defender->can_safely_mutate()
                                           && !immune);
    const bool can_rage   = defender->can_go_berserk();
    const bool can_slow   = !mon || !mons_is_firewood(defender->as_monster());
    const bool can_petrify= mon && (defender->res_petrify() <= 0);

    int clone_chance   = can_clone                      ?  1 : 0;
    int poly_chance    = can_poly                       ?  1 : 0;
    int poly_up_chance = can_poly                && mon ?  1 : 0;
    int shifter_chance = can_poly  && is_natural && mon ?  1 : 0;
    int rage_chance    = can_rage                       ? 10 : 0;
    int miscast_chance = 10;
    int slowpara_chance= can_slow                       ? 10 : 0;
    int petrify_chance = can_slow && can_petrify        ? 10 : 0;

    // Already a shifter?
    if (is_shifter)
        shifter_chance = 0;

    // A chaos self-attack increases the chance of certain effects,
    // due to a short-circuit/feedback/resonance/whatever.
    if (attacker == defender)
    {
        clone_chance   *= 2;
        poly_chance    *= 2;
        poly_up_chance *= 2;
        shifter_chance *= 2;
        miscast_chance *= 2;

        // Inform player that something is up.
        if (!skip_chaos_message && you.see_cell(defender->pos()))
        {
            if (defender->atype() == ACT_PLAYER)
                mpr("You give off a flash of multicoloured light!");
            else if (you.can_see(defender))
            {
                simple_monster_message(defender->as_monster(),
                                       " gives off a flash of "
                                       "multicoloured light!");
            }
            else
                mpr("There is a flash of multicoloured light!");
        }
    }

    // NOTE: Must appear in exact same order as in chaos_type enumeration.
    int probs[NUM_CHAOS_TYPES] =
    {
        clone_chance,   // CHAOS_CLONE
        poly_chance,    // CHAOS_POLY
        poly_up_chance, // CHAOS_POLY_UP
        shifter_chance, // CHAOS_MAKE_SHIFTER
        miscast_chance, // CHAOS_MISCAST
        rage_chance,    // CHAOS_RAGE

        10, // CHAOS_HEAL
        slowpara_chance,// CHAOS_HASTE
        10, // CHAOS_INVIS

        slowpara_chance,// CHAOS_SLOW
        slowpara_chance,// CHAOS_PARALYSIS
        petrify_chance,// CHAOS_PETRIFY
    };

    bolt beam;
    beam.flavour = BEAM_NONE;

    int choice = choose_random_weighted(probs, probs + NUM_CHAOS_TYPES);
#ifdef NOTE_DEBUG_CHAOS_EFFECTS
    std::string chaos_effect = "CHAOS effect: ";
    switch (choice)
    {
    case CHAOS_CLONE:           chaos_effect += "clone"; break;
    case CHAOS_POLY:            chaos_effect += "polymorph"; break;
    case CHAOS_POLY_UP:         chaos_effect += "polymorph PPT_MORE"; break;
    case CHAOS_MAKE_SHIFTER:    chaos_effect += "shifter"; break;
    case CHAOS_MISCAST:         chaos_effect += "miscast"; break;
    case CHAOS_RAGE:            chaos_effect += "berserk"; break;
    case CHAOS_HEAL:            chaos_effect += "healing"; break;
    case CHAOS_HASTE:           chaos_effect += "hasting"; break;
    case CHAOS_INVIS:           chaos_effect += "invisible"; break;
    case CHAOS_SLOW:            chaos_effect += "slowing"; break;
    case CHAOS_PARALYSIS:       chaos_effect += "paralysis"; break;
    case CHAOS_PETRIFY:         chaos_effect += "petrify"; break;
    default:                    chaos_effect += "(other)"; break;
    }

    take_note(Note(NOTE_MESSAGE, 0, 0, chaos_effect.c_str()), true);
#endif

    switch (static_cast<chaos_type>(choice))
    {
    case CHAOS_CLONE:
    {
        ASSERT(can_clone && clone_chance > 0);
        ASSERT(defender->atype() == ACT_MONSTER);

        int clone_idx = clone_mons(defender->as_monster(), true,
                                   &obvious_effect);
        if (clone_idx != NON_MONSTER)
        {
            if (obvious_effect)
            {
                special_damage_message =
                    make_stringf("%s is duplicated!",
                                 def_name(DESC_THE).c_str());
            }

            monster& clone(menv[clone_idx]);
            // The player shouldn't get new permanent followers from cloning.
            if (clone.attitude == ATT_FRIENDLY && !clone.is_summoned())
                clone.mark_summoned(6, true, MON_SUMM_CLONE);

            // Monsters being cloned is interesting.
            xom_is_stimulated(clone.friendly() ? 12 : 25);
        }
        break;
    }

    case CHAOS_POLY:
        ASSERT(can_poly && poly_chance > 0);
        beam.flavour = BEAM_POLYMORPH;
        break;

    case CHAOS_POLY_UP:
        ASSERT(can_poly && poly_up_chance > 0);
        ASSERT(defender->atype() == ACT_MONSTER);

        obvious_effect = you.can_see(defender);
        monster_polymorph(defender->as_monster(), RANDOM_MONSTER, PPT_MORE);
        break;

    case CHAOS_MAKE_SHIFTER:
    {
        ASSERT(can_poly && shifter_chance > 0);
        ASSERT(!is_shifter);
        ASSERT(defender->atype() == ACT_MONSTER);

        obvious_effect = you.can_see(defender);
        defender->as_monster()->add_ench(one_chance_in(3) ?
            ENCH_GLOWING_SHAPESHIFTER : ENCH_SHAPESHIFTER);
        // Immediately polymorph monster, just to make the effect obvious.
        monster_polymorph(defender->as_monster(), RANDOM_MONSTER);

        // Xom loves it if this happens!
        const int friend_factor = defender->as_monster()->friendly() ? 1 : 2;
        const int glow_factor   =
            (defender->as_monster()->has_ench(ENCH_SHAPESHIFTER) ? 1 : 2);
        xom_is_stimulated(64 * friend_factor * glow_factor);
        break;
    }
    case CHAOS_MISCAST:
    {
        int level = defender->get_experience_level();

        // At level == 27 there's a 13.9% chance of a level 3 miscast.
        int level0_chance = level;
        int level1_chance = std::max(0, level - 7);
        int level2_chance = std::max(0, level - 12);
        int level3_chance = std::max(0, level - 17);

        level = random_choose_weighted(
            level0_chance, 0,
            level1_chance, 1,
            level2_chance, 2,
            level3_chance, 3,
            0);

        miscast_level  = level;
        miscast_type   = SPTYP_RANDOM;
        miscast_target = one_chance_in(3) ? attacker : defender;
        break;
    }

    case CHAOS_RAGE:
        ASSERT(can_rage && rage_chance > 0);
        defender->go_berserk(false);
        obvious_effect = you.can_see(defender);
        break;

    case CHAOS_HEAL:
        beam.flavour = BEAM_HEALING;
        break;

    case CHAOS_HASTE:
        beam.flavour = BEAM_HASTE;
        break;

    case CHAOS_INVIS:
        beam.flavour = BEAM_INVISIBILITY;
        break;

    case CHAOS_SLOW:
        beam.flavour = BEAM_SLOW;
        break;

    case CHAOS_PARALYSIS:
        beam.flavour = BEAM_PARALYSIS;
        break;

    case CHAOS_PETRIFY:
        beam.flavour = BEAM_PETRIFY;
        break;

    default:
        die("Invalid chaos effect type");
        break;
    }

    if (beam.flavour != BEAM_NONE)
    {
        beam.glyph        = 0;
        beam.range        = 0;
        beam.colour       = BLACK;
        beam.effect_known = false;

        if (weapon && you.can_see(attacker))
        {
            beam.name = wep_name(DESC_YOUR);
            beam.item = weapon;
        }
        else
            beam.name = atk_name(DESC_THE);

        beam.thrower =
            (attacker->atype() == ACT_PLAYER)          ? KILL_YOU
            : attacker->as_monster()->confused_by_you() ? KILL_YOU_CONF
                                                       : KILL_MON;

        if (beam.thrower == KILL_YOU || attacker->as_monster()->friendly())
            beam.attitude = ATT_FRIENDLY;

        beam.beam_source = attacker->mindex();

        beam.source = defender->pos();
        beam.target = defender->pos();

        beam.damage = dice_def(damage_done + special_damage + aux_damage, 1);

        beam.ench_power = beam.damage.num;

        beam.fire();

        if (you.can_see(defender))
            obvious_effect = beam.obvious_effect;
    }

    if (!you.can_see(attacker))
        obvious_effect = false;
}

void melee_attack::chaos_affects_attacker()
{
    if (miscast_level >= 1 || !attacker->alive())
        return;

    coord_def dest(-1, -1);
	// Prefer to send it under the defender.
	if (defender->alive() && defender->pos() != attacker->pos())
		dest = defender->pos();

    // Move stairs out from under the attacker.
    if (one_chance_in(100) && move_stairs(attacker->pos(), dest))
    {
#ifdef NOTE_DEBUG_CHAOS_EFFECTS
        take_note(Note(NOTE_MESSAGE, 0, 0,
                       "CHAOS affects attacker: move stairs"), true);
#endif
        DID_AFFECT();
    }

    // Dump attacker or items under attacker to another level.
    if (is_valid_shaft_level()
        && (attacker->will_trigger_shaft()
            || igrd(attacker->pos()) != NON_ITEM)
        && one_chance_in(1000))
    {
        (void) attacker->do_shaft();
#ifdef NOTE_DEBUG_CHAOS_EFFECTS
        take_note(Note(NOTE_MESSAGE, 0, 0,
                       "CHAOS affects attacker: shaft effect"), true);
#endif
        DID_AFFECT();
    }

    // Create a colourful cloud.
    if (weapon && one_chance_in(1000))
    {
        mprf("Smoke pours forth from %s!", wep_name(DESC_YOUR).c_str());
        big_cloud(random_smoke_type(), &you, attacker->pos(), 20,
                  4 + random2(8));
#ifdef NOTE_DEBUG_CHAOS_EFFECTS
        take_note(Note(NOTE_MESSAGE, 0, 0,
                       "CHAOS affects attacker: smoke"), true);
#endif
        DID_AFFECT();
    }

    // Make a loud noise.
    if (weapon && player_can_hear(attacker->pos())
        && one_chance_in(200))
    {
        std::string msg = "";
        if (!you.can_see(attacker))
        {
            std::string noise = getSpeakString("weapon_noise");
            if (!noise.empty())
                msg = "You hear " + noise;
        }
        else
        {
            msg = getSpeakString("weapon_noises");
            std::string wepname = wep_name(DESC_YOUR);
            if (!msg.empty())
            {
                msg = replace_all(msg, "@Your_weapon@", wepname);
                msg = replace_all(msg, "@The_weapon@", wepname);
            }
        }

        if (!msg.empty())
        {
            mpr(msg.c_str(), MSGCH_SOUND);
            noisy(15, attacker->pos(), attacker->mindex());
#ifdef NOTE_DEBUG_CHAOS_EFFECTS
            take_note(Note(NOTE_MESSAGE, 0, 0,
                           "CHAOS affects attacker: noise"), true);
#endif
            DID_AFFECT();
        }
    }
}

void melee_attack::do_miscast()
{
    if (miscast_level == -1)
        return;

    ASSERT(miscast_target != NULL);
    ASSERT(miscast_level >= 0 && miscast_level <= 3);
    ASSERT(count_bits(miscast_type) == 1);

    if (!miscast_target->alive())
        return;

    if (miscast_target->atype() == ACT_PLAYER && you.banished)
        return;

    const bool chaos_brand =
        weapon && get_weapon_brand(*weapon) == SPWPN_CHAOS;

    // If the miscast is happening on the attacker's side and is due to
    // a chaos weapon then make smoke/sand/etc pour out of the weapon
    // instead of the attacker's hands.
    std::string hand_str;

    std::string cause = atk_name(DESC_THE);
    int         source;

    const int ignore_mask = ISFLAG_KNOW_CURSE | ISFLAG_KNOW_PLUSES;

    if (attacker->atype() == ACT_PLAYER)
    {
        source = NON_MONSTER;
        if (chaos_brand)
        {
            cause = "a chaos effect from ";
            // Ignore a lot of item flags to make cause as short as possible,
            // so it will (hopefully) fit onto a single line in the death
            // cause screen.
            cause += wep_name(DESC_YOUR,
                              ignore_mask
                              | ISFLAG_COSMETIC_MASK | ISFLAG_RACIAL_MASK);

            if (miscast_target == attacker)
                hand_str = wep_name(DESC_PLAIN, ignore_mask);
        }
    }
    else
    {
        source = attacker->mindex();

        if (chaos_brand && miscast_target == attacker
            && you.can_see(attacker))
        {
            hand_str = wep_name(DESC_PLAIN, ignore_mask);
        }
    }

    MiscastEffect(miscast_target, source, (spschool_flag_type) miscast_type,
                  miscast_level, cause, NH_NEVER, 0, hand_str, false);

    // Don't do miscast twice for one attack.
    miscast_level = -1;
}

// NOTE: random_chaos_brand() and random_chaos_attack_flavour() should
// return a set of effects that are roughly the same, to make it easy
// for chaos_affects_defender() not to do duplicate effects caused
// by the non-chaos brands/flavours they return.
brand_type melee_attack::random_chaos_brand()
{
    brand_type brand = SPWPN_NORMAL;
    // Assuming the chaos to be mildly intelligent, try to avoid brands
    // that clash with the most basic resists of the defender,
    // i.e. its holiness.
    while (true)
    {
        brand = (random_choose_weighted(
                     5, SPWPN_VORPAL,
                    10, SPWPN_FLAMING,
                    10, SPWPN_FREEZING,
                    10, SPWPN_ELECTROCUTION,
                    10, SPWPN_VENOM,
                    10, SPWPN_CHAOS,
                     5, SPWPN_DRAINING,
                     5, SPWPN_VAMPIRICISM,
                     5, SPWPN_HOLY_WRATH,
                     5, SPWPN_ANTIMAGIC,
                     2, SPWPN_CONFUSE,
                     2, SPWPN_DISTORTION,
                     0));

        if (one_chance_in(3))
            break;

        bool susceptible = true;
        switch (brand)
        {
        case SPWPN_FLAMING:
            if (defender->is_fiery())
                susceptible = false;
            break;
        case SPWPN_FREEZING:
            if (defender->is_icy())
                susceptible = false;
            break;
        case SPWPN_ELECTROCUTION:
            if (defender->airborne())
                susceptible = false;
            break;
        case SPWPN_VENOM:
            if (defender->holiness() == MH_UNDEAD)
                susceptible = false;
            break;
        case SPWPN_VAMPIRICISM:
            if (defender->is_summoned())
            {
                susceptible = false;
                break;
            }
            // intentional fall-through
        case SPWPN_DRAINING:
            if (defender->holiness() != MH_NATURAL)
                susceptible = false;
            break;
        case SPWPN_HOLY_WRATH:
            if (defender->holiness() != MH_UNDEAD
                && defender->holiness() != MH_DEMONIC)
            {
                susceptible = false;
            }
            break;
        case SPWPN_CONFUSE:
            if (defender->holiness() == MH_NONLIVING
                || defender->holiness() == MH_PLANT)
            {
                susceptible = false;
            }
            break;
        case SPWPN_ANTIMAGIC:
            if (defender->as_monster() &&
                !defender->as_monster()->can_use_spells())
                susceptible = false;
            break;
        default:
            break;
        }

        if (susceptible)
            break;
    }
#ifdef NOTE_DEBUG_CHAOS_BRAND
    std::string brand_name = "CHAOS brand: ";
    switch (brand)
    {
    case SPWPN_NORMAL:          brand_name += "(plain)"; break;
    case SPWPN_FLAMING:         brand_name += "flaming"; break;
    case SPWPN_FREEZING:        brand_name += "freezing"; break;
    case SPWPN_HOLY_WRATH:      brand_name += "holy wrath"; break;
    case SPWPN_ELECTROCUTION:   brand_name += "electrocution"; break;
    case SPWPN_VENOM:           brand_name += "venom"; break;
    case SPWPN_DRAINING:        brand_name += "draining"; break;
    case SPWPN_DISTORTION:      brand_name += "distortion"; break;
    case SPWPN_VAMPIRICISM:     brand_name += "vampiricism"; break;
    case SPWPN_VORPAL:          brand_name += "vorpal"; break;
    case SPWPN_ANTIMAGIC:       brand_name += "anti-magic"; break;
    // ranged weapon brands
    case SPWPN_FLAME:           brand_name += "flame"; break;
    case SPWPN_FROST:           brand_name += "frost"; break;

    // both ranged and non-ranged
    case SPWPN_CHAOS:           brand_name += "chaos"; break;
    case SPWPN_CONFUSE:         brand_name += "confusion"; break;
    default:                    brand_name += "(other)"; break;
    }

    // Pretty much duplicated by the chaos effect note,
    // which will be much more informative.
    if (brand != SPWPN_CHAOS)
        take_note(Note(NOTE_MESSAGE, 0, 0, brand_name.c_str()), true);
#endif
    return (brand);
}

attack_flavour melee_attack::random_chaos_attack_flavour()
{
    attack_flavour flavours[] =
        {AF_FIRE, AF_COLD, AF_ELEC, AF_POISON_NASTY, AF_VAMPIRIC, AF_DISTORT,
         AF_CONFUSE, AF_CHAOS};
    return (RANDOM_ELEMENT(flavours));
}

bool melee_attack::apply_damage_brand()
{
    bool brand_was_known = false;
    int brand;

    if (weapon)
    {
        if (is_artefact(*weapon))
            brand_was_known = artefact_known_wpn_property(*weapon, ARTP_BRAND);
        else
            brand_was_known = item_type_known(*weapon);
    }
    bool ret = false;

    // Monster resistance to the brand.
    int res = 0;

    special_damage = 0;
    obvious_effect = false;

    if (damage_brand == SPWPN_CHAOS)
        brand = random_chaos_brand();
    else
        brand = damage_brand;

    switch (brand)
    {
    case SPWPN_FLAMING:
        res = fire_res_apply_cerebov_downgrade(defender->res_fire());
        calc_elemental_brand_damage(BEAM_FIRE, res,
                                    defender->is_icy() ? "melt" : "burn");
        defender->expose_to_element(BEAM_FIRE);
        noise_factor += 400 / damage_done;
        break;

    case SPWPN_FREEZING:
        calc_elemental_brand_damage(BEAM_COLD, defender->res_cold(), "freeze");
        defender->expose_to_element(BEAM_COLD);
        break;

    case SPWPN_HOLY_WRATH:
        if (defender->undead_or_demonic())
            special_damage = 1 + (random2(damage_done * 15) / 10);

        if (special_damage && defender_visible)
        {
            special_damage_message =
                make_stringf(
                    "%s %s%s",
                    def_name(DESC_THE).c_str(),
                    defender->conj_verb("convulse").c_str(),
                    special_attack_punctuation().c_str());
        }
        break;

    case SPWPN_ELECTROCUTION:
        noise_factor += 800 / damage_done;
        if (defender->airborne() || defender->res_elec() > 0)
            break;
        else if (one_chance_in(3))
        {
            special_damage_message =
                defender->atype() == ACT_PLAYER?
                   "You are electrocuted!"
                :  "There is a sudden explosion of sparks!";
            special_damage = 10 + random2(15);
            special_damage_flavour = BEAM_ELECTRICITY;

            // Check for arcing in water, and add the final effect.
            const coord_def& pos = defender->pos();

            // We know the defender is neither airborne nor electricity
            // resistant, from above, but is it on water?
            if (feat_is_water(grd(pos)))
            {
                add_final_effect(FINEFF_LIGHTNING_DISCHARGE, attacker, defender,
                                 pos);
            }
        }

        break;

    case SPWPN_ORC_SLAYING:
        if (is_orckind(defender))
        {
            special_damage = 1 + random2(3*damage_done/2);
            if (defender_visible)
            {
                special_damage_message =
                    make_stringf(
                        "%s %s%s",
                        defender->name(DESC_THE).c_str(),
                        defender->conj_verb("convulse").c_str(),
                        special_attack_punctuation().c_str());
            }
        }
        break;

    case SPWPN_DRAGON_SLAYING:
        if (is_dragonkind(defender))
        {
            special_damage = 1 + random2(3*damage_done/2);
            if (defender_visible)
            {
                special_damage_message =
                    make_stringf(
                        "%s %s%s",
                        defender->name(DESC_THE).c_str(),
                        defender->conj_verb("convulse").c_str(),
                        special_attack_punctuation().c_str());
            }
        }
        break;

    case SPWPN_VENOM:
        if (!one_chance_in(4))
        {
            int old_poison;

            if (defender->atype() == ACT_PLAYER)
                old_poison = you.duration[DUR_POISONING];
            else
            {
                old_poison =
                    (defender->as_monster()->get_ench(ENCH_POISON)).degree;
            }

            // Weapons of venom do two levels of poisoning to the player,
            // but only one level to monsters.
            defender->poison(attacker, 2);

            if (defender->atype() == ACT_PLAYER
                   && old_poison < you.duration[DUR_POISONING]
                || defender->atype() != ACT_PLAYER
                   && old_poison <
                      (defender->as_monster()->get_ench(ENCH_POISON)).degree)
            {
                obvious_effect = true;
            }

        }
        break;

    case SPWPN_DRAINING:
        drain_defender();
        break;

    case SPWPN_VORPAL:
        special_damage = 1 + random2(damage_done) / 4;
        // Note: Leaving special_damage_message empty because there isn't one.
        break;

    case SPWPN_VAMPIRICISM:
    {
        if (x_chance_in_y(defender->res_negative_energy(), 3))
            break;

        if (!weapon || defender->holiness() != MH_NATURAL || damage_done < 1
            || attacker->stat_hp() == attacker->stat_maxhp()
            || defender->atype() != ACT_PLAYER
               && defender->as_monster()->is_summoned()
            || attacker == &you && you.duration[DUR_DEATHS_DOOR]
            || one_chance_in(5))
        {
            break;
        }

        obvious_effect = true;

        // Handle weapon effects.
        // We only get here if we've done base damage, so no
        // worries on that score.

        if (attacker->atype() == ACT_PLAYER)
            mpr("You feel better.");
        else if (attacker_visible)
        {
            if (defender->atype() == ACT_PLAYER)
            {
                mprf("%s draws strength from your injuries!",
                     attacker->name(DESC_THE).c_str());
            }
            else
            {
                mprf("%s is healed.",
                     attacker->name(DESC_THE).c_str());
            }
        }

        int hp_boost = weapon->special == UNRAND_VAMPIRES_TOOTH
                     ? damage_done : 1 + random2(damage_done);

        dprf("Vampiric Healing: damage %d, healed %d",
             damage_done, hp_boost);
        attacker->heal(hp_boost);

        attacker->god_conduct(DID_NECROMANCY, 2);
        break;
    }
    case SPWPN_PAIN:
        pain_affects_defender();
        break;

    case SPWPN_DISTORTION:
        ret = distortion_affects_defender();
        break;

    case SPWPN_CONFUSE:
    {
        // This was originally for confusing touch and it doesn't really
        // work on the player, but a monster with a chaos weapon will
        // occassionally come up with this brand. -cao
        if (defender->atype() == ACT_PLAYER)
            break;

        const int hdcheck =
            (defender->holiness() == MH_NATURAL ? random2(30) : random2(22));

        if (mons_class_is_confusable(defender->type)
            && hdcheck >= defender->get_experience_level())
        {
            // Declaring these just to pass to the enchant function.
            bolt beam_temp;
            beam_temp.thrower =
                attacker->atype() == ACT_PLAYER? KILL_YOU : KILL_MON;
            beam_temp.flavour = BEAM_CONFUSION;
            beam_temp.beam_source = attacker->mindex();
            beam_temp.apply_enchantment_to_monster(defender->as_monster());
            obvious_effect = beam_temp.obvious_effect;
        }

        if (attacker->atype() == ACT_PLAYER && damage_brand == SPWPN_CONFUSE)
        {
            ASSERT(you.duration[DUR_CONFUSING_TOUCH]);
            you.duration[DUR_CONFUSING_TOUCH] -= roll_dice(3, 5)
                                                 * BASELINE_DELAY;

            if (you.duration[DUR_CONFUSING_TOUCH] < 1)
                you.duration[DUR_CONFUSING_TOUCH] = 1;
            obvious_effect = false;
        }
        break;
    }

    case SPWPN_CHAOS:
        chaos_affects_defender();
        break;

    case SPWPN_ANTIMAGIC:
        antimagic_affects_defender();
        break;
    }

    if (damage_brand == SPWPN_CHAOS && brand != SPWPN_CHAOS && !ret
        && miscast_level == -1 && one_chance_in(20))
    {
        miscast_level  = 0;
        miscast_type   = SPTYP_RANDOM;
        miscast_target = coinflip() ? attacker : defender;
    }

    if (attacker->atype() == ACT_PLAYER && damage_brand == SPWPN_CHAOS)
    {
        // If your god objects to using chaos, then it makes the
        // brand obvious.
        if (did_god_conduct(DID_CHAOS, 2 + random2(3), brand_was_known))
            obvious_effect = true;
    }
    if (!obvious_effect)
        obvious_effect = !special_damage_message.empty();

    if (needs_message && !special_damage_message.empty())
    {
        mprf("%s", special_damage_message.c_str());
        // Don't do message-only miscasts along with a special
        // damage message.
        if (miscast_level == 0)
            miscast_level = -1;
    }

    if (special_damage > 0)
        inflict_damage(special_damage, special_damage_flavour, true);

    if (obvious_effect && attacker_visible && weapon != NULL)
    {
        if (is_artefact(*weapon))
            artefact_wpn_learn_prop(*weapon, ARTP_BRAND);
        else
            set_ident_flags(*weapon, ISFLAG_KNOW_TYPE);
    }

    return (ret);
}


// XXX:
//  * Noise should probably scale non-linearly with damage_done, and
//    maybe even non-linearly with noise_factor.
//
//  * Damage reduction via armour of the defender reduces noise,
//    but shouldn't.
//
//  * Damage reduction because of negative damage modifiers on the
//    weapon reduce noise, but probably shouldn't.
//
//  * Might want a different formula for noise generated by the
//    player.
//
//  Ideas:
//  * Each weapon type has a noise rating, like it does an accuracy
//    rating and base delay.
//
//  * For player, stealth skill and/or weapon skillr reducing noise.
//
//  * Randart property to make randart weapons louder or softer when
//    they hit.
void melee_attack::handle_noise(const coord_def & pos)
{
    // Successful stabs make no noise.
    if (stab_attempt)
    {
        noise_factor = 0;
        return;
    }

    int level = noise_factor * damage_done / 100 / 4;

    if (noise_factor > 0)
        level = std::max(1, level);

    // Cap melee noise at shouting volume.
    level = std::min(12, level);

    if (level > 0)
        noisy(level, pos, attacker->mindex());

    noise_factor = 0;
}

// Returns true if the attack cut off a head *and* cauterized it.
bool melee_attack::chop_hydra_head(int dam,
                                   int dam_type,
                                   brand_type wpn_brand)
{
    // Monster attackers have only a 25% chance of making the
    // chop-check to prevent runaway head inflation.
    if (attacker->atype() == ACT_MONSTER && !one_chance_in(4))
        return (false);

    if ((dam_type == DVORP_SLICING || dam_type == DVORP_CHOPPING
            || dam_type == DVORP_CLAWING)
        && dam > 0
        && (dam >= 4 || wpn_brand == SPWPN_VORPAL || coinflip()))
    {
        const char *verb = NULL;

        if (dam_type == DVORP_CLAWING)
        {
            static const char *claw_verbs[] = { "rip", "tear", "claw" };
            verb = RANDOM_ELEMENT(claw_verbs);
        }
        else
        {
            static const char *slice_verbs[] =
            {
                "slice", "lop", "chop", "hack"
            };
            verb = RANDOM_ELEMENT(slice_verbs);
        }

        if (defender->as_monster()->number == 1) // will be zero afterwards
        {
            if (defender_visible)
            {
                mprf("%s %s %s's last head off!",
                     atk_name(DESC_THE).c_str(),
                     attacker->conj_verb(verb).c_str(),
                     def_name(DESC_THE).c_str());
            }
            defender->as_monster()->number--;

            if (!defender->is_summoned())
                bleed_onto_floor(defender->pos(), defender->type,
                                 defender->as_monster()->hit_points, true);

            defender->hurt(attacker, defender->as_monster()->hit_points);

            return (true);
        }
        else
        {
            if (defender_visible)
            {
                mprf("%s %s one of %s's heads off!",
                     atk_name(DESC_THE).c_str(),
                     attacker->conj_verb(verb).c_str(),
                     def_name(DESC_THE).c_str());
            }
            defender->as_monster()->number--;

            // Only living hydras get to regenerate heads.
            if (defender->holiness() == MH_NATURAL)
            {
                unsigned int limit = 20;
                if (defender->type == MONS_LERNAEAN_HYDRA)
                    limit = 27;

                if (wpn_brand == SPWPN_FLAMING)
                {
                    if (defender_visible)
                        mpr("The flame cauterises the wound!");
                    return (true);
                }
                else if (defender->as_monster()->number < limit - 1)
                {
                    simple_monster_message(defender->as_monster(),
                                           " grows two more!");
                    defender->as_monster()->number += 2;
                    defender->heal(8 + random2(8), true);
                }
            }
        }
    }

    return (false);
}

bool melee_attack::decapitate_hydra(int dam, int damage_type)
{
    if (defender->atype() == ACT_MONSTER
        && defender->as_monster()->has_hydra_multi_attack()
        && defender->type != MONS_SPECTRAL_THING)
    {
        const int dam_type = (damage_type != -1) ? damage_type
                                                 : attacker->damage_type();
        const brand_type wpn_brand = attacker->damage_brand();

        return chop_hydra_head(dam, dam_type, wpn_brand);
    }
    return (false);
}

void melee_attack::player_sustain_passive_damage()
{
    if (mons_class_flag(defender->type, M_ACID_SPLASH))
        weapon_acid(5);
}

int melee_attack::player_staff_damage(skill_type skill)
{
    if (x_chance_in_y(you.skill(SK_EVOCATIONS, 200)
                    + you.skill(skill, 100), 3000))
    {
        return random2((you.skill(skill, 100)
                      + you.skill(SK_EVOCATIONS, 50)) / 80);
    }
    return 0;
}

void melee_attack::player_apply_staff_damage()
{
    special_damage = 0;

    if (!weapon || !item_is_staff(*weapon))
        return;

    if (random2(15) > you.skill(SK_EVOCATIONS))
        return;

    switch (weapon->sub_type)
    {
    case STAFF_AIR:
        if (damage_done + you.skill_rdiv(SK_AIR_MAGIC) <= random2(20))
            break;

        special_damage =
            resist_adjust_damage(defender,
                                 BEAM_ELECTRICITY,
                                 defender->res_elec(),
                                 player_staff_damage(SK_AIR_MAGIC));

        if (special_damage)
        {
            special_damage_message =
                make_stringf("%s is electrocuted!",
                             defender->name(DESC_THE).c_str());
            special_damage_flavour = BEAM_ELECTRICITY;
        }

        break;

    case STAFF_COLD:
        special_damage =
            resist_adjust_damage(defender,
                                 BEAM_COLD,
                                 defender->res_cold(),
                                 player_staff_damage(SK_ICE_MAGIC));

        if (special_damage)
        {
            special_damage_message =
                make_stringf(
                    "You freeze %s!",
                    defender->name(DESC_THE).c_str());
        }
        break;

    case STAFF_EARTH:
        special_damage = player_staff_damage(SK_EARTH_MAGIC);
        special_damage = apply_defender_ac(special_damage);

        if (special_damage > 0)
        {
            special_damage_message =
                make_stringf(
                    "You crush %s!",
                    defender->name(DESC_THE).c_str());
        }
        break;

    case STAFF_FIRE:
        special_damage =
            resist_adjust_damage(defender,
                                 BEAM_FIRE,
                                 defender->res_fire(),
                                 player_staff_damage(SK_FIRE_MAGIC));

        if (special_damage)
        {
            special_damage_message =
                make_stringf(
                    "You burn %s!",
                    defender->name(DESC_THE).c_str());
        }
        break;

    case STAFF_POISON:
    {
        // Base chance at 50% -- like mundane weapons.
        if (coinflip() || x_chance_in_y(you.skill(SK_POISON_MAGIC, 10), 80))
        {
            defender->poison(attacker, 2, defender->has_lifeforce()
                & x_chance_in_y(you.skill(SK_POISON_MAGIC, 10), 160));
        }
        break;
    }

    case STAFF_DEATH:
        if (defender->res_negative_energy())
            break;

        special_damage = player_staff_damage(SK_NECROMANCY);

        if (special_damage)
        {
            special_damage_message =
                make_stringf(
                    "%s convulses in agony!",
                    defender->name(DESC_THE).c_str());

            did_god_conduct(DID_NECROMANCY, 4);
        }
        break;

    case STAFF_SUMMONING:
        if (!defender->is_summoned())
            break;

        if (x_chance_in_y(you.skill(SK_EVOCATIONS, 20)
                        + you.skill(SK_SUMMONINGS, 10), 300))
        {
            cast_abjuration((you.skill(SK_SUMMONINGS, 100)
                            + you.skill(SK_EVOCATIONS, 50)) / 80,
                            defender->as_monster());
        }
        break;

    case STAFF_POWER:
    case STAFF_CHANNELING:
    case STAFF_CONJURATION:
    case STAFF_ENCHANTMENT:
    case STAFF_ENERGY:
    case STAFF_WIZARDRY:
        break;

    default:
        mpr("You're wielding some staff I've never heard of! (melee_attack.cc)",
            MSGCH_ERROR);
        break;
    }

    if (special_damage > 0)
    {
        if (!item_type_known(*weapon))
        {
            set_ident_flags(*weapon, ISFLAG_KNOW_TYPE);
            set_ident_type(*weapon, ID_KNOWN_TYPE);

            mprf("You are wielding %s.", weapon->name(DESC_A).c_str());
            more();
            you.wield_change = true;
        }
    }
}

bool melee_attack::player_check_monster_died()
{
    if (!defender->alive())
    {
        // note: doesn't take account of special weapons, etc.
        dprf("Hit for %d.", damage_done);

        player_monattk_hit_effects();

        _monster_die(defender->as_monster(), KILL_YOU, NON_MONSTER);

        return (true);
    }

    return (false);
}

/* Calculate the to-hit for an attacker
 *
 * @param random deterministic or stochastic calculation(s)
 */
int melee_attack::calc_to_hit(bool random)
{
    const int hd_mult = mons_class_flag(attacker->type, M_FIGHTER) ? 25 : 15;
    int mhit = attacker->atype() == ACT_PLAYER ?
                15 + (calc_stat_to_hit_base() / 2)
              : 18 + attacker->get_experience_level() * hd_mult / 10;

    const int base_hit = mhit;

    if (wearing_amulet(AMU_INACCURACY))
        mhit -= 5;

    // This if statement is temporary, it should be removed when the
    // implementation of a more universal (and elegant) to-hit calculation
    // is designed. The actual code is copied from the old mons_to_hit and
    // player_to_hit methods.
    if(attacker->atype() == ACT_PLAYER)
    {
        // fighting contribution
        mhit += maybe_random_div(you.skill(SK_FIGHTING, 100), 100, random);

        // weapon skill contribution
        if (weapon)
        {
            if (wpn_skill != SK_FIGHTING)
            {
                if (you.skill(wpn_skill) < 1 && player_in_a_dangerous_place())
                    xom_is_stimulated(10); // Xom thinks that is mildly amusing.

                mhit += maybe_random_div(you.skill(wpn_skill, 100), 100,
                                         random);
            }
        }
        else
        {                       // ...you must be unarmed
            mhit += species_has_claws(you.species) ? 4 : 2;

            mhit += maybe_random2(1 + you.skill(SK_UNARMED_COMBAT),
                                         random);
        }

        // weapon bonus contribution
        if (weapon)
        {
            if (weapon->base_type == OBJ_WEAPONS && !is_range_weapon(*weapon))
            {
                mhit += weapon->plus;
                mhit += property(*weapon, PWPN_HIT);

                if (get_equip_race(*weapon) == ISFLAG_ELVEN
                    && player_genus(GENPC_ELVEN))
                {
                    mhit += (random && coinflip() ? 2 : 1);
                }
                else if (get_equip_race(*weapon) == ISFLAG_ORCISH
                         && you.religion == GOD_BEOGH && !player_under_penance())
                {
                    mhit++;
                }

            }
            else if (weapon->base_type == OBJ_STAVES)
            {
                // magical staff
                mhit += property(*weapon, PWPN_HIT);

                if (item_is_rod(*weapon))
                    mhit += (short)weapon->props["rod_enchantment"];
            }
        }

        // slaying bonus
        mhit += slaying_bonus(PWPN_HIT);

        // hunger penalty
        if (you.hunger_state == HS_STARVING)
            mhit -= 3;

        // armour penalty
        mhit -= (attacker_armour_tohit_penalty
                        + attacker_shield_tohit_penalty);

        //mutation
        if (player_mutation_level(MUT_EYEBALLS))
            mhit += 2 * player_mutation_level(MUT_EYEBALLS) + 1;

        // hit roll
        mhit = maybe_random2(mhit, random);

        if (weapon && wpn_skill == SK_SHORT_BLADES && you.duration[DUR_SURE_BLADE])
        {
            int turn_duration = you.duration[DUR_SURE_BLADE] / BASELINE_DELAY;
            mhit += 5 +
                (random ? random2limit(turn_duration, 10) :
                 turn_duration / 2);
        }

        // other stuff
        if (!weapon)
        {
            if (you.duration[DUR_CONFUSING_TOUCH])
            {
                // Just trying to touch is easier that trying to damage.
                mhit += maybe_random2(you.dex(), random);
            }
        }

        // If no defender, we're calculating to-hit for debug-display
        // purposes, so don't drop down to defender code below (there is none)
        if (defender == NULL)
        	return (mhit);
    }
    else
    {
        if (weapon
            && (weapon->base_type == OBJ_WEAPONS
                && !is_range_weapon(*weapon)
                || weapon->base_type == OBJ_STAVES))
        {
            mhit += weapon->plus + property(*weapon, PWPN_HIT);
        }

        if(weapon && item_is_rod(*weapon))
            mhit += (short)weapon->props["rod_enchantment"];
    }

    if (attacker->confused())
        mhit -= 5;

    if (!defender->visible_to(attacker))
        mhit = mhit * 75 / 100;
    else
    {
        // This can only help if you're visible!
        if (defender->atype() == ACT_PLAYER
            && player_mutation_level(MUT_TRANSLUCENT_SKIN) >= 3)
            mhit -= 5;

        if (defender->backlit(true, false))
            mhit += 2 + random2(8);
        else if (!attacker->nightvision()
                 && defender->umbra(true, true))
            mhit -= 2 + random2(4);
    }

    dprf("%s: Base to-hit: %d, Final to-hit: %d",
         attacker->name(DESC_PLAIN).c_str(),
         base_hit, mhit);

    return (mhit);
}

/*
 * Calculate the attack delay for attacker
 *
 * Although we're trying to unify the code paths for players and monsters,
 * we don't have a unified system in place for calculating attack_delay using
 * the same base methods so we still have to check the atype of the attacker
 * and fork on the logic.
 *
 * @param random deterministic or stochastic calculation(s)
 */
int melee_attack::calc_attack_delay(bool random)
{
    if(attacker->atype() == ACT_PLAYER)
    {
        random_var attack_delay = weapon ? player_weapon_speed()
                                         : player_unarmed_speed();

        if (attacker_shield_penalty)
        {
            if (weapon && hands == HANDS_HALF)
                attack_delay += rv::roll_dice(1, attacker_shield_penalty);
            else
            {
                attack_delay += rv::min(rv::random2(1 + attacker_shield_penalty),
                                        rv::random2(1 + attacker_shield_penalty));
            }
        }

        attack_delay = rv::max(attack_delay, constant(3));
        int final_delay = random ? attack_delay.roll() : attack_delay.expected();

        if (you.duration[DUR_FINESSE])
        {
            ASSERT(!you.duration[DUR_BERSERK]);
            // Need to undo haste by hand.
            if (you.duration[DUR_HASTE])
                you.time_taken = haste_mul(you.time_taken);
            you.time_taken = div_rand_round(you.time_taken, 2);
        }

        dprf("Weapon speed: %d; min: %d; attack time: %d",
             final_delay, min_delay, you.time_taken);

        return (std::max(2, div_rand_round(you.time_taken * final_delay, 10)));
    }
    else
    {
        return (weapon ? property(*weapon, PWPN_SPEED) : 0);
    }

    return (0);
}

void melee_attack::player_stab_check()
{
    if (you.stat_zero[STAT_DEX] || you.confused())
    {
        stab_attempt = false;
        stab_bonus = 0;
        return;
    }

    const unchivalric_attack_type uat = is_unchivalric_attack(&you, defender);
    stab_attempt = (uat != UCAT_NO_ATTACK);
    const bool roll_needed = (uat != UCAT_SLEEPING && uat != UCAT_PARALYSED);

    int roll = 100;
    if (uat == UCAT_INVISIBLE && !mons_sense_invis(defender->as_monster()))
        roll -= 10;

    switch (uat)
    {
    case UCAT_NO_ATTACK:
        stab_bonus = 0;
        break;
    case UCAT_SLEEPING:
    case UCAT_PARALYSED:
        stab_bonus = 1;
        break;
    case UCAT_HELD_IN_NET:
    case UCAT_PETRIFYING:
    case UCAT_PETRIFIED:
        stab_bonus = 2;
        break;
    case UCAT_INVISIBLE:
    case UCAT_CONFUSED:
    case UCAT_FLEEING:
    case UCAT_ALLY:
        stab_bonus = 4;
        break;
    case UCAT_DISTRACTED:
        stab_bonus = 6;
        break;
    }

    // See if we need to roll against dexterity / stabbing.
    if (stab_attempt && roll_needed)
    {
        stab_attempt = x_chance_in_y(you.skill_rdiv(SK_STABBING) + you.dex() + 1,
                                     roll);
    }
}

random_var melee_attack::player_weapon_speed()
{
    random_var attack_delay = constant(15);

    if (weapon && (weapon->base_type == OBJ_WEAPONS
                   && !is_range_weapon(*weapon)
                   || weapon->base_type == OBJ_STAVES))
    {
        attack_delay = constant(property(*weapon, PWPN_SPEED));
        attack_delay -= div_rand_round(constant(you.skill(wpn_skill, 10)), 20);

        min_delay = property(*weapon, PWPN_SPEED) / 2;

        // Short blades can get up to at least unarmed speed.
        if (wpn_skill == SK_SHORT_BLADES && min_delay > 5)
            min_delay = 5;

        // All weapons have min delay 7 or better
        if (min_delay > 7)
            min_delay = 7;

        // never go faster than speed 3 (ie 3 attacks per round)
        if (min_delay < 3)
            min_delay = 3;

        // apply minimum to weapon skill modification
        attack_delay = rv::max(attack_delay, min_delay);

        if (weapon->base_type == OBJ_WEAPONS
            && damage_brand == SPWPN_SPEED)
        {
            attack_delay = (attack_delay + constant(1)) / 2;
        }
    }

    return (attack_delay);
}

random_var melee_attack::player_unarmed_speed()
{
    random_var unarmed_delay =
        rv::max(constant(10),
                 (rv::roll_dice(1, 10) +
                  rv::roll_dice(2, attacker_body_armour_penalty)));

    // Not even bats can attack faster than this.
    min_delay = 5;

    // Unarmed speed.
    if (you.burden_state == BS_UNENCUMBERED)
    {
        unarmed_delay =
            rv::max(unarmed_delay
                     - div_rand_round(constant(you.skill(SK_UNARMED_COMBAT, 10)),
                                player_in_bat_form() ? 30 : 50),
                    constant(min_delay));
    }

    return (unarmed_delay);
}

///////////////////////////////////////////////////////////////////////////

bool melee_attack::attack_warded_off()
{
    // [dshaligram] Note: warding is no longer a simple 50% chance.
    const int warding = defender->warding();
    if (warding
        && attacker->is_summoned()
        && attacker->as_monster()->check_res_magic(warding) <= 0)
    {
        if (needs_message)
        {
            mprf("%s tries to attack %s, but flinches away.",
                 atk_name(DESC_THE).c_str(),
                 defender_name().c_str());
        }

        if (defender == &you
            && player_equip(EQ_AMULET, AMU_WARDING, true)
            && !player_equip(EQ_STAFF, STAFF_SUMMONING, true))
        {
            item_def *amu = you.slot_item(EQ_AMULET);
            ASSERT(amu);
            wear_id_type(*amu);
        }
        return (true);
    }

    return (false);
}

bool melee_attack::attack_shield_blocked(bool verbose)
{
    if (!defender_shield && defender->atype() != ACT_PLAYER)
        return (false);

    if (defender->incapacitated())
        return (false);

    const int con_block = random2(attacker->shield_bypass_ability(to_hit)
                                  + defender->shield_block_penalty());
    int pro_block = defender->shield_bonus();

    if (!attacker->visible_to(defender))
        pro_block /= 3;

    dprf("Defender: %s, Pro-block: %d, Con-block: %d",
         def_name(DESC_PLAIN).c_str(), pro_block, con_block);

    if (pro_block >= con_block)
    {
        perceived_attack = true;

        if (needs_message && verbose)
        {
            mprf("%s %s %s attack.",
                 def_name(DESC_THE).c_str(),
                 defender->conj_verb("block").c_str(),
                 atk_name(DESC_ITS).c_str());
        }

        defender->shield_block_succeeded(attacker);

        return (true);
    }

    return (false);
}

std::string melee_attack::mons_attack_verb()
{
    static const char *klown_attack[] =
    {
        "hit",
        "poke",
        "prod",
        "flog",
        "pound",
        "slap",
        "tickle",
        "defenestrate",
        "sucker-punch",
        "elbow",
        "pinch",
        "strangle-hug",
        "squeeze",
        "tease",
        "eye-gouge",
        "karate-kick",
        "headlock",
        "wrestle",
        "trip-wire",
        "kneecap"
    };

    if (attacker->type == MONS_KILLER_KLOWN && attk_type == AT_HIT)
        return (RANDOM_ELEMENT(klown_attack));

    if (mons_is_feat_mimic(attacker->type))
    {
        const dungeon_feature_type feat = get_mimic_feat(attacker->as_monster());
        if (feat_is_door(feat))
            attk_type = AT_SNAP;
        else if (feat_is_fountain(feat))
            attk_type = AT_SPLASH;
    }

    if (attk_type == AT_TENTACLE_SLAP
        && (attacker->type == MONS_KRAKEN_TENTACLE
            || attacker->type == MONS_ELDRITCH_TENTACLE))
    {
        return ("slap");
    }

    static const char *attack_types[] =
    {
        "",
        "hit",         // including weapon attacks
        "bite",
        "sting",

        // spore
        "hit",

        "touch",
        "engulf",
        "claw",
        "peck",
        "headbutt",
        "punch",
        "kick",
        "tentacle-slap",
        "tail-slap",
        "gore",
        "constrict",
        "trample",
        "trunk-slap",
        "snap closed at",
        "splash"
    };

    ASSERT(attk_type <
           static_cast<int>(sizeof(attack_types) / sizeof(const char *)));
    return (attack_types[attk_type]);
}

std::string melee_attack::mons_attack_desc()
{
    if (!you.can_see(attacker))
        return ("");

    std::string ret;
    int dist = (attacker->pos() - defender->pos()).abs();
    if (dist > 2)
    {
        ASSERT(attk_flavour == AF_REACH || weapon && weapon_reach(*weapon));
        ret = "from afar";
    }

    if (weapon && attacker->type != MONS_DANCING_WEAPON)
        ret += " with " + weapon->name(DESC_A);

    return ret;
}

void melee_attack::announce_hit()
{
    if (!needs_message)
        return;

    if (attacker->atype() == ACT_MONSTER)
    {
        mprf("%s %s %s%s%s%s",
             atk_name(DESC_THE).c_str(),
             attacker->conj_verb(mons_attack_verb()).c_str(),
             defender_name().c_str(),
             debug_damage_number().c_str(),
             mons_attack_desc().c_str(),
             attack_strength_punctuation().c_str());
    }
    else
    {
        if (!verb_degree.empty() && verb_degree[0] != ' ')
            verb_degree = " " + verb_degree;

        mprf("You %s %s%s%s%s",
             attack_verb.c_str(),
             defender->name(DESC_THE).c_str(),
             verb_degree.c_str(), debug_damage_number().c_str(),
             attack_strength_punctuation().c_str());
    }
}

void melee_attack::mons_do_poison()
{
    if (attk_flavour == AF_POISON_NASTY
        || one_chance_in(15 + 5 * (attk_flavour == AF_POISON ? 1 : 0))
        || (damage_done > 1
            && one_chance_in(attk_flavour == AF_POISON ? 4 : 3)))
    {
        int amount = 1;
        if (attk_flavour == AF_POISON_NASTY)
            amount++;
        else if (attk_flavour == AF_POISON_MEDIUM)
            amount += random2(3);
        else if (attk_flavour == AF_POISON_STRONG)
            amount += roll_dice(2, 5);

        if (!defender->poison(attacker, amount))
            return;

        if (needs_message)
        {
			mprf("%s poisons %s!",
				 atk_name(DESC_THE).c_str(),
				 defender_name().c_str());
        }
    }
}

void melee_attack::mons_do_napalm()
{
    if (defender->res_sticky_flame())
        return;

    if (one_chance_in(20) || (damage_done > 2 && one_chance_in(3)))
    {
        if (needs_message)
        {
            mprf("%s %s covered in liquid flames%s",
                 def_name(DESC_THE).c_str(),
                 defender->conj_verb("are").c_str(),
                 special_attack_punctuation().c_str());
        }

        if (defender->atype() == ACT_PLAYER)
            napalm_player(random2avg(7, 3) + 1);
        else
        {
            napalm_monster(
                defender->as_monster(),
                attacker,
                std::min(4, 1 + random2(attacker->get_experience_level())/2));
        }
    }
}

void melee_attack::splash_defender_with_acid(int strength)
{
    if (defender->atype() == ACT_PLAYER)
    {
        mpr("You are splashed with acid!");
        splash_with_acid(strength);
    }
    else
    {
        special_damage += roll_dice(2, 4);
        if (defender_visible)
            mprf("%s is splashed with acid.", defender->name(DESC_THE).c_str());
        corrode_monster(defender->as_monster(), attacker);
    }
}

void melee_attack::mons_apply_attack_flavour()
{
    // Most of this is from BWR 4.1.2.

    attack_flavour flavour = attk_flavour;
    if (flavour == AF_CHAOS)
        flavour = random_chaos_attack_flavour();

    switch (flavour)
    {
    default:
        break;

    case AF_MUTATE:
        if (one_chance_in(4))
            defender->mutate();
        break;

    case AF_POISON:
    case AF_POISON_NASTY:
    case AF_POISON_MEDIUM:
    case AF_POISON_STRONG:
        mons_do_poison();
        break;

    case AF_POISON_STR:
    case AF_POISON_INT:
    case AF_POISON_DEX:
        if (defender->poison(attacker, roll_dice(1, 3)))
        {
            if (one_chance_in(4))
            {
                stat_type drained_stat = (flavour == AF_POISON_STR ? STAT_STR :
                                          flavour == AF_POISON_INT ? STAT_INT
                                                                   : STAT_DEX);
                defender->drain_stat(drained_stat, 1, attacker);
            }
        }
        break;

    case AF_ROT:
        if (one_chance_in(20) || (damage_done > 2 && one_chance_in(3)))
            rot_defender(2 + random2(3), damage_done > 5 ? 1 : 0);
        break;

    case AF_DISEASE:
        defender->sicken(50 + random2(100));
        break;

    case AF_FIRE:
        if (attacker->type == MONS_FIRE_VORTEX)
            attacker->as_monster()->suicide(-10);

        special_damage =
            resist_adjust_damage(defender,
                                 BEAM_FIRE,
                                 defender->res_fire(),
                                 attacker->get_experience_level()
                                 + random2(attacker->get_experience_level()));

        if (needs_message && special_damage)
        {
            mprf("%s %s engulfed in flames%s",
                 def_name(DESC_THE).c_str(),
                 defender->conj_verb("are").c_str(),
                 special_attack_punctuation().c_str());
        }

        defender->expose_to_element(BEAM_FIRE, 2);
        break;

    case AF_COLD:
        special_damage =
            resist_adjust_damage(defender,
                                 BEAM_COLD,
                                 defender->res_cold(),
                                 attacker->get_experience_level() +
                                 random2(2 * attacker->get_experience_level()));

        if (needs_message && special_damage)
        {
            mprf("%s %s %s%s",
                 atk_name(DESC_THE).c_str(),
                 attacker->conj_verb("freeze").c_str(),
                 defender_name().c_str(),
                 special_attack_punctuation().c_str());

        }

        defender->expose_to_element(BEAM_COLD, 2);
        break;

    case AF_ELEC:
        special_damage =
            resist_adjust_damage(
                defender,
                BEAM_ELECTRICITY,
                defender->res_elec(),
                attacker->get_experience_level() +
                random2(attacker->get_experience_level() / 2));
        special_damage_flavour = BEAM_ELECTRICITY;

        if (defender->airborne())
            special_damage = special_damage * 2 / 3;

        if (needs_message && special_damage)
        {
            mprf("%s %s %s%s",
                 atk_name(DESC_THE).c_str(),
                 attacker->conj_verb("shock").c_str(),
                 defender_name().c_str(),
                 special_attack_punctuation().c_str());
        }

        dprf("Shock damage: %d", special_damage);
        break;

    case AF_VAMPIRIC:
        // Only may bite non-vampiric monsters (or player) capable of bleeding.
        if (!defender->can_bleed())
            break;

        // Disallow draining of summoned monsters since they can't bleed.
        // XXX: Is this too harsh?
        if (defender->is_summoned())
            break;

        if (x_chance_in_y(defender->res_negative_energy(), 3))
            break;

        if (defender->stat_hp() < defender->stat_maxhp())
        {
            attacker->heal(1 + random2(damage_done), coinflip());

            if (needs_message)
            {
                mprf("%s %s strength from %s injuries!",
                     atk_name(DESC_THE).c_str(),
                     attacker->conj_verb("draw").c_str(),
                     def_name(DESC_ITS).c_str());
            }
        }
        break;

    case AF_DRAIN_STR:
    case AF_DRAIN_INT:
    case AF_DRAIN_DEX:
        if ((one_chance_in(20) || (damage_done > 0 && one_chance_in(3)))
            && defender->res_negative_energy() < random2(4))
        {
            stat_type drained_stat = (flavour == AF_DRAIN_STR ? STAT_STR :
                                      flavour == AF_DRAIN_INT ? STAT_INT
                                                              : STAT_DEX);
            defender->drain_stat(drained_stat, 1, attacker);
        }
        break;

    case AF_HUNGER:
        if (defender->holiness() == MH_UNDEAD)
            break;

        if (one_chance_in(20) || (damage_done > 0))
            defender->make_hungry(you.hunger / 4, false);
        break;

    case AF_BLINK:
        if (one_chance_in(3))
            attacker->blink();
        break;

    case AF_CONFUSE:
        if (attk_type == AT_SPORE)
        {
            if (defender->res_poison() > 0 || defender->is_unbreathing())
                break;

            if (--(attacker->as_monster()->hit_dice) <= 0)
                attacker->as_monster()->suicide();

            if (defender_visible)
            {
                mprf("%s %s engulfed in a cloud of spores!",
                     defender->name(DESC_THE).c_str(),
                     defender->conj_verb("are").c_str());
            }
        }

        if (one_chance_in(10)
            || (damage_done > 2 && one_chance_in(3)))
        {
            defender->confuse(attacker,
                              1 + random2(3+attacker->get_experience_level()));
        }
        break;

    case AF_DRAIN_XP:
        if (one_chance_in(30)
            || (damage_done > 5 && coinflip())
            || (attk_damage == 0 && !one_chance_in(3)))
        {
            drain_defender();
        }
        break;

    case AF_PARALYSE:
    {
        // Only wasps at the moment.
    	if (attacker->type == MONS_RED_WASP || one_chance_in(3))
			defender->poison(attacker, coinflip() ? 2 : 1);

		int paralyse_roll = (damage_done > 4 ? 3 : 20);
		if (attacker->type == MONS_YELLOW_WASP)
			paralyse_roll += 3;

		if (defender->res_poison() <= 0)
		{
			if (one_chance_in(paralyse_roll))
				defender->paralyse(attacker, roll_dice(1, 3));
			else
				defender->slow_down(attacker, roll_dice(1, 3));
		}
        break;
    }

    case AF_ACID:
        if (attacker->type == MONS_SPINY_WORM)
            defender->poison(attacker, 2 + random2(4));
        splash_defender_with_acid(3);
        break;

    case AF_DISTORT:
        distortion_affects_defender();
        break;

    case AF_RAGE:
        if (!one_chance_in(3) || !defender->can_go_berserk())
            break;

        if (needs_message)
        {
            mprf("%s %s %s!",
                 atk_name(DESC_THE).c_str(),
                 attacker->conj_verb("infuriate").c_str(),
                 defender_name().c_str());
        }

        defender->go_berserk(false);
        break;

    case AF_NAPALM:
        mons_do_napalm();
        break;

    case AF_CHAOS:
        chaos_affects_defender();
        break;

    case AF_STEAL:
        // Ignore monsters, for now.
        if (defender->atype() != ACT_PLAYER)
            break;

        attacker->as_monster()->steal_item_from_player();
        break;

    case AF_STEAL_FOOD:
    {
        // Monsters don't carry food.
        if (defender->atype() != ACT_PLAYER)
            break;

        const bool stolen = expose_player_to_element(BEAM_DEVOUR_FOOD, 10);
        const bool ground = expose_items_to_element(BEAM_DEVOUR_FOOD, you.pos(),
                                                    10);
        if (needs_message)
        {
            if (stolen)
            {
                mprf("%s devours some of your food!",
                     atk_name(DESC_THE).c_str());
            }
            else if (ground)
            {
                mprf("%s devours some of the food beneath you!",
                     atk_name(DESC_THE).c_str());
            }
        }
        break;
    }

    case AF_CRUSH:
    /*
        mprf("%s %s being crushed%s",
             def_name(DESC_THE).c_str(),
             defender->conj_verb("are").c_str(),
             special_attack_punctuation().c_str());
    */
        break;

    case AF_HOLY:

        if (defender->is_evil() || defender->is_unholy())
            special_damage = attk_damage * 0.75;

        if (needs_message && special_damage)
        {
            mprf("%s %s %s%s",
                 atk_name(DESC_THE).c_str(),
                 attacker->conj_verb("sear").c_str(),
                 defender_name().c_str(),
                 special_attack_punctuation().c_str());

        }

        break;

    case AF_ANTIMAGIC:
        antimagic_affects_defender();
        break;

    case AF_PAIN:
        pain_affects_defender();
        break;
    }
}

void melee_attack::do_passive_freeze()
{
    if (you.mutation[MUT_PASSIVE_FREEZE]
        && attacker->alive()
        && grid_distance(you.pos(), attacker->as_monster()->pos()) == 1)
    {
        bolt beam;
        beam.flavour = BEAM_COLD;
        beam.thrower = KILL_YOU;

        monster* mon = attacker->as_monster();

        const int orig_hurted = random2(11);
        int hurted = mons_adjust_flavoured(mon, beam, orig_hurted);

        if (!hurted)
            return;

        simple_monster_message(mon, " is very cold.");

#ifndef USE_TILE
        flash_monster_colour(mon, LIGHTBLUE, 200);
#endif

        mon->hurt(&you, hurted);

        if (mon->alive())
        {
            mon->expose_to_element(BEAM_COLD, orig_hurted);
            print_wounds(mon);

            const int cold_res = mon->res_cold();

            if (cold_res <= 0)
            {
                const int stun = (1 - cold_res) * random2(7);
                mon->speed_increment -= stun;
            }
        }
    }
}

void melee_attack::mons_do_eyeball_confusion()
{
    if (you.mutation[MUT_EYEBALLS]
        && attacker->alive()
        && grid_distance(you.pos(), attacker->as_monster()->pos()) == 1
        && x_chance_in_y(player_mutation_level(MUT_EYEBALLS), 20))
    {
        const int ench_pow = player_mutation_level(MUT_EYEBALLS) * 30;
        monster* mon = attacker->as_monster();

        if (mon->check_res_magic(ench_pow) <= 0
            && mons_class_is_confusable(mon->type))
        {
            mprf("The eyeballs on your body gaze at %s.",
                 mon->name(DESC_THE).c_str());

            mon->add_ench(mon_enchant(ENCH_CONFUSION, 0, &you,
                                      30 + random2(100)));
        }
    }
}

void melee_attack::do_spines()
{
    const item_def *body = you.slot_item(EQ_BODY_ARMOUR, false);
    const int mut = player_mutation_level(MUT_SPINY);
    int evp = 0;

    if (body)
        evp = -property(*body, PARM_EVASION);

    if (you.mutation[MUT_SPINY]
        && attacker->alive()
        && one_chance_in(evp + 1))
    {
        if (test_hit(2 + 4 * mut, attacker->melee_evasion(defender)) < 0)
        {
            simple_monster_message(attacker->as_monster(),
                                   " dodges your spines.");
            return;
        }

        int dmg = roll_dice(mut, 6);
        int ac = random2(1 + attacker->armour_class());
        int hurt = dmg - ac - evp;

        dprf("Spiny: dmg = %d ac = %d hurt = %d", dmg, ac, hurt);

        if (hurt <= 0)
            return;

        if (!defender_invisible)
        {
            simple_monster_message(attacker->as_monster(),
                                   " is struck by your spines.");
        }

        attacker->hurt(&you, hurt);
    }
}

void melee_attack::mons_emit_foul_stench()
{
    monster* mon = attacker->as_monster();

    if (you.mutation[MUT_FOUL_STENCH]
        && attacker->alive()
        && grid_distance(you.pos(), mon->pos()) == 1)
    {
        const int mut = player_mutation_level(MUT_FOUL_STENCH);

        if (one_chance_in(3))
            mon->sicken(50 + random2(100));

        if (damage_done > 4 && x_chance_in_y(mut, 5)
            && !cell_is_solid(mon->pos())
            && env.cgrid(mon->pos()) == EMPTY_CLOUD)
        {
            mpr("You emit a cloud of foul miasma!");
            place_cloud(CLOUD_MIASMA, mon->pos(), 5 + random2(6), &you);
        }
    }
}

bool melee_attack::do_knockback(bool trample)
{
    do
    {
        monster* def_monster = defender->as_monster();
        if (def_monster && mons_is_stationary(def_monster))
            // don't even print a message
            return false;

        int size_diff =
            attacker->body_size(PSIZE_BODY) - defender->body_size(PSIZE_BODY);
        if (!x_chance_in_y(size_diff + 3, 6))
            break;

        coord_def old_pos = defender->pos();
        coord_def new_pos = defender->pos() + defender->pos() - attacker->pos();

        // need a valid tile
        if (grd(new_pos) < DNGN_SHALLOW_WATER && !defender->is_habitable(new_pos))
            break;

        // don't trample into a monster - or do we want to cause a chain
        // reaction here?
        if (actor_at(new_pos))
            break;

        if (needs_message)
        {
            mprf("%s %s backwards!",
                 def_name(DESC_THE).c_str(),
                 defender->conj_verb("stumble").c_str());
        }

        if (defender->as_player())
            move_player_to_grid(new_pos, false, true);
        else
            defender->move_to_pos(new_pos);

        if (trample && attacker->is_habitable(old_pos))
            attacker->move_to_pos(old_pos);

        // Interrupt stair travel and passwall.
        if (defender == &you)
            stop_delay(true);

        return true;
    } while (0);

    if (needs_message)
    {
        mprf("%s %s %s ground!",
             def_name(DESC_THE).c_str(),
             defender->conj_verb("hold").c_str(),
             defender->pronoun(PRONOUN_POSSESSIVE).c_str());
    }

    return false;
}

void melee_attack::chaos_affect_actor(actor *victim)
{
    melee_attack attk(victim, victim);
    attk.weapon = NULL;
    attk.skip_chaos_message = true;
    attk.chaos_affects_defender();
    attk.do_miscast();
    if (!attk.special_damage_message.empty()
        && you.can_see(victim))
    {
        mprf("%s", attk.special_damage_message.c_str());
    }
}

bool melee_attack::_tran_forbid_aux_attack(unarmed_attack_type atk)
{
    switch (atk)
    {
    case UNAT_KICK:
    case UNAT_HEADBUTT:
    case UNAT_PUNCH:
        return (you.form == TRAN_ICE_BEAST
                || you.form == TRAN_DRAGON
                || you.form == TRAN_SPIDER
                || you.form == TRAN_BAT);

    default:
        return (false);
    }
}

bool melee_attack::_extra_aux_attack(unarmed_attack_type atk, bool is_base)
{
    // No extra unarmed attacks for disabled mutations.
    // XXX: It might be better to make player_mutation_level
    //      aware of mutations that are disabled due to transformation.
    if (_tran_forbid_aux_attack(atk))
        return (false);

    if (you.strength() + you.dex() <= random2(50))
        return (false);

    switch (atk)
    {
    case UNAT_KICK:
        return (is_base
                 || player_mutation_level(MUT_HOOVES)
                 || you.has_usable_talons()
                 || player_mutation_level(MUT_TENTACLE_SPIKE));

    case UNAT_HEADBUTT:
        return ((is_base
                 || player_mutation_level(MUT_HORNS)
                 || player_mutation_level(MUT_BEAK))
                && !one_chance_in(3));

    case UNAT_TAILSLAP:
        return ((is_base
                 || you.has_usable_tail())
                && coinflip());

    case UNAT_PSEUDOPODS:
        return ((is_base
                 || you.has_usable_pseudopods())
                && !one_chance_in(3));

    case UNAT_TENTACLES:
        return ((is_base
                 || you.has_usable_tentacles())
                && !one_chance_in(3));

    case UNAT_BITE:
        return ((is_base
                 || you.has_usable_fangs()
                 || player_mutation_level(MUT_ACIDIC_BITE))
                && x_chance_in_y(2, 5));

    case UNAT_PUNCH:
        return (is_base);

    default:
        return (false);
    }
}

// TODO: Potentially move this, may or may not belong here (may not
// even blong as its own function, could be integrated with the general
// to-hit method
// Returns the to-hit for your extra unarmed attacks.
// DOES NOT do the final roll (i.e., random2(your_to_hit)).
int melee_attack::calc_your_to_hit_unarmed(int uattack, bool vampiric)
{
    int your_to_hit;

    your_to_hit = 1300
                + you.dex() * 50
                + you.strength() * 20
                + you.skill(SK_FIGHTING, 30);
    your_to_hit /= 100;

    if (wearing_amulet(AMU_INACCURACY))
        your_to_hit -= 5;

    if (player_mutation_level(MUT_EYEBALLS))
        your_to_hit += 2 * player_mutation_level(MUT_EYEBALLS) + 1;

    // Vampires know how to bite and aim better when thirsty.
    if (you.species == SP_VAMPIRE && uattack == UNAT_BITE)
    {
        your_to_hit += 1;

        if (vampiric)
        {
            if (you.hunger_state == HS_STARVING)
                your_to_hit += 2;
            else if (you.hunger_state < HS_SATIATED)
                your_to_hit += 1;
        }
    }
    else if (you.species != SP_VAMPIRE && you.hunger_state == HS_STARVING)
        your_to_hit -= 3;

    your_to_hit += slaying_bonus(PWPN_HIT);

    return your_to_hit;
}

// TODO: Potentially remove, consider integrating with other to-hit or stat
// calculating methods
// weighted average of strength and dex, between (str+dex)/2 and dex
int melee_attack::calc_stat_to_hit_base()
{
    // towards_str_avg is a variable, whose sign points towards strength,
    // and the magnitude is half the difference (thus, when added directly
    // to you.dex() it gives the average of the two.
    const signed int towards_str_avg = (you.strength() - you.dex()) / 2;

    // dex is modified by strength towards the average, by the
    // weighted amount weapon_str_weight() / 10.
    return (you.dex() + towards_str_avg * player_weapon_str_weight() / 10);
}

int melee_attack::test_hit(int to_land, int ev)
{
    int   roll = -1;
    int margin = AUTOMATIC_HIT;

    if (to_land >= AUTOMATIC_HIT)
        return (true);
    else if (x_chance_in_y(MIN_HIT_MISS_PERCENTAGE, 100))
        margin = (random2(2) ? 1 : -1) * AUTOMATIC_HIT;
    else
    {
        roll = random2(to_land + 1);
        margin = (roll - random2avg(ev, 2));
    }

#ifdef DEBUG_DIAGNOSTICS
    float miss;

    if (to_hit < ev)
        miss = 100.0 - MIN_HIT_MISS_PERCENTAGE / 2.0;
    else
    {
        miss = MIN_HIT_MISS_PERCENTAGE / 2.0 +
            ((100.0 - MIN_HIT_MISS_PERCENTAGE) * ev) / to_hit;
    }

    dprf("to hit: %d; ev: %d; miss: %0.2f%%; roll: %d; result: %s%s (%d)",
         to_hit, ev, miss, roll, (margin >= 0) ? "hit" : "miss",
            (roll == -1) ? "!!!" : "", margin);
#endif

    return (margin);
}

/* Returns base weapon damage for attacker
 *
 * Because of the complex nature between player and monster base damage,
 * a simple atype() check forks the logic for now. At the moment this is
 * only is called within the context of ACT_PLAYER. Eventually, we should
 * aim for a unified code for calculating all / most combat related figures
 * and let the actor classes handle monster or player-specific scaling.
 *
 */
int melee_attack::calc_base_weapon_damage()
{
    int damage = 0;

    if(attacker->atype() == ACT_PLAYER)
    {
        if (weapon->base_type == OBJ_WEAPONS && !is_range_weapon(*weapon)
            || weapon->base_type == OBJ_STAVES)
        {
            damage = property(*weapon, PWPN_DAMAGE);
        }

        // Even large staves can be wielded with a worn shield, but they
        // are much less effective
        if (shield && weapon_skill(*weapon) == SK_STAVES
            && cmp_weapon_size(*weapon, SIZE_LARGE) >= 0)
        {
            damage /= 2;
        }
    }

    return (damage);
}

/* Returns attacker base unarmed damage
 *
 * Scales for current mutations and unarmed effects
 * TODO: Complete symmetry for base_unarmed damage
 * between monsters and players.
 */
int melee_attack::calc_base_unarmed_damage()
{
    int damage = 0;

    if(attacker->atype() == ACT_PLAYER)
    {
        damage = you.duration[DUR_CONFUSING_TOUCH] ? 0 : 3;

        switch (you.form)
        {
        case TRAN_SPIDER:
            damage = 5;
            break;
        case TRAN_BAT:
            damage = (you.species == SP_VAMPIRE ? 2 : 1);
            break;
        case TRAN_ICE_BEAST:
            damage = 12;
            break;
        case TRAN_BLADE_HANDS:
            damage = 12 + div_rand_round(you.strength() + you.dex(), 4);
            break;
        case TRAN_STATUE: // multiplied by 1.5 later
            damage = 6 + you.strength();
            break;
        case TRAN_DRAGON: // +6 from claws
            damage = 12 + div_rand_round(you.strength() * 2, 3);
            break;
        case TRAN_LICH:
            damage = 5;
            break;
        case TRAN_PIG:
            break;
        case TRAN_NONE:
        case TRAN_APPENDAGE:
            break;
        }

        if (you.has_usable_claws())
        {
            // Claw damage only applies for bare hands.
            damage += you.has_usable_claws(false) * 2;
            apply_bleeding = true;
        }

        if (player_in_bat_form())
        {
            // Bats really don't do a lot of damage.
            damage += you.skill_rdiv(SK_UNARMED_COMBAT, 1, 5);
        }
        else
            damage += you.skill_rdiv(SK_UNARMED_COMBAT);
    }

    return damage;
}

int melee_attack::calc_damage()
{
    if(attacker->atype() == ACT_MONSTER)
    {
        monster *as_mon = attacker->as_monster();

        int damage = 0;
        int damage_max = 0;
        if (weapon
            && (weapon->base_type == OBJ_WEAPONS
                && !is_range_weapon(*weapon)
                || item_is_rod(*weapon)))
        {
            damage_max = property(*weapon, PWPN_DAMAGE);
            damage += random2(damage_max);

            if (get_equip_race(*weapon) == ISFLAG_ORCISH
                && mons_genus(attacker->mons_species()) == MONS_ORC
                && coinflip())
            {
                damage++;
            }

            int wpn_damage_plus = weapon->plus2;
            if(item_is_rod(*weapon))
                wpn_damage_plus = (short)weapon->props["rod_enchantment"];

            if (wpn_damage_plus >= 0)
                damage += random2(wpn_damage_plus);
            else
                damage -= random2(1 - wpn_damage_plus);

            damage -= 1 + random2(3);
        }

        damage_max += attk_damage;
        damage     += 1 + random2(attk_damage);
        int frenzy_degree = -11;

        // Berserk/mighted/frenzied monsters get bonus damage.
        if (as_mon->has_ench(ENCH_MIGHT)
            || as_mon->has_ench(ENCH_BERSERK)
            || as_mon->has_ench(ENCH_INSANE))
        {
            damage = damage * 3 / 2;
        }
        else if (as_mon->has_ench(ENCH_BATTLE_FRENZY))
        {
            frenzy_degree = as_mon->get_ench(ENCH_BATTLE_FRENZY).degree;
        }
        else if (as_mon->has_ench(ENCH_ROUSED))
        {
            frenzy_degree = as_mon->get_ench(ENCH_ROUSED).degree;
        }

        if (frenzy_degree == -1)
        {

            const int orig_damage = damage;

            damage = damage * (115 + frenzy_degree * 15) / 100;

            dprf("%s frenzy damage: %d->%d",
                 attacker->name(DESC_PLAIN).c_str(), orig_damage, damage);
        }

        if (weapon && get_weapon_brand(*weapon) == SPWPN_SPEED)
            damage = div_rand_round(damage * 9, 10);

        // If the defender is asleep, the attacker gets a stab.
        if (defender && defender->asleep())
        {
            damage = damage * 5 / 2;

            dprf("Stab damage vs %s: %d",
                 defender->name(DESC_PLAIN).c_str(),
                 damage);
        }

        return (apply_defender_ac(damage, damage_max));
    }
    else
    {
        int potential_damage;

        potential_damage =
            !weapon ? calc_base_unarmed_damage()
                    : calc_base_weapon_damage();

        // [0.6 AC/EV overhaul] Shields don't go well with hand-and-half weapons.
        if (weapon && hands == HANDS_HALF)
        {
            potential_damage =
                std::max(1,
                         potential_damage - roll_dice(1, attacker_shield_penalty));
        }

        potential_damage = player_stat_modify_damage(potential_damage);

        damage_done =
            potential_damage > 0 ? one_chance_in(3) + random2(potential_damage) : 0;

        damage_done = player_apply_weapon_skill(damage_done);
        damage_done = player_apply_fighting_skill(damage_done, false);
        damage_done = player_apply_misc_modifiers(damage_done);
        damage_done = player_apply_weapon_bonuses(damage_done);

        player_weapon_auto_id();

        damage_done = player_stab(damage_done);
        damage_done = apply_defender_ac(damage_done);

        set_attack_verb();
        damage_done = std::max(0, damage_done);

        return damage_done;
    }

    return (0);
}

int melee_attack::apply_defender_ac(int damage, int damage_max)
{
    int ac = defender->armour_class();
    int stab_bypass = stab_bonus
                      ? random2(you.skill_rdiv(SK_STABBING) / stab_bonus)
                      : 0;
    if (ac > 0)
    {
        if(attacker->atype() == ACT_PLAYER)
        {
            damage -= random2(1 + defender->armour_class() - stab_bypass);
        }
        else
        {
            int damage_reduction = random2(ac + 1);
            int guaranteed_damage_reduction = 0;

            if (defender->atype() == ACT_PLAYER)
            {
                const int gdr_perc = defender->as_player()->gdr_perc();
                guaranteed_damage_reduction =
                    std::min(damage_max * gdr_perc / 100, ac / 2);
                damage_reduction =
                    std::max(guaranteed_damage_reduction, damage_reduction);
            }

            damage -= damage_reduction;

            dprf("AC: at: %s, df: %s, dam: %d (max %d), DR: %d (GDR %d), "
                 "rdam: %d",
                 attacker->name(DESC_PLAIN, true).c_str(),
                 defender->name(DESC_PLAIN, true).c_str(),
                 damage, damage_max, damage_reduction, guaranteed_damage_reduction,
                 damage - damage_reduction);
        }
    }

    return std::max(0, damage);
}

/* TODO: This code is only used from melee_attack methods, but perhaps it
 * should be ambigufied and moved to the actor class
 * Should life protection protect from this?
 *
 * Should eventually remove in favor of player/monster symmetry
 *
 * Called when stabbing, for bite attacks, and vampires wielding vampiric weapons
 * Returns true if blood was drawn.
 */
bool melee_attack::_player_vampire_draws_blood(const monster* mon, const int damage,
                                               bool needs_bite_msg, int reduction)
{
    ASSERT(you.species == SP_VAMPIRE);

    if (!_vamp_wants_blood_from_monster(mon))
        return (false);

    const corpse_effect_type chunk_type = mons_corpse_effect(mon->type);

    // Now print message, need biting unless already done (never for bat form!)
    if (needs_bite_msg && !player_in_bat_form())
    {
        mprf("You bite %s, and draw %s blood!",
             mon->name(DESC_THE, true).c_str(),
             mon->pronoun(PRONOUN_POSSESSIVE).c_str());
    }
    else
    {
        mprf("You draw %s blood!",
             apostrophise(mon->name(DESC_THE, true)).c_str());
    }

    // Regain hp.
    if (you.hp < you.hp_max)
    {
        int heal = 1 + random2(damage);
        if (chunk_type == CE_CLEAN)
            heal += 1 + random2(damage);
        if (heal > you.experience_level)
            heal = you.experience_level;

        // Decrease healing when done in bat form.
        if (player_in_bat_form())
            heal /= 2;

        if (heal > 0 && !you.duration[DUR_DEATHS_DOOR])
        {
            inc_hp(heal);
            mprf("You feel %sbetter.", (you.hp == you.hp_max) ? "much " : "");
        }
    }

    // Gain nutrition.
    if (you.hunger_state != HS_ENGORGED)
    {
        int food_value = 0;
        if (chunk_type == CE_CLEAN)
            food_value = 30 + random2avg(59, 2);
        else if (chunk_type == CE_CONTAMINATED
                 || chunk_is_poisonous(chunk_type))
        {
            food_value = 15 + random2avg(29, 2);
        }

        // Bats get rather less nutrition out of it.
        if (player_in_bat_form())
            food_value /= 2;

        food_value /= reduction;

        lessen_hunger(food_value, false);
    }

    did_god_conduct(DID_DRINK_BLOOD, 5 + random2(4));

    return (true);
}

// weighted average of strength and dex, between str and (str+dex)/2
int melee_attack::calc_stat_to_dam_base()
{
    const signed int towards_dex_avg = (you.dex() - you.strength()) / 2;
    return (you.strength() + towards_dex_avg * player_weapon_dex_weight() / 10);
}

void melee_attack::stab_message()
{
    switch (stab_bonus)
    {
    case 6:     // big melee, monster surrounded/not paying attention
        if (coinflip())
        {
            mprf("You %s %s from a blind spot!",
                  (you.species == SP_FELID) ? "pounce on" : "strike",
                  defender->name(DESC_THE).c_str());
        }
        else
        {
            mprf("You catch %s momentarily off-guard.",
                  defender->name(DESC_THE).c_str());
        }
        break;
    case 4:     // confused/fleeing
        if (!one_chance_in(3))
        {
            mprf("You catch %s completely off-guard!",
                  defender->name(DESC_THE).c_str());
        }
        else
        {
            mprf("You %s %s from behind!",
                  (you.species == SP_FELID) ? "pounce on" : "strike",
                  defender->name(DESC_THE).c_str());
        }
        break;
    case 2:
    case 1:
        if (you.species == SP_FELID && coinflip())
        {
            mprf("You pounce on the unaware %s!",
                 defender->name(DESC_PLAIN).c_str());
            break;
        }
        mprf("%s fails to defend %s.",
              defender->name(DESC_THE).c_str(),
              defender->pronoun(PRONOUN_REFLEXIVE).c_str());
        break;
    }
}

bool melee_attack::_vamp_wants_blood_from_monster(const monster* mon)
{
    if (you.species != SP_VAMPIRE)
        return (false);

    if (you.hunger_state == HS_ENGORGED)
        return (false);

    if (mon->is_summoned())
        return (false);

    if (!mons_has_blood(mon->type))
        return (false);

    const corpse_effect_type chunk_type = mons_corpse_effect(mon->type);

    // Don't drink poisonous or mutagenic blood.
    return (chunk_type == CE_CLEAN || chunk_type == CE_CONTAMINATED
            || (chunk_is_poisonous(chunk_type) && player_res_poison()));
}

int melee_attack::inflict_damage(int dam, beam_type flavour, bool clean)
{
    if (flavour == NUM_BEAMS)
        flavour = special_damage_flavour;
    // Auxes temporarily clear damage_brand so we don't need to check
    if (damage_brand == SPWPN_REAPING ||
        damage_brand == SPWPN_CHAOS && one_chance_in(100))
    {
        defender->props["reaping_damage"].get_int() += dam;
        // With two reapers of different friendliness, the most recent one
        // gets the zombie. Too rare a case to care any more.
        defender->props["reaper"].get_int() = attacker->mid;
    }
    return defender->hurt(attacker, dam, flavour, clean);
}
