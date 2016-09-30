/**
 * @file
 * @brief melee_attack class and associated melee_attack methods
 */

#include "AppHdr.h"

#include "melee_attack.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "areas.h"
#include "art-enum.h"
#include "attitude-change.h"
#include "bloodspatter.h"
#include "butcher.h"
#include "chardump.h"
#include "cloud.h"
#include "coordit.h"
#include "delay.h"
#include "english.h"
#include "env.h"
#include "exercise.h"
#include "fineff.h"
#include "food.h"
#include "godconduct.h"
#include "goditem.h"
#include "godpassive.h" // passive_t::convert_orcs
#include "hints.h"
#include "itemprop.h"
#include "mapdef.h"
#include "message.h"
#include "misc.h"
#include "mon-behv.h"
#include "mon-poly.h"
#include "mon-tentacle.h"
#include "religion.h"
#include "shout.h"
#include "spl-summoning.h"
#include "state.h"
#include "stringutil.h"
#include "target.h"
#include "terrain.h"
#include "transform.h"
#include "traps.h"
#include "unwind.h"
#include "view.h"
#include "xom.h"

#ifdef NOTE_DEBUG_CHAOS_BRAND
    #define NOTE_DEBUG_CHAOS_EFFECTS
#endif

#ifdef NOTE_DEBUG_CHAOS_EFFECTS
    #include "notes.h"
#endif

/*
 **************************************************
 *             BEGIN PUBLIC FUNCTIONS             *
 **************************************************
*/
melee_attack::melee_attack(actor *attk, actor *defn,
                           int attack_num, int effective_attack_num,
                           bool is_cleaving, coord_def attack_pos)
    :  // Call attack's constructor
    ::attack(attk, defn),

    attack_number(attack_num), effective_attack_number(effective_attack_num),
    cleaving(is_cleaving), is_riposte(false)
{
    attack_occurred = false;
    damage_brand = attacker->damage_brand(attack_number);
    init_attack(SK_UNARMED_COMBAT, attack_number);
    if (weapon && !using_weapon())
        wpn_skill = SK_FIGHTING;

    attack_position = attacker->pos();
}

bool melee_attack::can_reach()
{
    return attk_type == AT_HIT && weapon && weapon_reach(*weapon) > REACH_NONE
           || attk_flavour == AF_REACH
           || attk_flavour == AF_REACH_STING;
}

bool melee_attack::handle_phase_attempted()
{
    // Skip invalid and dummy attacks.
    if (defender && (!adjacent(attack_position, defender->pos())
                     && !can_reach())
        || attk_type == AT_CONSTRICT
           && (!attacker->can_constrict(defender)
               || attacker->is_monster() && attacker->mid == MID_PLAYER))
    {
        --effective_attack_number;

        return false;
    }

    if (attacker->is_player() && defender && defender->is_monster())
    {
        if (weapon && is_unrandom_artefact(*weapon, UNRAND_DEVASTATOR))
        {
            const char* verb = "attack";
            string junk1, junk2;
            bool junk3 = false;
            if (defender)
            {
                verb = (bad_attack(defender->as_monster(),
                                   junk1, junk2, junk3)
                        ? "attack" : "attack near");
            }

            targetter_smite hitfunc(attacker, 1, 1, 1, false);
            hitfunc.set_aim(defender->pos());

            if (stop_attack_prompt(hitfunc, verb))
            {
                cancel_attack = true;
                return false;
            }
        }
        else if (!cleave_targets.empty())
        {
            targetter_cleave hitfunc(attacker, defender->pos());
            if (stop_attack_prompt(hitfunc, "attack"))
            {
                cancel_attack = true;
                return false;
            }
        }
        else if (stop_attack_prompt(defender->as_monster(), false,
                                    attack_position))
        {
            cancel_attack = true;
            return false;
        }
    }

    if (attacker->is_player())
    {
        // Set delay now that we know the attack won't be cancelled.
        if (!is_riposte)
            you.time_taken = you.attack_delay().roll();
        if (weapon)
        {
            if (weapon->base_type == OBJ_WEAPONS)
                if (is_unrandom_artefact(*weapon)
                    && get_unrand_entry(weapon->unrand_idx)->type_name)
                {
                    count_action(CACT_MELEE, weapon->unrand_idx);
                }
                else
                    count_action(CACT_MELEE, weapon->sub_type);
            else if (weapon->base_type == OBJ_STAVES)
                count_action(CACT_MELEE, WPN_STAFF);
        }
        else
            count_action(CACT_MELEE, -1, -1); // unarmed subtype/auxtype
    }
    else
    {
        // Only the first attack costs any energy.
        if (!effective_attack_number)
        {
            int energy = attacker->as_monster()->action_energy(EUT_ATTACK);
            int delay = attacker->attack_delay().roll();
            dprf(DIAG_COMBAT, "Attack delay %d, multiplier %1.1f", delay, energy * 0.1);
            ASSERT(energy > 0);
            ASSERT(delay > 0);

            attacker->as_monster()->speed_increment
                -= div_rand_round(energy * delay, 10);
        }

        // Statues and other special monsters which have AT_NONE need to lose
        // energy, but otherwise should exit the melee attack now.
        if (attk_type == AT_NONE)
            return false;
    }

    if (attacker != defender)
    {
        // Allow setting of your allies' target, etc.
        attacker->attacking(defender);

        check_autoberserk();
    }

    // The attacker loses nutrition.
    attacker->make_hungry(3, true);

    // Xom thinks fumbles are funny...
    if (attacker->fumbles_attack())
    {
        // ... and thinks fumbling when trying to hit yourself is just
        // hilarious.
        xom_is_stimulated(attacker == defender ? 200 : 10);
        return false;
    }
    // Non-fumbled self-attacks due to confusion are still pretty funny, though.
    else if (attacker == defender && attacker->confused())
    {
        // And is still hilarious if it's the player.
        xom_is_stimulated(attacker->is_player() ? 200 : 100);
    }

    // Any attack against a monster we're afraid of has a chance to fail
    if (attacker->is_player() && you.afraid_of(defender->as_monster())
        && one_chance_in(3))
    {
        mprf("You attempt to attack %s, but flinch away in fear!",
             defender->name(DESC_THE).c_str());
        return false;
    }

    if (attk_flavour == AF_SHADOWSTAB
        && defender && !defender->can_see(*attacker))
    {
        if (you.see_cell(attack_position))
        {
            mprf("%s strikes at %s from the darkness!",
                 attacker->name(DESC_THE, true).c_str(),
                 defender->name(DESC_THE).c_str());
        }
        to_hit = AUTOMATIC_HIT;
        needs_message = false;
    }
    else if (attacker->is_monster()
             && attacker->type == MONS_DROWNED_SOUL)
    {
        to_hit = AUTOMATIC_HIT;
    }

    attack_occurred = true;

    // Check for player practicing dodging
    if (defender->is_player())
        practise_being_attacked();

    return true;
}

bool melee_attack::handle_phase_dodged()
{
    did_hit = false;

    if (needs_message)
    {
        // TODO: Unify these, placed player_warn_miss here so I can remove
        // player_attack
        if (attacker->is_player())
            player_warn_miss();
        else
        {
            mprf("%s%s misses %s%s",
                 atk_name(DESC_THE).c_str(),
                 evasion_margin_adverb().c_str(),
                 defender_name(true).c_str(),
                 attack_strength_punctuation(damage_done).c_str());
        }
    }
    if (defender->is_player())
        count_action(CACT_DODGE, DODGE_EVASION);

    if (attacker != defender && adjacent(defender->pos(), attack_position)
        && attacker->alive() && defender->can_see(*attacker)
        && !defender->cannot_act() && !defender->confused()
        && (!defender->is_player() || (!you.duration[DUR_LIFESAVING]
                                       && !attacker->as_monster()->neutral()))
        && !mons_aligned(attacker, defender) // confused friendlies attacking
        // Retaliation only works on the first attack in a round.
        // FIXME: player's attack is -1, even for auxes
        && effective_attack_number <= 0)
    {
        if (defender->is_player() ?
                you.species == SP_MINOTAUR :
                mons_species(mons_base_type(*defender->as_monster()))
                    == MONS_MINOTAUR)
        {
            do_minotaur_retaliation();
        }
        else if ((you.props["wudzu_hat_picked_v"].get_int() == 1
		  && have_passive(passive_t::thorn_vestment))
		  || (you.props["wudzu_hat_picked_r"].get_int() == 1
		  && have_passive(passive_t::thorn_regalia)))
        {
			if (coinflip())
				do_wudzu_hat_retaliation();
        }

        // Retaliations can kill!
        if (!attacker->alive())
            return false;

        if (defender->is_player())
        {
            const bool using_lbl = defender->weapon()
                && item_attack_skill(*defender->weapon()) == SK_LONG_BLADES;
            const bool using_fencers
                = player_equip_unrand(UNRAND_FENCERS);
            const int chance = using_lbl + using_fencers;

            if (x_chance_in_y(chance, 3) && !is_riposte) // no ping-pong!
                riposte();

            // Retaliations can kill!
            if (!attacker->alive())
                return false;
        }
    }

    return true;
}

static bool _flavour_triggers_damageless(attack_flavour flavour)
{
    return flavour == AF_CRUSH
           || flavour == AF_ENGULF
           || flavour == AF_PURE_FIRE
           || flavour == AF_SHADOWSTAB
           || flavour == AF_DROWN
           || flavour == AF_CORRODE
           || flavour == AF_HUNGER;
}

void melee_attack::apply_black_mark_effects()
{
    // Less reliable effects for players.
    if (attacker->is_player()
        && you.mutation[MUT_BLACK_MARK]
        && one_chance_in(5)
        || attacker->is_monster()
           && attacker->as_monster()->has_ench(ENCH_BLACK_MARK))
    {
        if (!defender->alive())
            return;

        switch (random2(3))
        {
            case 0:
                antimagic_affects_defender(damage_done * 8);
                break;
            case 1:
                defender->weaken(attacker, 6);
                break;
            case 2:
                defender->drain_exp(attacker, false, 10);
                break;
        }
    }
}

/* An attack has been determined to have hit something
 *
 * Handles to-hit effects for both attackers and defenders,
 * Determines damage and passes off execution to handle_phase_damaged
 * Also applies weapon brands
 *
 * Returns true if combat should continue, false if it should end here.
 */
bool melee_attack::handle_phase_hit()
{
    did_hit = true;
    perceived_attack = true;
    bool hit_woke_orc = false;

    if (attacker->is_player())
    {
        if (crawl_state.game_is_hints())
            Hints.hints_melee_counter++;

        // TODO: Remove this (placed here so I can get rid of player_attack)
        if (have_passive(passive_t::convert_orcs)
            && mons_genus(defender->mons_species()) == MONS_ORC
            && !defender->is_summoned()
            && !defender->as_monster()->is_shapeshifter()
            && you.see_cell(defender->pos()) && defender->asleep())
        {
            hit_woke_orc = true;
        }
    }

    damage_done = 0;
    // Slimify does no damage and serves as an on-hit effect, handle it
    if (attacker->is_player() && you.duration[DUR_SLIMIFY]
        && mon_can_be_slimified(defender->as_monster())
        && !cleaving)
    {
        // Bail out after sliming so we don't get aux unarmed and
        // attack a fellow slime.
        slimify_monster(defender->as_monster());
        you.duration[DUR_SLIMIFY] = 0;

        return false;
    }

    if (attacker->is_player() && you.duration[DUR_INFUSION])
    {
        if (enough_mp(1, true, false))
        {
            // infusion_power is set when the infusion spell is cast
            const int pow = you.props["infusion_power"].get_int();
            const int dmg = 2 + div_rand_round(pow, 12);
            const int hurt = defender->apply_ac(dmg);

            dprf(DIAG_COMBAT, "Infusion: dmg = %d hurt = %d", dmg, hurt);

            if (hurt > 0)
            {
                damage_done = hurt;
                dec_mp(1);
            }
        }
    }

    // This does more than just calculate the damage, it also sets up
    // messages, etc. It also wakes nearby creatures on a failed stab,
    // meaning it could have made the attacked creature vanish. That
    // will be checked early in player_monattack_hit_effects
    damage_done += calc_damage();

    bool stop_hit = false;
    // Check if some hit-effect killed the monster.
    if (attacker->is_player())
        stop_hit = !player_monattk_hit_effects();

    // check_unrand_effects is safe to call with a dead defender, so always
    // call it, even if the hit effects said to stop.
    if (stop_hit)
    {
        check_unrand_effects();
        return false;
    }

    if (damage_done > 0 || _flavour_triggers_damageless(attk_flavour))
    {
        if (!handle_phase_damaged())
            return false;

        // TODO: Remove this, (placed here to remove player_attack)
        if (attacker->is_player() && hit_woke_orc)
        {
            // Call function of orcs first noticing you, but with
            // beaten-up conversion messages (if applicable).
            beogh_follower_convert(defender->as_monster(), true);
        }
    }
    else if (needs_message)
    {
        attack_verb = attacker->is_player()
                      ? attack_verb
                      : attacker->conj_verb(mons_attack_verb());

        // TODO: Clean this up if possible, checking atype for do / does is ugly
        mprf("%s %s %s but %s no damage.",
             attacker->name(DESC_THE).c_str(),
             attack_verb.c_str(),
             defender_name(true).c_str(),
             attacker->is_player() ? "do" : "does");
    }

    // Check for weapon brand & inflict that damage too
    apply_damage_brand();

    if (check_unrand_effects())
        return false;

    if (damage_done > 0)
        apply_black_mark_effects();

    if (attacker->is_player())
    {
		if ((you.props["wudzu_gloves_picked_v"].get_int() == 1
	      && have_passive(passive_t::thorn_vestment))
	      || (you.props["wudzu_gloves_picked_r"].get_int() == 1
	      && have_passive(passive_t::thorn_regalia)))
	    {
		if (coinflip())
			poison_monster(defender->as_monster(), &you);
	    }
        // Always upset monster regardless of damage.
        // However, successful stabs inhibit shouting.
        behaviour_event(defender->as_monster(), ME_WHACK, attacker,
                        coord_def(), !stab_attempt);

        // [ds] Monster may disappear after behaviour event.
        if (!defender->alive())
            return true;
    }
    else if (defender->is_player())
    {
        // These effects (mutations right now) are only triggered when
        // the player is hit, each of them will verify their own required
        // parameters.
        do_passive_freeze();
#if TAG_MAJOR_VERSION == 34
        do_passive_heat();
#endif
        emit_foul_stench();
    }

    return true;
}

bool melee_attack::handle_phase_damaged()
{
    bool shroud_broken = false;

    // TODO: Move this somewhere else, this is a terrible place for a
    // block-like (prevents all damage) effect.
    if (attacker != defender
        && (defender->is_player() && you.duration[DUR_SHROUD_OF_GOLUBRIA]
            || defender->is_monster()
               && defender->as_monster()->has_ench(ENCH_SHROUD))
        && !one_chance_in(3))
    {
        // Chance of the shroud falling apart increases based on the
        // strain of it, i.e. the damage it is redirecting.
        if (x_chance_in_y(damage_done, 10+damage_done))
        {
            // Delay the message for the shroud breaking until after
            // the attack message.
            shroud_broken = true;
            if (defender->is_player())
                you.duration[DUR_SHROUD_OF_GOLUBRIA] = 0;
            else
                defender->as_monster()->del_ench(ENCH_SHROUD);
        }
        else
        {
            if (needs_message)
            {
                mprf("%s shroud bends %s attack away%s",
                     def_name(DESC_ITS).c_str(),
                     atk_name(DESC_ITS).c_str(),
                     attack_strength_punctuation(damage_done).c_str());
            }
            did_hit = false;
            damage_done = 0;

            return false;
        }
    }

    if (!attack::handle_phase_damaged())
        return false;

    if (shroud_broken && needs_message)
    {
        mprf(defender->is_player() ? MSGCH_WARN : MSGCH_PLAIN,
             "%s shroud falls apart!",
             def_name(DESC_ITS).c_str());
    }

    return true;
}

bool melee_attack::handle_phase_aux()
{
    if (attacker->is_player() && !cleaving)
    {
        // returns whether an aux attack successfully took place
        // additional attacks from cleave don't get aux
        if (!defender->as_monster()->friendly()
            && adjacent(defender->pos(), attack_position))
        {
            player_aux_unarmed();
        }

        // Don't print wounds after the first attack with Gyre/Gimble.
        // DUR_CLEAVE and Gyre/Gimble interact poorly together at the moment,
        // so don't try to skip print_wounds in that case.
        if (!(weapon && is_unrandom_artefact(*weapon, UNRAND_GYRE)
              && !you.duration[DUR_CLEAVE]))
        {
            print_wounds(*defender->as_monster());
        }
    }

    return true;
}

/**
 * Devour a monster whole!
 *
 * @param defender  The monster in question.
 */
static void _hydra_devour(monster &victim)
{
    // what's the highest hunger level this lets the player get to?
    const hunger_state_t max_hunger =
        static_cast<hunger_state_t>(HS_SATIATED + player_likes_chunks());

    // will eating this actually fill the player up?
    const bool filling = !have_passive(passive_t::goldify_corpses)
                          && player_mutation_level(MUT_HERBIVOROUS, false) < 3
                          && you.hunger_state <= max_hunger
                          && you.hunger_state < HS_ENGORGED;

    mprf("You %sdevour %s!",
         filling ? "hungrily " : "",
         victim.name(DESC_THE).c_str());

    // give a clearer message for eating invisible things
    if (!you.can_see(victim))
    {
        mprf("It tastes like %s.",
             mons_type_name(mons_genus(victim.type), DESC_PLAIN).c_str());
        // this could be the actual creature name, but it feels more
        // 'flavourful' this way??
        // feel free to just use the actual creature name if this has buggy
        // edge cases or such
    }
    if (victim.has_ench(ENCH_STICKY_FLAME))
        mprf("Spicy!");

    // nutrition (maybe)
    if (filling)
    {
        const int equiv_chunks =
            1 + random2(max_corpse_chunks(victim.type));
        lessen_hunger(CHUNK_BASE_NUTRITION * equiv_chunks, false, max_hunger);
    }

    // healing
    if (!you.duration[DUR_DEATHS_DOOR])
    {
        const int healing = 1 + victim.get_experience_level() * 3 / 4
                              + random2(victim.get_experience_level() * 3 / 4);
        you.heal(healing);
        calc_hp();
        canned_msg(MSG_GAIN_HEALTH);
        dprf("healed for %d (%d hd)", healing, victim.get_experience_level());
    }

    // and devour the corpse.
    victim.props[NEVER_CORPSE_KEY] = true;
}

/**
 * Possibly devour the defender whole.
 *
 * @param defender  The defender in question.
 */
static void _hydra_consider_devouring(monster &defender)
{
    ASSERT(!crawl_state.game_is_arena());

    dprf("considering devouring");

    // no unhealthy food
    if (determine_chunk_effect(mons_corpse_effect(defender.type)) != CE_CLEAN)
        return;

    dprf("chunk ok");

    // shapeshifters are mutagenic
    if (defender.is_shapeshifter())
    {
        // handle this carefully, so the player knows what's going on
        mprf("You spit out %s as %s twists & changes in your mouth!",
             defender.name(DESC_THE).c_str(),
             defender.pronoun(PRONOUN_SUBJECTIVE).c_str());
        return;
    }

    dprf("shifter ok");

    // or food that would incur divine penance...
    if (god_hates_eating(you.religion, defender.type))
        return;

    dprf("god ok");

    // can't eat enemies that leave no corpses...
    if (!mons_class_can_leave_corpse(mons_species(defender.type))
        || defender.is_summoned()
        || defender.flags & MF_HARD_RESET)
    {
        return;
    }

    dprf("corpse ok");

    // chow down.
    _hydra_devour(defender);
}

/**
 * Handle effects that fire when the defender (the target of the attack) is
 * killed.
 *
 * @return  Not sure; it seems to never be checked & always be true?
 */
bool melee_attack::handle_phase_killed()
{
    if (attacker->is_player() && you.form == TRAN_HYDRA
        && defender->is_monster() // better safe than sorry
        && defender->type != MONS_NO_MONSTER) // already reset
    {
        _hydra_consider_devouring(*defender->as_monster());
    }

    // Wyrmbane needs to be notified of deaths, including ones due to aux
    // attacks, but other users of melee_effects() don't want to possibly
    // be called twice. Adding another entry for a single artefact would
    // be overkill, so here we call it by hand. check_unrand_effects()
    // avoided triggering Wyrmbane's death effect earlier in the attack.
    if (unrand_entry && weapon && weapon->unrand_idx == UNRAND_WYRMBANE)
    {
        unrand_entry->melee_effects(weapon, attacker, defender,
                                               true, special_damage);
    }

    return attack::handle_phase_killed();
}

bool melee_attack::handle_phase_end()
{
    if (!cleave_targets.empty())
    {
        attack_cleave_targets(*attacker, cleave_targets, attack_number,
                              effective_attack_number);
    }

    // Check for passive mutation effects.
    if (defender->is_player() && defender->alive() && attacker != defender)
    {
        mons_do_eyeball_confusion();
        mons_do_tendril_disarm();
    }

    return attack::handle_phase_end();
}

/* Initiate the processing of the attack
 *
 * Called from the main code (fight.cc), this method begins the actual combat
 * for a particular attack and is responsible for looping through each of the
 * appropriate phases (which then can call other related phases within
 * themselves).
 *
 * Returns whether combat was completely successful
 *      If combat was not successful, it could be any number of reasons, like
 *      the defender or attacker dying during the attack? or a defender moving
 *      from its starting position.
 */
bool melee_attack::attack()
{
    if (!cleaving)
    {
        cleave_setup();
        if (!handle_phase_attempted())
            return false;
    }

    if (attacker != defender && attacker->is_monster()
        && mons_self_destructs(*attacker->as_monster()))
    {
        attacker->self_destruct();
        return did_hit = perceived_attack = true;
    }

    string saved_gyre_name;
    if (weapon && is_unrandom_artefact(*weapon, UNRAND_GYRE))
    {
        saved_gyre_name = get_artefact_name(*weapon);
        set_artefact_name(*weapon, cleaving ? "quick blade \"Gimble\""
                                            : "quick blade \"Gyre\"");
    }

    // Restore gyre's name before we return. We cannot use an unwind_var here
    // because the precise address of the ARTEFACT_NAME_KEY property might
    // change, for example if a summoned item is reset.
    ON_UNWIND
    {
        if (!saved_gyre_name.empty() && weapon
                && is_unrandom_artefact(*weapon, UNRAND_GYRE))
        {
            set_artefact_name(*weapon, saved_gyre_name);
        }
    };

    // Attacker might have died from effects of cleaving handled prior to this
    if (!attacker->alive())
        return false;

    // We might have killed the kraken target by cleaving a tentacle.
    if (!defender->alive())
    {
        handle_phase_killed();
        handle_phase_end();
        return attack_occurred;
    }

    // Apparently I'm insane for believing that we can still stay general past
    // this point in the combat code, mebe I am! --Cryptic

    // Calculate various ev values and begin to check them to determine the
    // correct handle_phase_ handler.
    const int ev = defender->evasion(EV_IGNORE_NONE, attacker);
    ev_margin = test_hit(to_hit, ev, !attacker->is_player());
    bool shield_blocked = attack_shield_blocked(true);

    // Stuff for god conduct, this has to remain here for scope reasons.
    god_conduct_trigger conducts[3];
    disable_attack_conducts(conducts);

    if (attacker->is_player() && attacker != defender)
    {
        set_attack_conducts(conducts, defender->as_monster());

        if (player_under_penance(GOD_ELYVILON)
            && god_hates_your_god(GOD_ELYVILON)
            && ev_margin >= 0
            && one_chance_in(20))
        {
            simple_god_message(" blocks your attack.", GOD_ELYVILON);
            handle_phase_end();
            return false;
        }
        // Check for stab (and set stab_attempt and stab_bonus)
        player_stab_check();
        // Make sure we hit if we passed the stab check.
        if (stab_attempt && stab_bonus > 0)
        {
            ev_margin = AUTOMATIC_HIT;
            shield_blocked = false;
        }
    }

    if (shield_blocked)
        handle_phase_blocked();
    else
    {
        if (attacker != defender && adjacent(defender->pos(), attack_position)
            && !is_riposte)
        {
            // Check for defender Spines
            do_spines();

            // Spines can kill!
            if (!attacker->alive())
                return false;
        }

        if (ev_margin >= 0)
        {
            bool cont = handle_phase_hit();

            attacker_sustain_passive_damage();

            if (!cont)
            {
                if (!defender->alive())
                    handle_phase_killed();
                handle_phase_end();
                return false;
            }
        }
        else
            handle_phase_dodged();
    }

    // Remove sanctuary if - through some attack - it was violated.
    if (env.sanctuary_time > 0 && attack_occurred && !cancel_attack
        && attacker != defender
        && (is_sanctuary(attack_position) || is_sanctuary(defender->pos()))
        && (attacker->is_player() || attacker->as_monster()->friendly()
                                     && !attacker->confused()))
    {
        remove_sanctuary(true);
    }

    if (attacker->is_player())
        do_miscast();

    // don't crash on banishment
    if (!defender->pos().origin())
        handle_noise(defender->pos());

    // Noisy weapons.
    if (attacker->is_player()
        && weapon
        && is_artefact(*weapon)
        && artefact_property(*weapon, ARTP_NOISE))
    {
        noisy_equipment();
    }

    alert_defender();

    if (!defender->alive())
        handle_phase_killed();

    handle_phase_aux();

    handle_phase_end();

    enable_attack_conducts(conducts);

    return attack_occurred;
}

void melee_attack::check_autoberserk()
{
    if (attacker->is_player())
    {
        for (int i = EQ_FIRST_EQUIP; i < NUM_EQUIP; ++i)
        {
            const item_def *item = you.slot_item(static_cast<equipment_type>(i));
            if (!item)
                continue;

            if (!is_artefact(*item))
                continue;

            if (x_chance_in_y(artefact_property(*item, ARTP_ANGRY), 100))
            {
                attacker->go_berserk(false);
                return;
            }
        }
    }
    else
    {
        for (int i = MSLOT_WEAPON; i <= MSLOT_JEWELLERY; ++i)
        {
            const item_def *item =
                attacker->as_monster()->mslot_item(static_cast<mon_inv_type>(i));
            if (!item)
                continue;

            if (!is_artefact(*item))
                continue;

            if (x_chance_in_y(artefact_property(*item, ARTP_ANGRY), 100))
            {
                attacker->go_berserk(false);
                return;
            }
        }
    }
}

bool melee_attack::check_unrand_effects()
{
    if (unrand_entry && unrand_entry->melee_effects && weapon)
    {
        const bool died = !defender->alive();

        // Don't trigger the Wyrmbane death effect yet; that is done in
        // handle_phase_killed().
        if (weapon->unrand_idx == UNRAND_WYRMBANE && died)
            return true;

        // Recent merge added damage_done to this method call
        unrand_entry->melee_effects(weapon, attacker, defender,
                                    died, damage_done);
        return !defender->alive(); // may have changed
    }

    return false;
}

class AuxAttackType
{
public:
    AuxAttackType(int _damage, string _name) :
    damage(_damage), name(_name) { };
public:
    virtual int get_damage() const { return damage; };
    virtual int get_brand() const { return SPWPN_NORMAL; };
    virtual string get_name() const { return name; };
    virtual string get_verb() const { return get_name(); };
protected:
    const int damage;
    const string name;
};

class AuxConstrict: public AuxAttackType
{
public:
    AuxConstrict()
    : AuxAttackType(0, "grab") { };
};

class AuxKick: public AuxAttackType
{
public:
    AuxKick()
    : AuxAttackType(-1, "kick") { };

    int get_damage() const override
    {
        if (you.has_usable_hooves())
        {
            // Max hoof damage: 10.
            return player_mutation_level(MUT_HOOVES) * 5 / 3;
        }

        if (you.has_usable_talons())
        {
            // Max talon damage: 9.
            return 1 + player_mutation_level(MUT_TALONS);
        }

        // Max spike damage: 8.
        // ... yes, apparently tentacle spikes are "kicks".
        return player_mutation_level(MUT_TENTACLE_SPIKE);
    }

    string get_verb() const override
    {
        if (you.has_usable_talons())
            return "claw";
        if (player_mutation_level(MUT_TENTACLE_SPIKE))
            return "pierce";
        return name;
    }

    string get_name() const override
    {
        if (player_mutation_level(MUT_TENTACLE_SPIKE))
            return "tentacle spike";
        return name;
    }
};

class AuxHeadbutt: public AuxAttackType
{
public:
    AuxHeadbutt()
    : AuxAttackType(5, "headbutt") { };

    int get_damage() const override
    {
        return damage + player_mutation_level(MUT_HORNS) * 3;
    }
};

class AuxPeck: public AuxAttackType
{
public:
    AuxPeck()
    : AuxAttackType(6, "peck") { };
};

class AuxTailslap: public AuxAttackType
{
public:
    AuxTailslap()
    : AuxAttackType(6, "tail-slap") { };

    int get_damage() const override
    {
        return damage + max(0, player_mutation_level(MUT_STINGER) * 2 - 1);
    }

    int get_brand() const override
    {
        return player_mutation_level(MUT_STINGER) ? SPWPN_VENOM : SPWPN_NORMAL;
    }
};

class AuxPunch: public AuxAttackType
{
public:
    AuxPunch()
    : AuxAttackType(5, "punch") { };

    int get_damage() const override
    {
        const int base_dam = damage + you.skill_rdiv(SK_UNARMED_COMBAT, 1, 2);

        if (you.form == TRAN_BLADE_HANDS)
            return base_dam + 6;

        if (you.has_usable_claws())
            return base_dam + roll_dice(you.has_claws(), 3);

        return base_dam;
    }

    string get_name() const override
    {
        if (you.form == TRAN_BLADE_HANDS)
            return "slash";

        if (you.has_usable_claws())
            return "claw";

        if (you.has_usable_tentacles())
            return "tentacle-slap";

        return name;
    }

};

class AuxBite: public AuxAttackType
{
public:
    AuxBite()
    : AuxAttackType(0, "bite") { };

    int get_damage() const override
    {
        const int fang_damage = you.has_usable_fangs() * 2;
        if (player_mutation_level(MUT_ANTIMAGIC_BITE))
            return fang_damage + div_rand_round(you.get_hit_dice(), 3);

        const int str_damage = div_rand_round(max(you.strength()-10, 0), 5);

        if (player_mutation_level(MUT_ACIDIC_BITE))
            return fang_damage + str_damage;

        return fang_damage + str_damage;
    }

    int get_brand() const override
    {
        if (player_mutation_level(MUT_ANTIMAGIC_BITE))
            return SPWPN_ANTIMAGIC;

        if (player_mutation_level(MUT_ACIDIC_BITE))
            return SPWPN_ACID;

        return SPWPN_NORMAL;
    }
};

class AuxPseudopods: public AuxAttackType
{
public:
    AuxPseudopods()
    : AuxAttackType(4, "bludgeon") { };

    int get_damage() const override
    {
        return damage * you.has_usable_pseudopods();
    }
};

class AuxTentacles: public AuxAttackType
{
public:
    AuxTentacles()
    : AuxAttackType(12, "squeeze") { };
};

static const AuxConstrict   AUX_CONSTRICT = AuxConstrict();
static const AuxKick        AUX_KICK = AuxKick();
static const AuxPeck        AUX_PECK = AuxPeck();
static const AuxHeadbutt    AUX_HEADBUTT = AuxHeadbutt();
static const AuxTailslap    AUX_TAILSLAP = AuxTailslap();
static const AuxPunch       AUX_PUNCH = AuxPunch();
static const AuxBite        AUX_BITE = AuxBite();
static const AuxPseudopods  AUX_PSEUDOPODS = AuxPseudopods();
static const AuxTentacles   AUX_TENTACLES = AuxTentacles();

static const AuxAttackType* const aux_attack_types[] =
{
    &AUX_CONSTRICT,
    &AUX_KICK,
    &AUX_HEADBUTT,
    &AUX_PECK,
    &AUX_TAILSLAP,
    &AUX_PUNCH,
    &AUX_BITE,
    &AUX_PSEUDOPODS,
    &AUX_TENTACLES,
};


/* Setup all unarmed (non attack_type) variables
 *
 * Clears any previous unarmed attack information and sets everything from
 * noise_factor to verb and damage. Called after player_aux_choose_uc_attack
 */
void melee_attack::player_aux_setup(unarmed_attack_type atk)
{
    const int num_aux_objs = ARRAYSZ(aux_attack_types);
    const int num_aux_atks = UNAT_LAST_ATTACK - UNAT_FIRST_ATTACK + 1;
    COMPILE_CHECK(num_aux_objs == num_aux_atks);

    ASSERT(atk >= UNAT_FIRST_ATTACK);
    ASSERT(atk <= UNAT_LAST_ATTACK);
    const AuxAttackType* const aux = aux_attack_types[atk - UNAT_FIRST_ATTACK];

    aux_damage = aux->get_damage();
    damage_brand = (brand_type)aux->get_brand();
    aux_attack = aux->get_name();
    aux_verb = aux->get_verb();

    if (atk == UNAT_BITE
        && _vamp_wants_blood_from_monster(defender->as_monster()))
    {
        damage_brand = SPWPN_VAMPIRISM;
    }
}

/**
 * Decide whether the player gets a bonus punch attack.
 *
 * Partially random.
 *
 * @return  Whether the player gets a bonus punch aux attack on this attack.
 */
bool melee_attack::player_gets_aux_punch()
{
    if (!get_form()->can_offhand_punch())
        return false;

    // roll for punch chance based on uc skill & armour penalty
    if (!attacker->fights_well_unarmed(attacker_armour_tohit_penalty
                                       + attacker_shield_tohit_penalty))
    {
        return false;
    }

    // No punching with a shield or 2-handed wpn.
    // Octopodes aren't affected by this, though!
    if (you.species != SP_OCTOPODE && !you.has_usable_offhand())
        return false;

    // Octopodes get more tentacle-slaps.
    return x_chance_in_y(you.species == SP_OCTOPODE ? 3 : 2,
                         6);
}

bool melee_attack::player_aux_test_hit()
{
    // XXX We're clobbering did_hit
    did_hit = false;

    const int evasion = defender->evasion(EV_IGNORE_NONE, attacker);

    if (player_under_penance(GOD_ELYVILON)
        && god_hates_your_god(GOD_ELYVILON)
        && to_hit >= evasion
        && one_chance_in(20))
    {
        simple_god_message(" blocks your attack.", GOD_ELYVILON);
        return false;
    }

    bool auto_hit = one_chance_in(30);

    if (to_hit >= evasion || auto_hit)
        return true;

    mprf("Your %s misses %s.", aux_attack.c_str(),
         defender->name(DESC_THE).c_str());

    return false;
}

/* Controls the looping on available unarmed attacks
 *
 * As the master method for unarmed player combat, this loops through
 * available unarmed attacks, determining whether they hit and - if so -
 * calculating and applying their damage.
 *
 * Returns (defender dead)
 */
bool melee_attack::player_aux_unarmed()
{
    unwind_var<brand_type> save_brand(damage_brand);

    for (int i = UNAT_FIRST_ATTACK; i <= UNAT_LAST_ATTACK; ++i)
    {
        if (!defender->alive())
            break;

        unarmed_attack_type atk = static_cast<unarmed_attack_type>(i);

        if (!_extra_aux_attack(atk))
            continue;

        // Determine and set damage and attack words.
        player_aux_setup(atk);

        if (atk == UNAT_CONSTRICT && !attacker->can_constrict(defender))
            continue;

        to_hit = random2(calc_your_to_hit_unarmed(atk));

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
            return true;

        if (player_aux_test_hit())
        {
            // Upset the monster.
            behaviour_event(defender->as_monster(), ME_WHACK, attacker);
            if (!defender->alive())
                return true;

            if (attack_shield_blocked(true))
                continue;
            if (player_aux_apply(atk))
                return true;
        }
    }

    return false;
}

bool melee_attack::player_aux_apply(unarmed_attack_type atk)
{
    did_hit = true;

    count_action(CACT_MELEE, -1, atk); // aux_attack subtype/auxtype

    aux_damage  = player_stat_modify_damage(aux_damage);

    aux_damage  = random2(aux_damage);

    aux_damage  = player_apply_fighting_skill(aux_damage, true);

    aux_damage  = player_apply_misc_modifiers(aux_damage);

    aux_damage  = player_apply_slaying_bonuses(aux_damage, true);

    aux_damage  = player_apply_final_multipliers(aux_damage);

    if (atk == UNAT_CONSTRICT)
        aux_damage = 0;
    else
        aux_damage = apply_defender_ac(aux_damage);

    aux_damage = inflict_damage(aux_damage, BEAM_MISSILE);
    damage_done = aux_damage;

    if (atk == UNAT_CONSTRICT)
        attacker->start_constricting(*defender);

    if (damage_done > 0 || atk == UNAT_CONSTRICT)
    {
        player_announce_aux_hit();

        if (damage_brand == SPWPN_ACID)
            defender->splash_with_acid(&you, 3);

        if (damage_brand == SPWPN_VENOM && coinflip())
            poison_monster(defender->as_monster(), &you);

        // Normal vampiric biting attack, not if already got stabbing special.
        if (damage_brand == SPWPN_VAMPIRISM && you.species == SP_VAMPIRE
            && (!stab_attempt || stab_bonus <= 0))
        {
            _player_vampire_draws_blood(defender->as_monster(), damage_done);
        }

        if (damage_brand == SPWPN_ANTIMAGIC && you.mutation[MUT_ANTIMAGIC_BITE]
            && damage_done > 0)
        {
            const bool spell_user = defender->antimagic_susceptible();

            antimagic_affects_defender(damage_done * 32);

            // MP drain suppressed under Pakellas, but antimagic still applies.
            if (!have_passive(passive_t::no_mp_regen) || spell_user)
            {
                mprf("You %s %s %s.",
                     have_passive(passive_t::no_mp_regen) ? "disrupt" : "drain",
                     defender->as_monster()->pronoun(PRONOUN_POSSESSIVE).c_str(),
                     spell_user ? "magic" : "power");
            }

            if (!have_passive(passive_t::no_mp_regen)
                && you.magic_points != you.max_magic_points
                && !defender->as_monster()->is_summoned()
                && !mons_is_firewood(*defender->as_monster()))
            {
                int drain = random2(damage_done * 2) + 1;
                // Augment mana drain--1.25 "standard" effectiveness at 0 mp,
                // 0.25 at mana == max_mana
                drain = (int)((1.25 - you.magic_points / you.max_magic_points)
                              * drain);
                if (drain)
                {
                    mpr("You feel invigorated.");
                    inc_mp(drain);
                }
            }
        }
    }
    else // no damage was done
    {
        mprf("You %s %s%s.",
             aux_verb.c_str(),
             defender->name(DESC_THE).c_str(),
             you.can_see(*defender) ? ", but do no damage" : "");
    }

    if (defender->as_monster()->hit_points < 1)
    {
        handle_phase_killed();
        return true;
    }

    return false;
}

void melee_attack::player_announce_aux_hit()
{
    mprf("You %s %s%s%s",
         aux_verb.c_str(),
         defender->name(DESC_THE).c_str(),
         debug_damage_number().c_str(),
         attack_strength_punctuation(damage_done).c_str());
}

string melee_attack::player_why_missed()
{
    const int ev = defender->evasion(EV_IGNORE_NONE, attacker);
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
        const string armour_name = armour ? armour->name(DESC_BASENAME)
                                          : string("armour");

        if (armour_miss && !shield_miss)
            return "Your " + armour_name + " prevents you from hitting ";
        else if (shield_miss && !armour_miss)
            return "Your shield prevents you from hitting ";
        else
            return "Your shield and " + armour_name
                   + " prevent you from hitting ";
    }

    return "You" + evasion_margin_adverb() + " miss ";
}

void melee_attack::player_warn_miss()
{
    did_hit = false;

    mprf("%s%s.",
         player_why_missed().c_str(),
         defender->name(DESC_THE).c_str());

    // Upset only non-sleeping non-fleeing monsters if we missed.
    if (!defender->asleep() && !mons_is_fleeing(*defender->as_monster()))
        behaviour_event(defender->as_monster(), ME_WHACK, attacker);
}

// A couple additive modifiers that should be applied to both unarmed and
// armed attacks.
int melee_attack::player_apply_misc_modifiers(int damage)
{
    if (you.duration[DUR_MIGHT] || you.duration[DUR_BERSERK])
        damage += 1 + random2(10);

    if (you.species != SP_VAMPIRE && you.hunger_state <= HS_STARVING)
        damage -= random2(5);

    return damage;
}

// Multipliers to be applied to the final (pre-stab, pre-AC) damage.
// It might be tempting to try to pick and choose what pieces of the damage
// get affected by such multipliers, but putting them at the end is the
// simplest effect to understand if they aren't just going to be applied
// to the base damage of the weapon.
int melee_attack::player_apply_final_multipliers(int damage)
{
    //cleave damage modifier
    if (cleaving)
        damage = cleave_damage_mod(damage);

    // not additive, statues are supposed to be bad with tiny toothpicks but
    // deal crushing blows with big weapons
    if (you.form == TRAN_STATUE)
        damage = div_rand_round(damage * 3, 2);

    // Can't affect much of anything as a shadow.
    if (you.form == TRAN_SHADOW)
        damage = div_rand_round(damage, 2);

    if (you.duration[DUR_WEAK])
        damage = div_rand_round(damage * 3, 4);

    if (you.duration[DUR_CONFUSING_TOUCH] && wpn_skill == SK_UNARMED_COMBAT)
        return 0;

    return damage;
}

void melee_attack::set_attack_verb(int damage)
{
    if (!attacker->is_player())
        return;

    int weap_type = WPN_UNKNOWN;

    if (Options.has_fake_lang(FLANG_GRUNT))
        damage = HIT_STRONG + 1;

    if (!weapon)
        weap_type = WPN_UNARMED;
    else if (weapon->base_type == OBJ_STAVES)
        weap_type = WPN_STAFF;
    else if (weapon->base_type == OBJ_WEAPONS
             && !is_range_weapon(*weapon))
    {
        weap_type = weapon->sub_type;
    }

    // All weak hits with weapons look the same.
    if (damage < HIT_WEAK
        && weap_type != WPN_UNARMED)
    {
        if (weap_type != WPN_UNKNOWN)
            attack_verb = "hit";
        else
            attack_verb = "clumsily bash";
        return;
    }

    // Take normal hits into account. If the hit is from a weapon with
    // more than one damage type, randomly choose one damage type from
    // it.
    monster_type defender_genus = mons_genus(defender->type);
    switch (weapon ? single_damage_type(*weapon) : -1)
    {
    case DAM_PIERCE:
        if (damage < HIT_MED)
            attack_verb = "puncture";
        else if (damage < HIT_STRONG)
            attack_verb = "impale";
        else
        {
            if (defender->is_monster()
                && defender_visible
                && defender_genus == MONS_HOG)
            {
                attack_verb = "spit";
                verb_degree = "like the proverbial pig";
            }
            else if (defender_genus == MONS_CRAB
                     && Options.has_fake_lang(FLANG_GRUNT))
            {
                attack_verb = "attack";
                verb_degree = "'s weak point";
            }
            else
            {
                static const char * const pierce_desc[][2] =
                {
                    {"spit", "like a pig"},
                    {"skewer", "like a kebab"},
                    {"stick", "like a pincushion"},
                    {"perforate", "like a sieve"}
                };
                const int choice = random2(ARRAYSZ(pierce_desc));
                attack_verb = pierce_desc[choice][0];
                verb_degree = pierce_desc[choice][1];
            }
        }
        break;

    case DAM_SLICE:
        if (damage < HIT_MED)
            attack_verb = "slash";
        else if (damage < HIT_STRONG)
            attack_verb = "slice";
        else if (defender_genus == MONS_OGRE)
        {
            attack_verb = "dice";
            verb_degree = "like an onion";
        }
        else if (defender_genus == MONS_SKELETON)
        {
            attack_verb = "fracture";
            verb_degree = "into splinters";
        }
        else if (defender_genus == MONS_HOG)
        {
            attack_verb = "carve";
            verb_degree = "like the proverbial ham";
        }
        else if (defender_genus == MONS_TENGU && one_chance_in(3))
        {
            attack_verb = "carve";
            verb_degree = "like a turkey";
        }
        else if ((defender_genus == MONS_YAK || defender_genus == MONS_YAKTAUR)
                 && Options.has_fake_lang(FLANG_GRUNT))
            attack_verb = "shave";
        else
        {
            static const char * const slice_desc[][2] =
            {
                {"open",    "like a pillowcase"},
                {"slice",   "like a ripe choko"},
                {"cut",     "into ribbons"},
                {"carve",   "like a ham"},
                {"chop",    "into pieces"}
            };
            const int choice = random2(ARRAYSZ(slice_desc));
            attack_verb = slice_desc[choice][0];
            verb_degree = slice_desc[choice][1];
        }
        break;

    case DAM_BLUDGEON:
        if (damage < HIT_MED)
            attack_verb = one_chance_in(4) ? "thump" : "sock";
        else if (damage < HIT_STRONG)
            attack_verb = "bludgeon";
        else if (defender_genus == MONS_SKELETON)
        {
            attack_verb = "shatter";
            verb_degree = "into splinters";
        }
        else if (defender->type == MONS_GREAT_ORB_OF_EYES)
        {
            attack_verb = "splatter";
            verb_degree = "into a gooey mess";
        }
        else
        {
            static const char * const bludgeon_desc[][2] =
            {
                {"crush",   "like a grape"},
                {"beat",    "like a drum"},
                {"hammer",  "like a gong"},
                {"pound",   "like an anvil"},
                {"flatten", "like a pancake"}
            };
            const int choice = random2(ARRAYSZ(bludgeon_desc));
            attack_verb = bludgeon_desc[choice][0];
            verb_degree = bludgeon_desc[choice][1];
        }
        break;

    case DAM_WHIP:
        if (damage < HIT_MED)
            attack_verb = "whack";
        else if (damage < HIT_STRONG)
            attack_verb = "thrash";
        else
        {
            if (defender->holiness() & (MH_HOLY | MH_NATURAL | MH_DEMONIC))
            {
                attack_verb = "punish";
                verb_degree = ", causing immense pain";
                break;
            }
            else
                attack_verb = "devastate";
        }
        break;

    case -1: // unarmed
    {
        const FormAttackVerbs verbs = get_form(you.form)->uc_attack_verbs;
        if (verbs.weak != nullptr)
        {
            if (damage < HIT_WEAK)
                attack_verb = verbs.weak;
            else if (damage < HIT_MED)
                attack_verb = verbs.medium;
            else if (damage < HIT_STRONG)
                attack_verb = verbs.strong;
            else
                attack_verb = verbs.devastating;
            break;
        }

        if (you.damage_type() == DVORP_CLAWING)
        {
            if (damage < HIT_WEAK)
                attack_verb = "scratch";
            else if (damage < HIT_MED)
                attack_verb = "claw";
            else if (damage < HIT_STRONG)
                attack_verb = "mangle";
            else
                attack_verb = "eviscerate";
        }
        else if (you.damage_type() == DVORP_TENTACLE)
        {
            if (damage < HIT_WEAK)
                attack_verb = "tentacle-slap";
            else if (damage < HIT_MED)
                attack_verb = "bludgeon";
            else if (damage < HIT_STRONG)
                attack_verb = "batter";
            else
                attack_verb = "thrash";
        }
        else
        {
            if (damage < HIT_WEAK)
                attack_verb = "hit";
            else if (damage < HIT_MED)
                attack_verb = "punch";
            else if (damage < HIT_STRONG)
                attack_verb = "pummel";
            else if (defender->is_monster()
                     && (mons_genus(defender->type) == MONS_WORKER_ANT
                         || mons_genus(defender->type) == MONS_FORMICID))
            {
                attack_verb = "squash";
                verb_degree = "like the proverbial ant";
            }
            else
            {
                static const char * const punch_desc[][2] =
                {
                    {"pound",     "into fine dust"},
                    {"pummel",    "like a punching bag"},
                    {"pulverise", ""},
                    {"squash",    "like an ant"}
                };
                const int choice = random2(ARRAYSZ(punch_desc));
                // XXX: could this distinction work better?
                if (choice == 0
                    && defender->is_monster()
                    && mons_has_blood(defender->type))
                {
                    attack_verb = "beat";
                    verb_degree = "into a bloody pulp";
                }
                else
                {
                    attack_verb = punch_desc[choice][0];
                    verb_degree = punch_desc[choice][1];
                }
            }
        }
        break;
    }

    case WPN_UNKNOWN:
    default:
        attack_verb = "hit";
        break;
    }
}

void melee_attack::player_exercise_combat_skills()
{
    if (defender && defender->is_monster()
        && !mons_is_firewood(*defender->as_monster()))
    {
        practise_hitting(weapon);
    }
}

/*
 * Applies god conduct for weapon ego
 *
 * Using speed brand as a chei worshipper, or holy/unholy weapons
 */
void melee_attack::player_weapon_upsets_god()
{
    if (weapon && weapon->base_type == OBJ_WEAPONS
        && god_hates_item_handling(*weapon))
    {
        did_god_conduct(god_hates_item_handling(*weapon), 2);
    }
    else if (weapon && weapon->is_type(OBJ_STAVES, STAFF_FIRE))
        did_god_conduct(DID_FIRE, 1);
}

/* Apply player-specific effects as well as brand damage.
 *
 * Called after damage is calculated, but before unrand effects and before
 * damage is dealt.
 *
 * Returns true if combat should continue, false if it should end here.
 */
bool melee_attack::player_monattk_hit_effects()
{
    player_weapon_upsets_god();

    // Don't even check vampire bloodletting if the monster has already
    // been reset (for example, a spectral weapon who noticed in
    // player_stab_check that it shouldn't exist anymore).
    if (defender->type == MONS_NO_MONSTER)
        return false;

    // Thirsty vampires will try to use a stabbing situation to draw blood.
    if (you.species == SP_VAMPIRE
        && damage_done > 0
        && stab_attempt
        && stab_bonus > 0)
    {
        _player_vampire_draws_blood(defender->as_monster(), damage_done, true);
    }

    if (!defender->alive())
        return false;

    // These effects apply only to monsters that are still alive:

    // Returns true if the hydra was killed by the decapitation, in which case
    // nothing more should be done to the hydra.
    if (consider_decapitation(damage_done))
        return false;

    // Mutually exclusive with (overrides) brand damage!
    special_damage = 0;
    apply_staff_damage();

    if (!defender->alive())
        return false;

    if (special_damage || special_damage_flavour)
    {
        dprf(DIAG_COMBAT, "Special damage to %s: %d, flavour: %d",
             defender->name(DESC_THE).c_str(),
             special_damage, special_damage_flavour);

        special_damage = inflict_damage(special_damage);
        if (special_damage > 0)
            defender->expose_to_element(special_damage_flavour, 2);
    }

    return true;
}

void melee_attack::rot_defender(int amount)
{
    // Keep the defender alive so that we credit kills properly.
    if (defender->rot(attacker, amount, true, true))
    {
        if (needs_message)
        {
            if (defender->is_player())
                mpr("You feel your flesh rotting away!");
            else if (defender->is_monster() && defender_visible)
                mprf("%s looks less resilient!", defender_name(false).c_str());
        }
    }
}

void melee_attack::handle_noise(const coord_def & pos)
{
    // Successful stabs make no noise.
    if (stab_attempt)
        return;

    int loudness = damage_done / 4;

    // All non-stab melee attacks make some noise.
    loudness = max(1, loudness);

    // Cap melee noise at shouting volume.
    loudness = min(12, loudness);

    noisy(loudness, pos, attacker->mid);
}

/**
 * If appropriate, chop a head off the defender. (Usually a hydra.)
 *
 * @param dam           The damage done in the attack that may or may not chop
  *                     off a head.
 * @param damage_type   The type of damage done in the attack.
 * @return              Whether the defender was killed by the decapitation.
 */
bool melee_attack::consider_decapitation(int dam, int damage_type)
{
    const int dam_type = (damage_type != -1) ? damage_type :
                                               attacker->damage_type();
    const brand_type wpn_brand = attacker->damage_brand();

    if (!attack_chops_heads(dam, dam_type, wpn_brand))
        return false;

    decapitate(dam_type);

    if (!defender->alive())
        return true;

    // Only living hydras get to regenerate heads.
    if (!(defender->holiness() & MH_NATURAL))
        return false;

    // What's the largest number of heads the defender can have?
    const int limit = defender->type == MONS_LERNAEAN_HYDRA ? 27
                                                            : MAX_HYDRA_HEADS;

    if (wpn_brand == SPWPN_FLAMING)
    {
        if (defender_visible)
            mpr("The flame cauterises the wound!");
        return false;
    }

    int heads = defender->heads();
    if (heads >= limit - 1)
        return false; // don't overshoot the head limit!

    simple_monster_message(*defender->as_monster(), " grows two more!");
    defender->as_monster()->num_heads += 2;
    defender->heal(8 + random2(8));

    return false;
}

/**
 * Can the given actor lose its heads? (Is it hydra or hydra-like?)
 *
 * @param defender  The actor in question.
 * @return          Whether the given actor is susceptible to head-choppage.
 */
static bool actor_can_lose_heads(const actor* defender)
{
    if (defender->is_monster()
        && defender->as_monster()->has_hydra_multi_attack()
        && defender->type != MONS_SPECTRAL_THING
        && defender->as_monster()->mons_species() != MONS_SERPENT_OF_HELL)
    {
        return true;
    }

    return false;
}

/**
 * Does this attack chop off one of the defender's heads? (Generally only
 * relevant for hydra defenders)
 *
 * @param dam           The damage done in the attack in question.
 * @param dam_type      The vorpal_damage_type of the attack.
 * @param wpn_brand     The brand_type of the attack.
 * @return              Whether the attack will chop off a head.
 */
bool melee_attack::attack_chops_heads(int dam, int dam_type, int wpn_brand)
{
    // hydras and hydra-like things only.
    if (!actor_can_lose_heads(defender))
        return false;

    // no decapitate on riposte (Problematic)
    if (is_riposte)
        return false;

    // Monster attackers+defenders have only a 25% chance of making the
    // chop-check to prevent runaway head inflation.
    // XXX: Tentatively making an exception for spectral weapons
    const bool player_spec_weap = attacker->is_monster()
                                    && attacker->type == MONS_SPECTRAL_WEAPON
                                    && attacker->as_monster()->summoner
                                        == MID_PLAYER;
    if (attacker->is_monster() && defender->is_monster()
        && !player_spec_weap && !one_chance_in(4))
    {
        return false;
    }

    // Only cutting implements.
    if (dam_type != DVORP_SLICING && dam_type != DVORP_CHOPPING
        && dam_type != DVORP_CLAWING)
    {
        return false;
    }

    // Small claws are not big enough.
    if (dam_type == DVORP_CLAWING && attacker->has_claws() < 3)
        return false;

    // You need to have done at least some damage.
    if (dam <= 0 || dam < 4 && coinflip())
        return false;

    // ok, good enough!
    return true;
}

/**
 * Decapitate the (hydra or hydra-like) defender!
 *
 * @param dam_type      The vorpal_damage_type of the attack.
 */
void melee_attack::decapitate(int dam_type)
{
    // Player hydras don't gain or lose heads.
    ASSERT(defender->is_monster());

    const char *verb = nullptr;

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

    int heads = defender->heads();
    if (heads == 1) // will be zero afterwards
    {
        if (defender_visible)
        {
            mprf("%s %s %s last head off!",
                 atk_name(DESC_THE).c_str(),
                 attacker->conj_verb(verb).c_str(),
                 apostrophise(defender_name(true)).c_str());
        }

        if (!defender->is_summoned())
        {
            bleed_onto_floor(defender->pos(), defender->type,
                             defender->as_monster()->hit_points, true);
        }

        defender->hurt(attacker, INSTANT_DEATH);

        return;
    }

    if (defender_visible)
    {
        mprf("%s %s one of %s heads off!",
             atk_name(DESC_THE).c_str(),
             attacker->conj_verb(verb).c_str(),
             apostrophise(defender_name(true)).c_str());
    }

    defender->as_monster()->num_heads--;
}

/**
 * Apply passive retaliation damage from hitting acid monsters.
 */
void melee_attack::attacker_sustain_passive_damage()
{
    // If the defender has been cleaned up, it's too late for anything.
    if (!defender->alive())
        return;

    if (!mons_class_flag(defender->type, M_ACID_SPLASH))
        return;

    if (attacker->res_acid() >= 3)
        return;

    if (!adjacent(attacker->pos(), defender->pos()) || is_riposte)
        return;

    const int acid_strength = resist_adjust_damage(attacker, BEAM_ACID, 5);

    // Spectral weapons can't be corroded (but can take acid damage).
    const bool avatar = attacker->is_monster()
                        && mons_is_avatar(attacker->as_monster()->type);

    if (!avatar)
    {
        if (x_chance_in_y(acid_strength + 1, 30))
            attacker->corrode_equipment();
    }

    if (attacker->is_player())
        mpr(you.hands_act("burn", "!"));
    else
    {
        simple_monster_message(*attacker->as_monster(),
                               " is burned by acid!");
    }
    attacker->hurt(defender, roll_dice(1, acid_strength), BEAM_ACID,
                   KILLED_BY_ACID, "", "", false);
}

int melee_attack::staff_damage(skill_type skill)
{
    if (x_chance_in_y(attacker->skill(SK_EVOCATIONS, 200)
                    + attacker->skill(skill, 100), 3000))
    {
        return random2((attacker->skill(skill, 100)
                      + attacker->skill(SK_EVOCATIONS, 50)) / 80);
    }
    return 0;
}

void melee_attack::apply_staff_damage()
{
    if (!weapon)
        return;

    if (attacker->is_player() && player_mutation_level(MUT_NO_ARTIFICE))
        return;

    if (weapon->base_type != OBJ_STAVES)
        return;

    switch (weapon->sub_type)
    {
    case STAFF_AIR:
        special_damage =
            resist_adjust_damage(defender,
                                 BEAM_ELECTRICITY,
                                 staff_damage(SK_AIR_MAGIC));

        if (special_damage)
        {
            special_damage_message =
                make_stringf("%s %s electrocuted!",
                             defender->name(DESC_THE).c_str(),
                             defender->conj_verb("are").c_str());
            special_damage_flavour = BEAM_ELECTRICITY;
        }

        break;

    case STAFF_COLD:
        special_damage =
            resist_adjust_damage(defender,
                                 BEAM_COLD,
                                 staff_damage(SK_ICE_MAGIC));

        if (special_damage)
        {
            special_damage_message =
                make_stringf(
                    "%s freeze%s %s!",
                    attacker->name(DESC_THE).c_str(),
                    attacker->is_player() ? "" : "s",
                    defender->name(DESC_THE).c_str());
            special_damage_flavour = BEAM_COLD;
        }
        break;

    case STAFF_EARTH:
        special_damage = staff_damage(SK_EARTH_MAGIC);
        special_damage = apply_defender_ac(special_damage);

        if (special_damage > 0)
        {
            special_damage_message =
                make_stringf(
                    "%s crush%s %s!",
                    attacker->name(DESC_THE).c_str(),
                    attacker->is_player() ? "" : "es",
                    defender->name(DESC_THE).c_str());
        }
        break;

    case STAFF_FIRE:
        special_damage =
            resist_adjust_damage(defender,
                                 BEAM_FIRE,
                                 staff_damage(SK_FIRE_MAGIC));

        if (special_damage)
        {
            special_damage_message =
                make_stringf(
                    "%s burn%s %s!",
                    attacker->name(DESC_THE).c_str(),
                    attacker->is_player() ? "" : "s",
                    defender->name(DESC_THE).c_str());
            special_damage_flavour = BEAM_FIRE;

            if (defender->is_player())
                maybe_melt_player_enchantments(BEAM_FIRE, special_damage);
        }
        break;

    case STAFF_POISON:
    {
        if (random2(300) >= attacker->skill(SK_EVOCATIONS, 20) + attacker->skill(SK_POISON_MAGIC, 10))
            return;

        // Base chance at 50% -- like mundane weapons.
        if (x_chance_in_y(80 + attacker->skill(SK_POISON_MAGIC, 10), 160))
            defender->poison(attacker, 2);
        break;
    }

    case STAFF_DEATH:
        special_damage =
            resist_adjust_damage(defender,
                                 BEAM_NEG,
                                 staff_damage(SK_NECROMANCY));

        if (special_damage)
        {
            special_damage_message =
                make_stringf(
                    "%s %s in agony!",
                    defender->name(DESC_THE).c_str(),
                    defender->conj_verb("writhe").c_str());

            attacker->god_conduct(DID_EVIL, 4);
        }
        break;

    case STAFF_SUMMONING:
    case STAFF_POWER:
    case STAFF_CONJURATION:
#if TAG_MAJOR_VERSION == 34
    case STAFF_ENCHANTMENT:
#endif
    case STAFF_ENERGY:
    case STAFF_WIZARDRY:
        break;

    default:
        die("Invalid staff type: %d", weapon->sub_type);
    }
}

/**
 * Calculate the to-hit for an attacker
 *
 * @param random If false, calculate average to-hit deterministically.
 */
int melee_attack::calc_to_hit(bool random)
{
    int mhit = attack::calc_to_hit(random);

    if (attacker->is_player() && !weapon)
    {
        // Just trying to touch is easier than trying to damage.
        if (you.duration[DUR_CONFUSING_TOUCH])
            mhit += maybe_random2(you.dex(), random);

        // TODO: Review this later (transformations getting extra hit
        // almost across the board seems bad) - Cryp71c
        mhit += maybe_random2(get_form()->unarmed_hit_bonus, random);
    }

    return mhit;
}

void melee_attack::player_stab_check()
{
    attack::player_stab_check();
}

/**
 * Can we get a good stab with this weapon?
 */
bool melee_attack::player_good_stab()
{
    return wpn_skill == SK_SHORT_BLADES
           || player_mutation_level(MUT_PAWS)
           || player_equip_unrand(UNRAND_BOOTS_ASSASSIN)
              && (!weapon || is_melee_weapon(*weapon));
}

/* Select the attack verb for attacker
 *
 * If klown, select randomly from klown_attack, otherwise check for any special
 * case attack verbs (tentacles or door/fountain-mimics) and if all else fails,
 * select an attack verb from attack_types based on the ENUM value of attk_type.
 *
 * Returns (attack_verb)
 */
string melee_attack::mons_attack_verb()
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
        return RANDOM_ELEMENT(klown_attack);

    //XXX: then why give them it in the first place?
    if (attk_type == AT_TENTACLE_SLAP && mons_is_tentacle(attacker->type))
        return "slap";

    static const char *attack_types[] =
    {
        "hit",         // including weapon attacks
        "bite",
        "sting",

        // spore
        "release spores at",

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
#if TAG_MAJOR_VERSION == 34
        "snap closed at",
        "splash",
#endif
        "pounce on",
#if TAG_MAJOR_VERSION == 34
        "sting",
#endif
    };
    COMPILE_CHECK(ARRAYSZ(attack_types) == AT_LAST_REAL_ATTACK);

    const int verb_index = attk_type - AT_FIRST_ATTACK;
    ASSERT(verb_index < (int)ARRAYSZ(attack_types));
    return attack_types[verb_index];
}

string melee_attack::mons_attack_desc()
{
    if (!you.can_see(*attacker))
        return "";

    string ret;
    int dist = (attack_position - defender->pos()).rdist();
    if (dist > 1)
    {
        ASSERT(can_reach());
        ret = " from afar";
    }

    if (weapon && attacker->type != MONS_DANCING_WEAPON && attacker->type != MONS_SPECTRAL_WEAPON)
        ret += " with " + weapon->name(DESC_A);

    return ret;
}

void melee_attack::announce_hit()
{
    if (!needs_message || attk_flavour == AF_CRUSH)
        return;

    if (attacker->is_monster())
    {
        mprf("%s %s %s%s%s%s",
             atk_name(DESC_THE).c_str(),
             attacker->conj_verb(mons_attack_verb()).c_str(),
             defender_name(true).c_str(),
             debug_damage_number().c_str(),
             mons_attack_desc().c_str(),
             attack_strength_punctuation(damage_done).c_str());
    }
    else
    {
        if (!verb_degree.empty() && verb_degree[0] != ' '
            && verb_degree[0] != ',' && verb_degree[0] != '\'')
        {
            verb_degree = " " + verb_degree;
        }

        mprf("You %s %s%s%s%s",
             attack_verb.c_str(),
             defender->name(DESC_THE).c_str(),
             verb_degree.c_str(), debug_damage_number().c_str(),
             attack_strength_punctuation(damage_done).c_str());
    }
}

// Returns if the target was actually poisoned by this attack
bool melee_attack::mons_do_poison()
{
    int amount = 1;
    bool force = false;

    if (attk_flavour == AF_POISON_STRONG)
    {
        amount = random_range(attacker->get_hit_dice() * 11 / 3,
                              attacker->get_hit_dice() * 13 / 2);
    }
    else
    {
        amount = random_range(attacker->get_hit_dice() * 2,
                              attacker->get_hit_dice() * 4);
    }

    if (!defender->poison(attacker, amount, force))
        return false;

    if (needs_message)
    {
        mprf("%s poisons %s!",
                atk_name(DESC_THE).c_str(),
                defender_name(true).c_str());
        if (force)
        {
            mprf("%s partially resist%s.",
                defender_name(false).c_str(),
                defender->is_player() ? "" : "s");
        }
    }

    return true;
}

void melee_attack::mons_do_napalm()
{
    if (defender->res_sticky_flame())
        return;

    if (one_chance_in(3))
    {
        if (needs_message)
        {
            mprf("%s %s covered in liquid flames%s",
                 defender_name(false).c_str(),
                 defender->conj_verb("are").c_str(),
                 attack_strength_punctuation(special_damage).c_str());
        }

        if (defender->is_player())
            napalm_player(random2avg(7, 3) + 1, atk_name(DESC_A));
        else
        {
            napalm_monster(
                defender->as_monster(),
                attacker,
                min(4, 1 + random2(attacker->get_hit_dice())/2));
        }
    }
}

static void _print_resist_messages(actor* defender, int base_damage,
                                   beam_type flavour)
{
    // check_your_resists is used for the player case to get additional
    // effects such as Xom amusement, melting of icy effects, etc.
    // mons_adjust_flavoured is used for the monster case to get all of the
    // special message handling ("The ice beast melts!") correct.
    // XXX: there must be a nicer way to do this, especially because we're
    // basically calculating the damage twice in the case where messages
    // are needed.
    if (defender->is_player())
        (void)check_your_resists(base_damage, flavour, "");
    else
    {
        bolt beam;
        beam.flavour = flavour;
        (void)mons_adjust_flavoured(defender->as_monster(),
                                    beam,
                                    base_damage,
                                    true);
    }
}

bool melee_attack::mons_attack_effects()
{
    // may have died earlier, due to e.g. pain bond
    // we could continue with the rest of their attack, but it's a minefield
    // of potential crashes. so, let's not.
    if (attacker->is_monster() && invalid_monster(attacker->as_monster()))
        return false;

    // Monsters attacking themselves don't get attack flavour.
    // The message sequences look too weird. Also, stealing
    // attacks aren't handled until after the damage msg. Also,
    // no attack flavours for dead defenders
    if (attacker != defender && defender->alive())
    {
        mons_apply_attack_flavour();

        if (needs_message && !special_damage_message.empty())
            mpr(special_damage_message);

        if (special_damage > 0)
        {
            inflict_damage(special_damage, special_damage_flavour);
            special_damage = 0;
            special_damage_message.clear();
            special_damage_flavour = BEAM_NONE;
        }

        apply_staff_damage();

        if (needs_message && !special_damage_message.empty())
            mpr(special_damage_message);

        if (special_damage > 0
            && inflict_damage(special_damage, special_damage_flavour))
        {
            defender->expose_to_element(special_damage_flavour, 2);
        }
    }

    if (defender->is_player())
        practise_being_hit();

    // A tentacle may have banished its own parent/sibling and thus itself.
    if (!attacker->alive())
    {
        if (miscast_target == defender)
            do_miscast(); // Will handle a missing defender, too.
        return false;
    }

    // consider_decapitation() returns true if the defender was killed
    // by the decapitation, in which case we should stop the rest of the
    // attack, too.
    if (consider_decapitation(damage_done,
                              attacker->damage_type(attack_number)))
    {
        return false;
    }

    if (attacker != defender && attk_flavour == AF_TRAMPLE)
        do_knockback();

    special_damage = 0;
    special_damage_message.clear();
    special_damage_flavour = BEAM_NONE;

    // Defender banished. Bail since the defender is still alive in the
    // Abyss.
    if (defender->is_banished())
    {
        do_miscast();
        return false;
    }

    if (!defender->alive())
    {
        do_miscast();
        return attacker->alive();
    }

    // Bail if the monster is attacking itself without a weapon, since
    // intrinsic monster attack flavours aren't applied for self-attacks.
    if (attacker == defender && !weapon)
    {
        if (miscast_target == defender)
            do_miscast();
        return false;
    }

    if (!defender->alive())
    {
        do_miscast();
        return attacker->alive();
    }

    if (miscast_target == defender)
        do_miscast();

    // Miscast explosions may kill the attacker.
    if (!attacker->alive())
        return false;

    if (miscast_target == attacker)
        do_miscast();

    // Miscast might have killed the attacker.
    if (!attacker->alive())
        return false;

    return true;
}

void melee_attack::mons_apply_attack_flavour()
{
    // Most of this is from BWR 4.1.2.
    int base_damage = 0;

    attack_flavour flavour = attk_flavour;
    if (flavour == AF_CHAOTIC)
        flavour = random_chaos_attack_flavour();

    // Note that if damage_done == 0 then this code won't be reached
    // unless the flavour is in _flavour_triggers_damageless.
    switch (flavour)
    {
    default:
        // Just to trigger special melee damage effects for regular attacks
        // (e.g. Qazlal's elemental adaptation).
        defender->expose_to_element(BEAM_MISSILE, 2);
        break;

    case AF_MUTATE:
        if (one_chance_in(4))
        {
            defender->malmutate(you.can_see(*attacker) ?
                apostrophise(attacker->name(DESC_PLAIN)) + " mutagenic touch" :
                "mutagenic touch");
        }
        break;

    case AF_POISON:
    case AF_POISON_STRONG:
    case AF_REACH_STING:
        if (one_chance_in(3))
            mons_do_poison();
        break;

    case AF_ROT:
        if (one_chance_in(3))
            rot_defender(1);
        break;

    case AF_FIRE:
        base_damage = attacker->get_hit_dice()
                      + random2(attacker->get_hit_dice());
        special_damage =
            resist_adjust_damage(defender,
                                 BEAM_FIRE,
                                 base_damage);
        special_damage_flavour = BEAM_FIRE;

        if (needs_message && base_damage)
        {
            mprf("%s %s engulfed in flames%s",
                 defender_name(false).c_str(),
                 defender->conj_verb("are").c_str(),
                 attack_strength_punctuation(special_damage).c_str());

            _print_resist_messages(defender, base_damage, BEAM_FIRE);
        }

        defender->expose_to_element(BEAM_FIRE, 2);
        break;

    case AF_COLD:
        base_damage = attacker->get_hit_dice()
                      + random2(2 * attacker->get_hit_dice());
        special_damage =
            resist_adjust_damage(defender,
                                 BEAM_COLD,
                                 base_damage);
        special_damage_flavour = BEAM_COLD;

        if (needs_message && base_damage)
        {
            mprf("%s %s %s%s",
                 atk_name(DESC_THE).c_str(),
                 attacker->conj_verb("freeze").c_str(),
                 defender_name(true).c_str(),
                 attack_strength_punctuation(special_damage).c_str());

            _print_resist_messages(defender, base_damage, BEAM_COLD);
        }

        defender->expose_to_element(BEAM_COLD, 2);
        break;

    case AF_ELEC:
        base_damage = attacker->get_hit_dice()
                      + random2(attacker->get_hit_dice() / 2);

        special_damage =
            resist_adjust_damage(defender,
                                 BEAM_ELECTRICITY,
                                 base_damage);
        special_damage_flavour = BEAM_ELECTRICITY;

        if (needs_message && base_damage)
        {
            mprf("%s %s %s%s",
                 atk_name(DESC_THE).c_str(),
                 attacker->conj_verb("shock").c_str(),
                 defender_name(true).c_str(),
                 attack_strength_punctuation(special_damage).c_str());

            _print_resist_messages(defender, base_damage, BEAM_ELECTRICITY);
        }

        dprf(DIAG_COMBAT, "Shock damage: %d", special_damage);
        defender->expose_to_element(BEAM_ELECTRICITY, 2);
        break;

        // Combines drain speed and vampiric.
    case AF_SCARAB:
        if (x_chance_in_y(3, 5))
            drain_defender_speed();

        // deliberate fall-through
    case AF_VAMPIRIC:
        if (!(defender->holiness() & MH_NATURAL))
            break;

        // Disallow draining of summoned monsters.
        if (defender->is_summoned())
            break;

        if (defender->stat_hp() < defender->stat_maxhp())
        {
            int healed = resist_adjust_damage(defender, BEAM_NEG,
                                              1 + random2(damage_done));
            if (healed)
            {
                attacker->heal(healed);
                if (needs_message)
                {
                    mprf("%s %s strength from %s injuries!",
                         atk_name(DESC_THE).c_str(),
                         attacker->conj_verb("draw").c_str(),
                         def_name(DESC_ITS).c_str());
                }
            }
        }
        break;

    case AF_DRAIN_STR:
    case AF_DRAIN_INT:
    case AF_DRAIN_DEX:
        if (one_chance_in(20) || one_chance_in(3))
        {
            stat_type drained_stat = (flavour == AF_DRAIN_STR ? STAT_STR :
                                      flavour == AF_DRAIN_INT ? STAT_INT
                                                              : STAT_DEX);
            defender->drain_stat(drained_stat, 1);
        }
        break;

    case AF_HUNGER:
        if (defender->holiness() & MH_UNDEAD)
            break;

        defender->make_hungry(you.hunger / 4, false);
        break;

    case AF_BLINK:
        // blinking can kill, delay the call
        if (one_chance_in(3))
            blink_fineff::schedule(attacker);
        break;

    case AF_CONFUSE:
        if (attk_type == AT_SPORE)
        {
            if (defender->is_unbreathing())
                break;

            monster *attkmon = attacker->as_monster();
            attkmon->set_hit_dice(attkmon->get_experience_level() - 1);
            if (attkmon->get_experience_level() <= 0)
                attacker->as_monster()->suicide();

            if (defender_visible)
            {
                mprf("%s %s engulfed in a cloud of spores!",
                     defender->name(DESC_THE).c_str(),
                     defender->conj_verb("are").c_str());
            }
        }

        if (one_chance_in(3))
        {
            defender->confuse(attacker,
                              1 + random2(3+attacker->get_hit_dice()));
        }
        break;

    case AF_DRAIN_XP:
        if (coinflip())
            drain_defender();
        break;

    case AF_PARALYSE:
    {
        // Only wasps at the moment, so Zin vitalisation
        // protects from the paralysis and slow.
        if (defender->is_player() && you.duration[DUR_DIVINE_STAMINA] > 0)
        {
            mpr("Your divine stamina protects you from poison!");
            break;
        }

        // doesn't affect poison-immune enemies
        if (defender->res_poison() >= 3)
            break;

        if (attacker->type == MONS_HORNET || one_chance_in(3))
        {
            int dmg = random_range(attacker->get_hit_dice() * 3 / 2,
                                   attacker->get_hit_dice() * 5 / 2);
            defender->poison(attacker, dmg);
        }

        int paralyse_roll = attacker->type == MONS_HORNET ? 4 : 8;

        const bool strong_result = one_chance_in(paralyse_roll);

        if (strong_result
            && !(defender->res_poison() > 0 || x_chance_in_y(2, 3)))
        {
            defender->paralyse(attacker, roll_dice(1, 3));
        }
        else if (strong_result
                 || !(defender->res_poison() > 0 || x_chance_in_y(2, 3)))
        {
            defender->slow_down(attacker, roll_dice(1, 3));
        }

        break;
    }

    case AF_ACID:
        defender->splash_with_acid(attacker, 3);
        break;

    case AF_CORRODE:
        defender->corrode_equipment(atk_name(DESC_THE).c_str());
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
                 defender_name(true).c_str());
        }

        defender->go_berserk(false);
        break;

    case AF_STICKY_FLAME:
        mons_do_napalm();
        break;

    case AF_CHAOTIC:
        chaos_affects_defender();
        break;

    case AF_STEAL:
        // Ignore monsters, for now.
        if (!defender->is_player())
            break;

        attacker->as_monster()->steal_item_from_player();
        break;

    case AF_HOLY:
        if (defender->holy_wrath_susceptible())
            special_damage = attk_damage * 0.75;

        if (needs_message && special_damage)
        {
            mprf("%s %s %s%s",
                 atk_name(DESC_THE).c_str(),
                 attacker->conj_verb("sear").c_str(),
                 defender_name(true).c_str(),
                 attack_strength_punctuation(special_damage).c_str());

        }
        break;

    case AF_ANTIMAGIC:
        antimagic_affects_defender(attacker->get_hit_dice() * 12);

        if (mons_genus(attacker->type) == MONS_VINE_STALKER
            && attacker->is_monster())
        {
            const bool spell_user = defender->antimagic_susceptible();

            if (you.can_see(*attacker) || you.can_see(*defender))
            {
                mprf("%s drains %s %s.",
                     attacker->name(DESC_THE).c_str(),
                     defender->pronoun(PRONOUN_POSSESSIVE).c_str(),
                     spell_user ? "magic" : "power");
            }

            monster* vine = attacker->as_monster();
            if (vine->has_ench(ENCH_ANTIMAGIC)
                && (defender->is_player()
                    || (!defender->as_monster()->is_summoned()
                        && !mons_is_firewood(*defender->as_monster()))))
            {
                mon_enchant me = vine->get_ench(ENCH_ANTIMAGIC);
                vine->lose_ench_duration(me, random2(damage_done) + 1);
                simple_monster_message(*attacker->as_monster(),
                                       spell_user
                                       ? " looks very invigorated."
                                       : " looks invigorated.");
            }
        }
        break;

    case AF_PAIN:
        pain_affects_defender();
        break;

    case AF_ENSNARE:
        if (one_chance_in(3))
            ensnare(defender);
        break;

    case AF_CRUSH:
        if (needs_message)
        {
            mprf("%s %s %s.",
                 atk_name(DESC_THE).c_str(),
                 attacker->conj_verb("grab").c_str(),
                 defender_name(true).c_str());
        }
        attacker->start_constricting(*defender);
        // if you got grabbed, interrupt stair climb and passwall
        if (defender->is_player())
            stop_delay(true);
        break;

    case AF_ENGULF:
        if (x_chance_in_y(2, 3) && attacker->can_constrict(defender))
        {
            if (defender->is_player() && !you.duration[DUR_WATER_HOLD]
                && !you.duration[DUR_WATER_HOLD_IMMUNITY])
            {
                you.duration[DUR_WATER_HOLD] = 10;
                you.props["water_holder"].get_int() = attacker->as_monster()->mid;
            }
            else if (defender->is_monster()
                     && !defender->as_monster()->has_ench(ENCH_WATER_HOLD))
            {
                defender->as_monster()->add_ench(mon_enchant(ENCH_WATER_HOLD, 1,
                                                             attacker, 1));
            }
            else
                return; //Didn't apply effect; no message

            if (needs_message)
            {
                mprf("%s %s %s in water!",
                     atk_name(DESC_THE).c_str(),
                     attacker->conj_verb("engulf").c_str(),
                     defender_name(true).c_str());
            }
        }

        defender->expose_to_element(BEAM_WATER, 0);
        break;

    case AF_PURE_FIRE:
        if (attacker->type == MONS_FIRE_VORTEX)
            attacker->as_monster()->suicide(-10);

        special_damage = (attacker->get_hit_dice() * 3 / 2
                          + random2(attacker->get_hit_dice()));
        special_damage = defender->apply_ac(special_damage, 0, AC_HALF);
        special_damage = resist_adjust_damage(defender,
                                              BEAM_FIRE,
                                              special_damage);

        if (needs_message && special_damage)
        {
            mprf("%s %s %s!",
                    atk_name(DESC_THE).c_str(),
                    attacker->conj_verb("burn").c_str(),
                    defender_name(true).c_str());

            _print_resist_messages(defender, special_damage, BEAM_FIRE);
        }

        defender->expose_to_element(BEAM_FIRE, 2);
        break;

    case AF_DRAIN_SPEED:
        if (x_chance_in_y(3, 5))
            drain_defender_speed();
        break;

    case AF_VULN:
        if (one_chance_in(3))
        {
            bool visible_effect = false;
            if (defender->is_player())
            {
                if (!you.duration[DUR_LOWERED_MR])
                    visible_effect = true;
                you.increase_duration(DUR_LOWERED_MR, 20 + random2(20), 40);
            }
            else
            {
                if (!defender->as_monster()->has_ench(ENCH_LOWERED_MR))
                    visible_effect = true;
                mon_enchant lowered_mr(ENCH_LOWERED_MR, 1, attacker,
                                       (20 + random2(20)) * BASELINE_DELAY);
                defender->as_monster()->add_ench(lowered_mr);
            }

            if (needs_message && visible_effect)
            {
                mprf("%s magical defenses are stripped away!",
                     def_name(DESC_ITS).c_str());
            }
        }
        break;

    case AF_SHADOWSTAB:
        attacker->as_monster()->del_ench(ENCH_INVIS, true);
        break;

    case AF_DROWN:
        if (attacker->type == MONS_DROWNED_SOUL)
            attacker->as_monster()->suicide(-1000);

        if (defender->res_water_drowning() <= 0)
        {
            special_damage = attacker->get_hit_dice() * 3 / 4
                            + random2(attacker->get_hit_dice() * 3 / 4);
            special_damage_flavour = BEAM_WATER;
            kill_type = KILLED_BY_WATER;

            if (needs_message)
            {
                mprf("%s %s %s%s",
                    atk_name(DESC_THE).c_str(),
                    attacker->conj_verb("drown").c_str(),
                    defender_name(true).c_str(),
                    attack_strength_punctuation(special_damage).c_str());
            }
        }
        break;

    case AF_WEAKNESS:
        if (coinflip())
            defender->weaken(attacker, 12);
        break;
    }
}

void melee_attack::do_passive_freeze()
{
    if (you.mutation[MUT_PASSIVE_FREEZE]
        && attacker->alive()
        && adjacent(you.pos(), attacker->as_monster()->pos()))
    {
        bolt beam;
        beam.flavour = BEAM_COLD;
        beam.thrower = KILL_YOU;

        monster* mon = attacker->as_monster();

        const int orig_hurted = random2(11);
        int hurted = mons_adjust_flavoured(mon, beam, orig_hurted);

        if (!hurted)
            return;

        simple_monster_message(*mon, " is very cold.");

#ifndef USE_TILE_LOCAL
        flash_monster_colour(mon, LIGHTBLUE, 200);
#endif

        mon->hurt(&you, hurted);

        if (mon->alive())
        {
            mon->expose_to_element(BEAM_COLD, orig_hurted);
            print_wounds(*mon);
        }
    }
}

#if TAG_MAJOR_VERSION == 34
void melee_attack::do_passive_heat()
{
    if (you.species == SP_LAVA_ORC && temperature_effect(LORC_PASSIVE_HEAT)
        && attacker->alive()
        && grid_distance(you.pos(), attacker->as_monster()->pos()) == 1)
    {
        bolt beam;
        beam.flavour = BEAM_FIRE;
        beam.thrower = KILL_YOU;

        monster* mon = attacker->as_monster();

        const int orig_hurted = random2(5);
        int hurted = mons_adjust_flavoured(mon, beam, orig_hurted);

        if (!hurted)
            return;

        simple_monster_message(*mon, " is singed by your heat.");

#ifndef USE_TILE
        flash_monster_colour(mon, LIGHTRED, 200);
#endif

        mon->hurt(&you, hurted);

        if (mon->alive())
        {
            mon->expose_to_element(BEAM_FIRE, orig_hurted);
            print_wounds(*mon);
        }
    }
}
#endif

void melee_attack::mons_do_eyeball_confusion()
{
    if (you.mutation[MUT_EYEBALLS]
        && attacker->alive()
        && adjacent(you.pos(), attacker->as_monster()->pos())
        && x_chance_in_y(player_mutation_level(MUT_EYEBALLS), 20))
    {
        const int ench_pow = player_mutation_level(MUT_EYEBALLS) * 30;
        monster* mon = attacker->as_monster();

        if (mon->check_res_magic(ench_pow) <= 0)
        {
            mprf("The eyeballs on your body gaze at %s.",
                 mon->name(DESC_THE).c_str());

            if (!mon->check_clarity(false))
            {
                mon->add_ench(mon_enchant(ENCH_CONFUSION, 0, &you,
                                          30 + random2(100)));
            }
        }
    }
}

void melee_attack::mons_do_tendril_disarm()
{
    monster* mon = attacker->as_monster();
    // some rounding errors here, but not significant
    const int adj_mon_hd = mon->is_fighter() ? mon->get_hit_dice() * 3 / 2
                                             : mon->get_hit_dice();

    if (player_mutation_level(MUT_TENDRILS)
        && one_chance_in(5)
        && (random2(you.dex()) > adj_mon_hd
            || random2(you.strength()) > adj_mon_hd))
    {
        item_def* mons_wpn = mon->disarm();
        if (mons_wpn)
        {
            mprf("Your tendrils lash around %s %s and pull it to the ground!",
                 apostrophise(mon->name(DESC_THE)).c_str(),
                 mons_wpn->name(DESC_PLAIN).c_str());
        }
    }
}

void melee_attack::do_spines()
{
    // Monsters only get struck on their first attack per round
    if (attacker->is_monster() && effective_attack_number > 0)
        return;

    if (defender->is_player())
    {

		int wudzu_spine = 0;
		if (have_passive(passive_t::spiny_thorns))
			wudzu_spine = 1;

        const int mut = (you.form == TRAN_PORCUPINE) ? 3
                        : min(3,player_mutation_level(MUT_SPINY)+wudzu_spine);

        if (mut && attacker->alive() && coinflip())
        {
            int dmg = random_range(mut, you.experience_level + mut);
            int hurt = attacker->apply_ac(dmg);

            dprf(DIAG_COMBAT, "Spiny: dmg = %d hurt = %d", dmg, hurt);

            if (hurt <= 0)
                return;

            simple_monster_message(*attacker->as_monster(),
                                   " is struck by your spines.");

            attacker->hurt(&you, hurt);
        }
    }
    else if (defender->as_monster()->is_spiny())
    {
        // Thorn hunters can attack their own brambles without injury
        if (defender->type == MONS_BRIAR_PATCH
            && attacker->type == MONS_THORN_HUNTER
            // Dithmenos' shadow can't take damage, don't spam.
            || attacker->type == MONS_PLAYER_SHADOW)
        {
            return;
        }

        if (attacker->alive() && one_chance_in(3))
        {
            int dmg = roll_dice(5, 4);
            int hurt = attacker->apply_ac(dmg);
            dprf(DIAG_COMBAT, "Spiny: dmg = %d hurt = %d", dmg, hurt);

            if (hurt <= 0)
                return;
            if (you.can_see(*defender) || attacker->is_player())
            {
                mprf("%s %s struck by %s %s.", attacker->name(DESC_THE).c_str(),
                     attacker->conj_verb("are").c_str(),
                     defender->name(DESC_ITS).c_str(),
                     defender->type == MONS_BRIAR_PATCH ? "thorns"
                                                        : "spines");
            }
            attacker->hurt(defender, hurt, BEAM_MISSILE, KILLED_BY_SPINES);
        }
    }
}

void melee_attack::emit_foul_stench()
{
    monster* mon = attacker->as_monster();

    if (you.mutation[MUT_FOUL_STENCH]
        && attacker->alive()
        && adjacent(you.pos(), mon->pos()))
    {
        const int mut = player_mutation_level(MUT_FOUL_STENCH);

        if (one_chance_in(3))
            mon->sicken(50 + random2(100));

        if (damage_done > 4 && x_chance_in_y(mut, 5)
            && !cell_is_solid(mon->pos())
            && !cloud_at(mon->pos()))
        {
            mpr("You emit a cloud of foul miasma!");
            place_cloud(CLOUD_MIASMA, mon->pos(), 5 + random2(6), &you);
        }
    }
}

void melee_attack::do_minotaur_retaliation()
{
    if (!defender->is_player())
    {
        // monsters have no STR or DEX
        if (x_chance_in_y(2, 5))
        {
            int hurt = attacker->apply_ac(random2(21));
            if (you.see_cell(defender->pos()))
            {
                const string defname = defender->name(DESC_THE);
                mprf("%s furiously retaliates!", defname.c_str());
                if (hurt <= 0)
                {
                    mprf("%s headbutts %s, but does no damage.", defname.c_str(),
                         attacker->name(DESC_THE).c_str());
                }
                else
                {
                    mprf("%s headbutts %s%s", defname.c_str(),
                         attacker->name(DESC_THE).c_str(),
                         attack_strength_punctuation(hurt).c_str());
                }
            }
            if (hurt > 0)
            {
                attacker->hurt(defender, hurt, BEAM_MISSILE,
                               KILLED_BY_HEADBUTT);
            }
        }
        return;
    }

    if (!form_keeps_mutations())
    {
        // You are in a non-minotaur form.
        return;
    }
    // This will usually be 2, but could be 3 if the player mutated more.
    const int mut = player_mutation_level(MUT_HORNS);

    if (5 * you.strength() + 7 * you.dex() > random2(600))
    {
        // Use the same damage formula as a regular headbutt.
        int dmg = 5 + mut * 3;
        dmg = player_stat_modify_damage(dmg);
        dmg = random2(dmg);
        dmg = player_apply_fighting_skill(dmg, true);
        dmg = player_apply_misc_modifiers(dmg);
        dmg = player_apply_slaying_bonuses(dmg, true);
        dmg = player_apply_final_multipliers(dmg);
        int hurt = attacker->apply_ac(dmg);

        mpr("You furiously retaliate!");
        dprf(DIAG_COMBAT, "Retaliation: dmg = %d hurt = %d", dmg, hurt);
        if (hurt <= 0)
        {
            mprf("You headbutt %s, but do no damage.",
                 attacker->name(DESC_THE).c_str());
            return;
        }
        else
        {
            mprf("You headbutt %s%s",
                 attacker->name(DESC_THE).c_str(),
                 attack_strength_punctuation(hurt).c_str());
            attacker->hurt(&you, hurt);
        }
    }
}

void melee_attack::do_wudzu_hat_retaliation()
{
    if (defender->cannot_act()
        || defender->confused()
        || !attacker->alive()
        || defender->is_player() && you.duration[DUR_LIFESAVING])
    {
        return;
    }

    const int mut = player_mutation_level(MUT_HORNS);

    if (5 * you.strength() + 7 * you.dex() > random2(600))
    {
        // Use the same damage formula as a regular headbutt, plus some for thorns.
        int dmg = 7 + mut * 3;
        dmg = player_stat_modify_damage(dmg);
        dmg = random2(dmg);
        dmg = player_apply_fighting_skill(dmg, true);
        dmg = player_apply_misc_modifiers(dmg);
        dmg = player_apply_slaying_bonuses(dmg, true);
        dmg = player_apply_final_multipliers(dmg);
        int hurt = attacker->apply_ac(dmg);

        mpr("You furiously retaliate!");
        dprf(DIAG_COMBAT, "Retaliation: dmg = %d hurt = %d", dmg, hurt);
        if (hurt <= 0)
        {
            mprf("You headbutt %s, but do no damage.",
                 attacker->name(DESC_THE).c_str());
            return;
        }
        else
        {
            mprf("You headbutt %s%s",
                 attacker->name(DESC_THE).c_str(),
                 attack_strength_punctuation(hurt).c_str());
            attacker->hurt(&you, hurt);
        }
    }
}

/**
 * Launch a long blade counterattack against the attacker. No sanity checks;
 * caller beware!
 *
 * XXX: might be wrong for deep elf blademasters with a long blade in only
 * one hand
 */
void melee_attack::riposte()
{
    if (you.see_cell(defender->pos()))
    {
        mprf("%s riposte%s.", defender->name(DESC_THE).c_str(),
             defender->is_player() ? "" : "s");
    }
    melee_attack attck(defender, attacker, 0, effective_attack_number + 1);
    attck.is_riposte = true;
    attck.attack();
}

bool melee_attack::do_knockback(bool trample)
{
    if (defender->is_stationary())
        return false; // don't even print a message

    const int size_diff =
        attacker->body_size(PSIZE_BODY) - defender->body_size(PSIZE_BODY);
    const coord_def old_pos = defender->pos();
    const coord_def new_pos = old_pos + old_pos - attack_position;

    if (!x_chance_in_y(size_diff + 3, 6)
        // need a valid tile
        || !defender->is_habitable_feat(grd(new_pos))
        // don't trample anywhere the attacker can't follow
        || !attacker->is_habitable_feat(grd(old_pos))
        // don't trample into a monster - or do we want to cause a chain
        // reaction here?
        || actor_at(new_pos)
        // Prevent trample/drown combo when flight is expiring
        || defender->is_player() && need_expiration_warning(new_pos))
    {
        if (needs_message)
        {
            mprf("%s %s %s ground!",
                 defender_name(false).c_str(),
                 defender->conj_verb("hold").c_str(),
                 defender->pronoun(PRONOUN_POSSESSIVE).c_str());
        }

        return false;
    }

    if (needs_message)
    {
        const bool can_stumble = !defender->airborne()
                                  && !defender->incapacitated();
        const string verb = can_stumble ? "stumble" : "are shoved";
        mprf("%s %s backwards!",
             defender_name(false).c_str(),
             defender->conj_verb(verb).c_str());
    }

    // Schedule following _before_ actually trampling -- if the defender
    // is a player, a shaft trap will unload the level. If trampling will
    // somehow fail, move attempt will be ignored.
    if (trample)
        trample_follow_fineff::schedule(attacker, old_pos);

    if (defender->is_player())
    {
        move_player_to_grid(new_pos, false);
        // Interrupt stair travel and passwall.
        stop_delay(true);
    }
    else
        defender->move_to_pos(new_pos);

    return true;
}

/**
 * Find the list of targets to cleave after hitting the main target.
 */
void melee_attack::cleave_setup()
{
    // Don't cleave on a self-attack.
    if (attacker->pos() == defender->pos())
        return;

    // We need to get the list of the remaining potential targets now because
    // if the main target dies, its position will be lost.
    get_cleave_targets(*attacker, defender->pos(), cleave_targets,
                       attack_number);
    // We're already attacking this guy.
    cleave_targets.pop_front();
}

// cleave damage modifier for additional attacks: 70% of base damage
int melee_attack::cleave_damage_mod(int dam)
{
    if (weapon && is_unrandom_artefact(*weapon, UNRAND_GYRE))
        return dam;
    return div_rand_round(dam * 7, 10);
}

void melee_attack::chaos_affect_actor(actor *victim)
{
    ASSERT(victim); // XXX: change to actor &victim
    melee_attack attk(victim, victim);
    attk.weapon = nullptr;
    attk.fake_chaos_attack = true;
    attk.chaos_affects_defender();
    attk.do_miscast();
    if (!attk.special_damage_message.empty()
        && you.can_see(*victim))
    {
        mpr(attk.special_damage_message);
    }
}

/**
 * Does the player get to use the given aux attack during this melee attack?
 *
 * Partially random.
 *
 * @param atk   The type of aux attack being considered.
 * @return      Whether the player may use the given aux attack.
 */
bool melee_attack::_extra_aux_attack(unarmed_attack_type atk)
{
    if (atk != UNAT_CONSTRICT
        && you.strength() + you.dex() <= random2(50))
    {
        return false;
    }

    switch (atk)
    {
    case UNAT_CONSTRICT:
        return player_mutation_level(MUT_CONSTRICTING_TAIL)
                || you.species == SP_OCTOPODE && you.has_usable_tentacle();

    case UNAT_KICK:
        return you.has_usable_hooves()
               || you.has_usable_talons()
               || player_mutation_level(MUT_TENTACLE_SPIKE);

    case UNAT_PECK:
        return player_mutation_level(MUT_BEAK) && !one_chance_in(3);

    case UNAT_HEADBUTT:
        return player_mutation_level(MUT_HORNS) && !one_chance_in(3);

    case UNAT_TAILSLAP:
        return you.has_usable_tail() && coinflip();

    case UNAT_PSEUDOPODS:
        return you.has_usable_pseudopods() && !one_chance_in(3);

    case UNAT_TENTACLES:
        return you.has_usable_tentacles() && !one_chance_in(3);

    case UNAT_BITE:
        return player_mutation_level(MUT_ANTIMAGIC_BITE)
               || (you.has_usable_fangs()
                   || player_mutation_level(MUT_ACIDIC_BITE))
                   && x_chance_in_y(2, 5);

    case UNAT_PUNCH:
        return player_gets_aux_punch();

    default:
        return false;
    }
}

// TODO: Potentially move this, may or may not belong here (may not
// even belong as its own function, could be integrated with the general
// to-hit method
// Returns the to-hit for your extra unarmed attacks.
// DOES NOT do the final roll (i.e., random2(your_to_hit)).
int melee_attack::calc_your_to_hit_unarmed(int uattack)
{
    int your_to_hit;

    your_to_hit = 1300
                + you.dex() * 75
                + you.skill(SK_FIGHTING, 30);
    your_to_hit /= 100;

    your_to_hit -= 5 * you.inaccuracy();

    if (player_mutation_level(MUT_EYEBALLS))
        your_to_hit += 2 * player_mutation_level(MUT_EYEBALLS) + 1;

    if (you.species != SP_VAMPIRE && you.hunger_state <= HS_STARVING)
        your_to_hit -= 3;

    your_to_hit += slaying_bonus();

    return your_to_hit;
}

bool melee_attack::using_weapon()
{
    return weapon && is_melee_weapon(*weapon);
}

int melee_attack::weapon_damage()
{
    if (!using_weapon())
        return 0;

    return property(*weapon, PWPN_DAMAGE);
}

int melee_attack::calc_mon_to_hit_base()
{
    const bool fighter = attacker->is_monster()
                         && attacker->as_monster()->is_fighter();
    const int hd_mult = fighter ? 25 : 15;
    return 18 + attacker->get_hit_dice() * hd_mult / 10;
}

/**
 * Add modifiers to the base damage.
 * Currently only relevant for monsters.
 */
int melee_attack::apply_damage_modifiers(int damage, int damage_max)
{
    ASSERT(attacker->is_monster());
    monster *as_mon = attacker->as_monster();

    // Berserk/mighted monsters get bonus damage.
    if (as_mon->has_ench(ENCH_MIGHT) || as_mon->has_ench(ENCH_BERSERK))
        damage = damage * 3 / 2;

    if (as_mon->has_ench(ENCH_IDEALISED))
        damage *= 2; // !

    if (as_mon->has_ench(ENCH_WEAK))
        damage = damage * 2 / 3;

    // If the defender is asleep, the attacker gets a stab.
    if (defender && (defender->asleep()
                     || (attk_flavour == AF_SHADOWSTAB
                         &&!defender->can_see(*attacker))))
    {
        damage = damage * 5 / 2;
        dprf(DIAG_COMBAT, "Stab damage vs %s: %d",
             defender->name(DESC_PLAIN).c_str(),
             damage);
    }

    if (cleaving)
        damage = cleave_damage_mod(damage);

    return damage;
}

int melee_attack::calc_damage()
{
    // Constriction deals damage over time, not when grabbing.
    if (attk_flavour == AF_CRUSH)
        return 0;

    return attack::calc_damage();
}

/* TODO: This code is only used from melee_attack methods, but perhaps it
 * should be ambigufied and moved to the actor class
 * Should life protection protect from this?
 *
 * Should eventually remove in favor of player/monster symmetry
 *
 * Called when stabbing and for bite attacks.
 *
 * Returns true if blood was drawn.
 */
bool melee_attack::_player_vampire_draws_blood(const monster* mon, const int damage,
                                               bool needs_bite_msg)
{
    ASSERT(you.species == SP_VAMPIRE);

    if (!_vamp_wants_blood_from_monster(mon) ||
        (!adjacent(defender->pos(), attack_position) && needs_bite_msg))
    {
        return false;
    }

    // Now print message, need biting unless already done (never for bat form!)
    if (needs_bite_msg && you.form != TRAN_BAT)
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
        int heal = 2 + random2(damage) + random2(damage);
        if (heal > you.experience_level)
            heal = you.experience_level;

        if (heal > 0 && !you.duration[DUR_DEATHS_DOOR])
        {
            inc_hp(heal);
            canned_msg(MSG_GAIN_HEALTH);
        }
    }

    // Gain nutrition.
    if (you.hunger_state != HS_ENGORGED)
        lessen_hunger(30 + random2avg(59, 2), false);

    return true;
}

bool melee_attack::_vamp_wants_blood_from_monster(const monster* mon)
{
    return you.species == SP_VAMPIRE
           && you.hunger_state < HS_SATIATED
           && !mon->is_summoned()
           && mons_has_blood(mon->type)
           && !testbits(mon->flags, MF_SPECTRALISED);
}
