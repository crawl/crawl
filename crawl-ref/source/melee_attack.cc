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
#include "art-enum.h"
#include "attitude-change.h"
#include "beam.h"
#include "cloud.h"
#include "database.h"
#include "delay.h"
#include "effects.h"
#include "env.h"
#include "exercise.h"
#include "fineff.h"
#include "fight.h"
#include "fprop.h"
#include "food.h"
#include "goditem.h"
#include "invent.h"
#include "items.h"
#include "itemname.h"
#include "itemprop.h"
#include "item_use.h"
#include "libutil.h"
#include "makeitem.h"
#include "message.h"
#include "misc.h"
#include "mon-abil.h"
#include "mon-act.h"
#include "mon-behv.h"
#include "mon-cast.h"
#include "mon-clone.h"
#include "mon-place.h"
#include "terrain.h"
#include "mgen_data.h"
#include "mon-stuff.h"
#include "mon-util.h"
#include "mutation.h"
#include "options.h"
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
#include "target.h"
#include "transform.h"
#include "traps.h"
#include "travel.h"
#include "hints.h"
#include "unwind.h"
#include "view.h"
#include "shout.h"
#include "xom.h"

#ifdef NOTE_DEBUG_CHAOS_BRAND
    #define NOTE_DEBUG_CHAOS_EFFECTS
#endif

#ifdef NOTE_DEBUG_CHAOS_EFFECTS
    #include "notes.h"
#endif

// Odd helper function, why is this declared like this?
#define DID_AFFECT() \
{ \
    if (miscast_level == 0) \
        miscast_level = -1; \
    return; \
}

static bool _form_uses_xl()
{
    // No body parts that translate in any way to something fisticuffs could
    // matter to, the attack mode is different.  Plus, it's weird to have
    // users of one particular [non-]weapon be effective for this
    // unintentional form while others can just run or die.  I believe this
    // should apply to more forms, too.  [1KB]
    return you.form == TRAN_WISP;
}

/*
 **************************************************
 *             BEGIN PUBLIC FUNCTIONS             *
 **************************************************
*/
melee_attack::melee_attack(actor *attk, actor *defn,
                           int attack_num, int effective_attack_num,
                           bool is_cleaving)
    :  // Call attack's constructor
    ::attack(attk, defn),

    perceived_attack(false), obvious_effect(false), attack_number(attack_num),
    effective_attack_number(effective_attack_num),
    skip_chaos_message(false), special_damage_flavour(BEAM_NONE),
    stab_attempt(false), stab_bonus(0), cleaving(is_cleaving),
    miscast_level(-1), miscast_type(SPTYP_NONE), miscast_target(NULL),
    simu(false)
{
    attack_occurred = false;
    weapon          = attacker->weapon(attack_number);
    damage_brand    = attacker->damage_brand(attack_number);
    wpn_skill       = weapon ? weapon_skill(*weapon) : SK_UNARMED_COMBAT;
    if (_form_uses_xl())
        wpn_skill = SK_FIGHTING; // for stabbing, mostly
    to_hit          = calc_to_hit();
    can_cleave      = wpn_skill == SK_AXES && attacker != defender
                      && !attacker->confused();

    attacker_armour_tohit_penalty =
        div_rand_round(attacker->armour_tohit_penalty(true, 20), 20);
    attacker_shield_tohit_penalty =
        div_rand_round(attacker->shield_tohit_penalty(true, 20), 20);

    if (attacker->is_monster())
    {
        mon_attack_def mon_attk = mons_attack_spec(attacker->as_monster(),
                                                   attack_number);

        attk_type       = mon_attk.type;
        attk_flavour    = mon_attk.flavour;
        attk_damage     = mon_attk.damage;

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
        else if (attk_type == AT_TRUNK_SLAP && attacker->type == MONS_SKELETON)
        {
            // Elephant trunks have no bones inside.
            attk_type = AT_NONE;
        }
    }
    else
    {
        attk_type    = AT_HIT;
        attk_flavour = AF_PLAIN;
    }

    shield = attacker->shield();
    defender_shield = defender ? defender->shield() : defender_shield;

    if (weapon && weapon->base_type == OBJ_WEAPONS && is_artefact(*weapon))
    {
        artefact_wpn_properties(*weapon, art_props);
        if (is_unrandom_artefact(*weapon))
            unrand_entry = get_unrand_entry(weapon->special);
    }

    attacker_visible   = attacker->observable();
    attacker_invisible = (!attacker_visible && you.see_cell(attacker->pos()));
    defender_visible   = defender && defender->observable();
    defender_invisible = (!defender_visible && defender
                          && you.see_cell(defender->pos()));
    needs_message      = (attacker_visible || defender_visible);

    attacker_body_armour_penalty = attacker->adjusted_body_armour_penalty(20);
    attacker_shield_penalty = attacker->adjusted_shield_penalty(20);
}

static bool _conduction_affected(const coord_def &pos)
{
    const actor *act = actor_at(pos);

    // Don't check rElec to avoid leaking information about armour etc.
    return feat_is_water(grd(pos)) && act && act->ground_level();
}

bool melee_attack::can_reach()
{
    return ((attk_type == AT_HIT && weapon && weapon_reach(*weapon))
            || attk_flavour == AF_REACH
            || attk_type == AT_REACH_STING);
}

bool melee_attack::handle_phase_attempted()
{
    // Skip invalid and dummy attacks.
    if ((!adjacent(attacker->pos(), defender->pos()) && !can_reach())
        || attk_type == AT_SHOOT
        || attk_type == AT_CONSTRICT && !attacker->can_constrict(defender))
    {
        --effective_attack_number;

        return false;
    }

    if (attacker->is_player() && defender->is_monster())
    {
        if ((damage_brand == SPWPN_ELECTROCUTION
                && _conduction_affected(defender->pos()))
            || (weapon && is_unrandom_artefact(*weapon)
                && weapon->special == UNRAND_DEVASTATOR))
        {
            if (damage_brand == SPWPN_ELECTROCUTION
                && adjacent(attacker->pos(), defender->pos())
                && _conduction_affected(attacker->pos())
                && !attacker->res_elec()
                && !you.received_weapon_warning)
            {
                string prompt = "Really attack with ";
                if (weapon)
                    prompt += weapon->name(DESC_YOUR);
                else
                    prompt += "your electric unarmed attack";
                prompt += " while in water? ";

                if (yesno(prompt.c_str(), true, 'n'))
                    you.received_weapon_warning = true;
                else
                {
                    canned_msg(MSG_OK);
                    cancel_attack = true;
                    return false;
                }
            }
            else
            {
                string junk1, junk2;
                const char *verb = (bad_attack(defender->as_monster(),
                                               junk1, junk2)
                                    ? "attack" : "attack near");
                bool (*aff_func)(const coord_def &) = 0;
                if (damage_brand == SPWPN_ELECTROCUTION)
                    aff_func = _conduction_affected;

                targetter_smite hitfunc(attacker, 1, 1, 1, false, aff_func);
                hitfunc.set_aim(defender->pos());

                if (stop_attack_prompt(hitfunc, verb))
                {
                    cancel_attack = true;
                    return false;
                }
            }
        }
        else if (can_cleave)
        {
            targetter_cleave hitfunc(attacker, defender->pos());
            if (stop_attack_prompt(hitfunc, "attack"))
            {
                cancel_attack = true;
                return false;
            }
        }
        else if (stop_attack_prompt(defender->as_monster(), false,
                                    attacker->pos()))
        {
            cancel_attack = true;
            return false;
        }
        else if (cell_is_solid(defender->pos())
                 && mons_wall_shielded(defender->as_monster())
                 && you.can_see(defender)
                 && !you.confused()
                 && !crawl_state.game_is_zotdef())
        {
            // Don't waste a turn hitting a rock worm when you know it
            // will do nothing.
            mprf("The %s protects %s from harm.",
                 raw_feature_description(defender->pos()).c_str(),
                 defender->name(DESC_THE).c_str());
            cancel_attack = true;
            return false;
        }
    }
    // Set delay now that we know the attack won't be cancelled.
    if (attacker->is_player())
    {
        you.time_taken = calc_attack_delay();
        if (weapon)
        {
            if (weapon->base_type == OBJ_WEAPONS)
                if (is_unrandom_artefact(*weapon)
                    && get_unrand_entry(weapon->special)->type_name)
                {
                    count_action(CACT_MELEE, weapon->special);
                }
                else
                    count_action(CACT_MELEE, weapon->sub_type);
            else if (weapon->base_type == OBJ_RODS)
                count_action(CACT_MELEE, WPN_ROD);
            else if (weapon->base_type == OBJ_STAVES)
                count_action(CACT_MELEE, WPN_STAFF);
        }
        else
            count_action(CACT_MELEE, -1);
    }
    else
    {
        // Only the first attack costs any energy.
        if (!effective_attack_number)
        {
            int energy = attacker->as_monster()->action_energy(EUT_ATTACK);
            int delay = calc_attack_delay();
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

        if (damage_brand == SPWPN_CHAOS)
            chaos_affects_attacker();

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

    // Defending monster protects itself from attacks using the wall
    // it's in. Zotdef: allow a 5% chance of a hit anyway
    if (defender->is_monster() && cell_is_solid(defender->pos())
        && mons_wall_shielded(defender->as_monster())
        && (!crawl_state.game_is_zotdef() || !one_chance_in(20)))
    {
        string feat_name = raw_feature_description(defender->pos());

        if (attacker->is_player())
        {
            if (you.can_see(defender))
            {
                mprf("The %s protects %s from harm.",
                     feat_name.c_str(),
                     defender->name(DESC_THE).c_str());
            }
            else
                mprf("You hit the %s.", feat_name.c_str());
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

        return false;
    }

    if (attk_flavour == AF_SHADOWSTAB && defender && !defender->can_see(attacker))
    {
        if (you.see_cell(attacker->pos()))
            mprf("%s strikes at %s from the darkness!",
                 attacker->name(DESC_THE, true).c_str(),
                 defender->name(DESC_THE).c_str());
        to_hit = AUTOMATIC_HIT;
        needs_message = false;
    }

    attack_occurred = true;

    // Check for player practicing dodging
    if (one_chance_in(3) && defender->is_player())
        practise(EX_MONSTER_MAY_HIT);

    return true;
}

bool melee_attack::handle_phase_blocked()
{
    damage_done = 0;
    return true;
}

bool melee_attack::handle_phase_dodged()
{
    did_hit = false;

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
                     defender_name().c_str(),
                     attack_strength_punctuation().c_str());
            }
        }
    }

    if (attacker != defender && adjacent(defender->pos(), attacker->pos()))
    {
        if (attacker->alive()
            && (defender->is_player() ?
                   you.species == SP_MINOTAUR :
                   mons_base_type(defender->as_monster()) == MONS_MINOTAUR)
            && defender->can_see(attacker)
            // Retaliation only works on the first attack in a round.
            // FIXME: player's attack is -1, even for auxes
            && effective_attack_number <= 0)
        {
            do_minotaur_retaliation();
        }

        // Retaliations can kill!
        if (!attacker->alive())
            return false;
    }

    return true;
}

static bool _flavour_triggers_damageless(attack_flavour flavour)
{
    return (flavour == AF_CRUSH
            || flavour == AF_DROWN
            || flavour == AF_PURE_FIRE
            || flavour == AF_SHADOWSTAB
            || flavour == AF_WATERPORT);
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
        if (you_worship(GOD_BEOGH)
            && mons_genus(defender->mons_species()) == MONS_ORC
            && !defender->is_summoned()
            && !defender->as_monster()->is_shapeshifter()
            && !player_under_penance() && you.piety >= piety_breakpoint(2)
            && mons_near(defender->as_monster()) && defender->asleep())
        {
            hit_woke_orc = true;
        }
    }

    // Slimify does no damage and serves as an on-hit effect, handle it
    if (attacker->is_player() && you.duration[DUR_SLIMIFY]
        && mon_can_be_slimified(defender->as_monster())
        && !cleaving)
    {
        // Bail out after sliming so we don't get aux unarmed and
        // attack a fellow slime.
        damage_done = 0;
        slimify_monster(defender->as_monster());
        you.duration[DUR_SLIMIFY] = 0;

        return false;
    }

    // This does more than just calculate the damage, it also sets up
    // messages, etc.
    damage_done = calc_damage();

    if (attacker->is_player() && you.duration[DUR_INFUSION])
    {
        if (you.magic_points > 0
            || you.species == SP_DJINNI && ((you.hp - 1)/DJ_MP_RATE) > 0)
        {
            // infusion_power is set when the infusion spell is cast
            const int pow = you.props["infusion_power"].get_int();
            const int dmg = 2 + div_rand_round(pow, 25);
            const int hurt = defender->apply_ac(dmg);

            dprf(DIAG_COMBAT, "Infusion: dmg = %d hurt = %d", dmg, hurt);

            if (hurt > 0)
            {
                damage_done += hurt;
                dec_mp(1);
            }
        }
    }

    bool stop_hit = false;
    // Check if some hit-effect killed the monster.
    if (attacker->is_player())
        stop_hit = !player_monattk_hit_effects();

    // check_unrand_effects is safe to call with a dead defender, so always
    // call it, even if the hit effects said to stop.
    if (check_unrand_effects() || stop_hit)
        return false;

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
             defender->name(DESC_THE).c_str(),
             attacker->is_player() ? "do" : "does");
    }

    // Check for weapon brand & inflict that damage too
    apply_damage_brand();

    if (attacker->is_player())
    {
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
        // parameters of the effec
        do_passive_freeze();
        do_passive_heat();
        emit_foul_stench();
    }

    return true;
}

bool melee_attack::handle_phase_damaged()
{
    bool shroud_broken = false;

    // TODO: Move this somewhere else, this is a terrible place for a
    // block-like (prevents all damage) effect.
    if (attacker != defender &&
        defender->is_player() &&
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

    // We have to check in_bounds() because removed kraken tentacles are
    // temporarily returned to existence (without a position) when they
    // react to damage.
    if (defender->can_bleed()
        && !defender->is_summoned()
        && !defender->submerged()
        && in_bounds(defender->pos())
        && !simu)
    {
        int blood = modify_blood_amount(damage_done, attacker->damage_type());
        if (blood > defender->stat_hp())
            blood = defender->stat_hp();
        if (blood)
            (new blood_fineff(defender, defender->pos(), blood))->schedule();
    }

    announce_hit();
    // Inflict stored damage
    damage_done = inflict_damage(damage_done);

    // TODO: Unify these, added here so we can get rid of player_attack
    if (attacker->is_player())
    {
        if (damage_done)
            player_exercise_combat_skills();

        // ugh, inspecting attack_verb here is pretty ugly
        if (damage_done && attack_verb == "trample")
            do_knockback();

        if (defender->alive())
        {
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
        // Monsters attacking themselves don't get attack flavour.
        // The message sequences look too weird.  Also, stealing
        // attacks aren't handled until after the damage msg. Also,
        // no attack flavours for dead defenders
        if (attacker != defender && defender->alive())
        {
            mons_apply_attack_flavour();
            apply_staff_damage();

            if (needs_message && !special_damage_message.empty())
                mprf("%s", special_damage_message.c_str());

            if (special_damage > 0)
                inflict_damage(special_damage, special_damage_flavour);
        }

        const bool chaos_attack = damage_brand == SPWPN_CHAOS
                                  || (attk_flavour == AF_CHAOS
                                      && attacker != defender);

        if (defender->is_player())
            practise(EX_MONSTER_WILL_HIT);

        // decapitate_hydra() returns true if the wound was cauterized or the
        // last head was removed.  In the former case, we shouldn't apply
        // the brand damage (so we return here).  If the monster was killed
        // by the decapitation, we should stop the rest of the attack, too.
        if (decapitate_hydra(damage_done, attacker->damage_type(attack_number)))
            return defender->alive();

        special_damage = 0;
        special_damage_message.clear();
        special_damage_flavour = BEAM_NONE;

        if (attacker != defender && attk_type == AT_TRAMPLE)
            do_knockback();

        // Defender banished.  Bail since the defender is still alive in the
        // Abyss.
        if (defender->is_banished())
        {
            if (chaos_attack && attacker->alive())
                chaos_affects_attacker();

            do_miscast();
            return false;
        }

        if (!defender->alive())
        {
            if (chaos_attack && attacker->alive())
                chaos_affects_attacker();

            do_miscast();
            return true;
        }

        // Yredelemnul's injury mirroring can kill the attacker.
        // Also, bail if the monster is attacking itself without a
        // weapon, since intrinsic monster attack flavours aren't
        // applied for self-attacks.
        if (!attacker->alive() || (attacker == defender && !weapon))
        {
            if (miscast_target == defender)
                do_miscast();
            return false;
        }

        if (!defender->alive())
        {
            if (chaos_attack && attacker->alive())
                chaos_affects_attacker();

            do_miscast();
            return true;
        }

        if (chaos_attack && attacker->alive())
            chaos_affects_attacker();

        if (miscast_target == defender)
            do_miscast();

        // Yredelemnul's injury mirroring can kill the attacker.
        if (!attacker->alive())
            return false;

        if (miscast_target == attacker)
            do_miscast();

        // Miscast might have killed the attacker.
        if (!attacker->alive())
            return false;
    }

    if (shroud_broken)
        mpr("Your shroud falls apart!", MSGCH_WARN);

    if (defender->is_player() && you.mutation[MUT_JELLY_GROWTH]
        && x_chance_in_y(damage_done, you.hp_max))
    {
        mgen_data mg(MONS_JELLY, BEH_STRICT_NEUTRAL, 0, 0, 0,
                     you.pos(), MHITNOT, 0, you.religion);

        if (create_monster(mg))
        {
            mpr("Your attached jelly is knocked off by the blow!");
            you.mutation[MUT_JELLY_GROWTH] = 0;
        }
    }

    return true;
}

bool melee_attack::handle_phase_killed()
{
    // Wyrmbane needs to be notified of deaths, including ones due to its
    // Dragon slaying brand, but other users of melee_effects() don't want
    // to possibly be called twice. Adding another entry for a single
    // artefact would be overkill, so here we call it by hand:
    if (unrand_entry && weapon && weapon->special == UNRAND_WYRMBANE)
    {
        unrand_entry->fight_func.melee_effects(weapon, attacker, defender,
                                               true, special_damage);
    }

    monster * const mon = defender->as_monster();
    if (!invalid_monster(mon))
        monster_die(mon, attacker);

    return true;
}

bool melee_attack::handle_phase_aux()
{
    if (attacker->is_player())
    {
        // returns whether an aux attack successfully took place
        if (!defender->as_monster()->friendly()
            && adjacent(defender->pos(), attacker->pos())
            && !cleaving) // additional attacks from cleave don't get aux
        {
            player_aux_unarmed();
        }

        print_wounds(defender->as_monster());
    }

    return true;
}

bool melee_attack::handle_phase_end()
{
    if (!cleave_targets.empty())
    {
        attack_cleave_targets(attacker, cleave_targets, attack_number,
                              effective_attack_number);
    }

    // Check for passive mutation effects.
    if (defender->is_player() && defender->alive() && attacker != defender)
    {
        mons_do_eyeball_confusion();
        tendril_disarm();
    }

    // This may invalidate both the attacker and defender.
    fire_final_effects();

    return true;
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
    if (!cleaving && !handle_phase_attempted())
        return false;

    if (attacker != defender && attacker->self_destructs())
        return (did_hit = perceived_attack = true);

    if (can_cleave && !cleaving)
        cleave_setup();

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
    const int ev = defender->melee_evasion(attacker);
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
        if (attacker != defender && adjacent(defender->pos(), attacker->pos()))
        {
            // Check for defender Spines
            do_spines();

            // Spines can kill!
            if (!attacker->alive())
                return false;
        }

        if (ev_margin >= 0)
        {
            if (attacker != defender && attack_warded_off())
            {
                perceived_attack = true;
                handle_phase_end();
                return false;
            }

            bool cont = handle_phase_hit();

            attacker_sustain_passive_damage();

            if (!cont)
            {
                handle_phase_end();
                return false;
            }
        }
        else
            handle_phase_dodged();
    }

    // Remove sanctuary if - through some attack - it was violated.
    if (env.sanctuary_time > 0 && attack_occurred && !cancel_attack
        && attacker != defender && !attacker->confused()
        && (is_sanctuary(attacker->pos()) || is_sanctuary(defender->pos()))
        && (attacker->is_player() || attacker->as_monster()->friendly()))
    {
        remove_sanctuary(true);
    }

    if (attacker->is_player())
    {
        if (damage_brand == SPWPN_CHAOS)
            chaos_affects_attacker();

        do_miscast();
    }
    else
    {
        // Invisible monster might have interrupted butchering.
        if (you_are_delayed() && defender->is_player()
            && perceived_attack && !attacker_visible)
        {
            handle_interrupted_swap(false, true);
        }
    }

    adjust_noise();
    // don't crash on banishment
    if (!defender->pos().origin())
        handle_noise(defender->pos());

    // Allow monster attacks to draw the ire of the defender.  Player
    // attacks are handled elsewhere.
    if (perceived_attack
        && defender->is_monster()
        && attacker->is_monster()
        && attacker->alive() && defender->alive()
        && (defender->as_monster()->foe == MHITNOT || one_chance_in(3)))
    {
        behaviour_event(defender->as_monster(), ME_WHACK, attacker);
    }

    // If an enemy attacked a friend, set the pet target if it isn't set
    // already, but not if sanctuary is in effect (pet target must be
    // set explicitly by the player during sanctuary).
    if (perceived_attack && attacker->alive()
        && (defender->is_player() || defender->as_monster()->friendly())
        && !attacker->is_player()
        && !crawl_state.game_is_arena()
        && !attacker->as_monster()->wont_attack()
        && you.pet_target == MHITNOT
        && env.sanctuary_time <= 0)
    {
        if (defender->is_player())
        {
            interrupt_activity(AI_MONSTER_ATTACKS, attacker->as_monster());

            // Try to switch to a melee weapon in a/b slot if we don't have one
            // wielded and end the turn.
            if (Options.auto_switch && !wielded_weapon_check(weapon, true))
                for (int i = 0; i <= 1; ++i)
                    if (is_melee_weapon(you.inv[i]) && wield_weapon(true, i))
                        return false;
        }

        you.pet_target = attacker->mindex();
    }

    if (!defender->alive())
        handle_phase_killed();

    handle_phase_aux();

    handle_phase_end();

    enable_attack_conducts(conducts);

    return attack_occurred;
}

/* Initialises the noise_factor
 *
 * For both players and monsters, separately sets the noise_factor for weapon
 * attacks based on damage type or attack_type/attack_flavour respectively.
 * Sets an average noise value for unarmed combat regardless of atype().
 */
// TODO: Unify attack_type and unarmed_attack_type, the former includes the latter
// however formerly, attack_type was mons_attack_type and used exclusively for monster use.
void melee_attack::adjust_noise()
{
    if (attacker->is_player() && weapon != NULL)
    {
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
    else if (attacker->is_monster() && weapon == NULL)
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
        case AT_POUNCE:
            noise_factor = 100;
            break;

        case AT_STING:
        case AT_SPORE:
        case AT_ENGULF:
        case AT_REACH_STING:
            noise_factor = 75;
            break;

        case AT_TOUCH:
            noise_factor = 0;
            break;

        case AT_WEAP_ONLY:
            break;

        // To prevent compiler warnings.
        case AT_NONE:
        case AT_RANDOM:
        case AT_SHOOT:
            die("Invalid attack type for noise_factor");
            break;

        default:
            die("%d Unhandled attack type for noise_factor", attk_type);
            break;
        }

        switch (attk_flavour)
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
        noise_factor = 150;
}

void melee_attack::check_autoberserk()
{
    if (attacker->suppressed())
        return;

    if (attacker->is_player())
    {
        for (int i = EQ_WEAPON; i < NUM_EQUIP; ++i)
        {
            const item_def *item = you.slot_item(static_cast<equipment_type>(i));
            if (!item)
                continue;

            if (!is_artefact(*item))
                continue;

            if (x_chance_in_y(artefact_wpn_property(*item, ARTP_ANGRY), 100))
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

            if (x_chance_in_y(artefact_wpn_property(*item, ARTP_ANGRY), 100))
            {
                attacker->go_berserk(false);
                return;
            }
        }
    }
}

bool melee_attack::check_unrand_effects()
{
    if (attacker->suppressed())
        return false;

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

    return false;
}

/* Setup all unarmed (non attack_type) variables
 *
 * Clears any previous unarmed attack information and sets everything from
 * noise_factor to verb and damage. Called after player_aux_choose_uc_attack
 */
void melee_attack::player_aux_setup(unarmed_attack_type atk)
{
    noise_factor = 100;
    aux_attack.clear();
    aux_verb.clear();
    damage_brand = SPWPN_NORMAL;
    aux_damage = 0;

    switch (atk)
    {
    case UNAT_CONSTRICT:
        aux_attack = aux_verb = "grab";
        aux_damage = 0;
        noise_factor = 10; // extremely quiet?
        break;

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

            // Max talon damage: 9.
            aux_damage += 1 + player_mutation_level(MUT_TALONS);
        }
        else if (player_mutation_level(MUT_TENTACLE_SPIKE))
        {
            aux_attack = "tentacle spike";
            aux_verb = "pierce";

            // Max spike damage: 8.
            aux_damage += player_mutation_level(MUT_TENTACLE_SPIKE);
        }

        break;

    case UNAT_PECK:
        aux_damage = 6;
        noise_factor = 75;
        aux_attack = aux_verb = "peck";
        break;

    case UNAT_HEADBUTT:
        aux_damage = 5 + player_mutation_level(MUT_HORNS) * 3;
        aux_attack = aux_verb = "headbutt";
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
        aux_damage = 5 + you.skill_rdiv(SK_UNARMED_COMBAT, 1, 2);

        if (you.form == TRAN_BLADE_HANDS)
        {
            aux_verb = "slash";
            aux_damage += 6;
            noise_factor = 75;
        }
        else if (you.has_usable_claws())
        {
            aux_verb = "claw";
            aux_damage += roll_dice(you.has_claws(), 3);
        }
        else if (you.has_usable_tentacles())
        {
            aux_attack = aux_verb = "tentacle-slap";
            noise_factor = 125;
        }

        break;

    case UNAT_BITE:
        aux_attack = aux_verb = "bite";
        aux_damage += you.has_usable_fangs() * 2;
        aux_damage += div_rand_round(max(you.strength()-10, 0), 5);
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

        // Tentacles give you both a main attack (replacing punch)
        // and this secondary, high damage attack.
    case UNAT_TENTACLES:
        aux_attack = aux_verb = "squeeze";
        aux_damage += you.has_usable_tentacles() ? 12 : 0;
        noise_factor = 100; // quieter than slapping
        break;

    default:
        die("unknown aux attack type");
        break;
    }
}

/* Selects the unarmed attack type given by Unarmed Combat skill
 *
 * Currently the only possibility is an offhand punch. Other auxes are linked
 * directly to mutations and just depend on stats.
 */
unarmed_attack_type melee_attack::player_aux_choose_uc_attack()
{
    unarmed_attack_type uc_attack = coinflip() ? UNAT_PUNCH : UNAT_NO_ATTACK;
    // Octopodes get more tentacle-slaps.
    if (you.species == SP_OCTOPODE && coinflip())
        uc_attack = UNAT_PUNCH;
    // No punching with a shield or 2-handed wpn.
    // Octopodes aren't affected by this, though!
    if (you.species != SP_OCTOPODE && uc_attack == UNAT_PUNCH
        && !you.has_usable_offhand())
    {
        uc_attack = UNAT_NO_ATTACK;
    }

    if (_tran_forbid_aux_attack(uc_attack))
        uc_attack = UNAT_NO_ATTACK;

    return uc_attack;
}

bool melee_attack::player_aux_test_hit()
{
    // XXX We're clobbering did_hit
    did_hit = false;

    const int evasion = defender->melee_evasion(attacker);

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

    const int phaseless_evasion =
        defender->melee_evasion(attacker, EV_IGNORE_PHASESHIFT);

    if (to_hit >= phaseless_evasion && defender_visible)
    {
        mprf("Your %s passes through %s as %s momentarily phases out.",
            aux_attack.c_str(),
            defender->name(DESC_THE).c_str(),
            defender->pronoun(PRONOUN_SUBJECTIVE).c_str());
    }
    else
    {
        mprf("Your %s misses %s.", aux_attack.c_str(),
             defender->name(DESC_THE).c_str());
    }

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

    /*
     * uc_attack is the auxiliary unarmed attack the player gets
     * for unarmed combat skill, which doesn't require a mutation to use
     * but still needs to pass the other checks in _extra_aux_attack().
     */
    unarmed_attack_type uc_attack = UNAT_NO_ATTACK;
    // Unarmed skill gives a chance at getting an aux even without the
    // corresponding mutation.
    if (attacker->fights_well_unarmed(attacker_armour_tohit_penalty
                                   + attacker_shield_tohit_penalty))
    {
        uc_attack = player_aux_choose_uc_attack();
    }

    for (int i = UNAT_FIRST_ATTACK; i <= UNAT_LAST_ATTACK; ++i)
    {
        if (!defender->alive())
            break;

        unarmed_attack_type atk = static_cast<unarmed_attack_type>(i);

        if (!_extra_aux_attack(atk, (uc_attack == atk)))
            continue;

        // Determine and set damage and attack words.
        player_aux_setup(atk);

        if (atk == UNAT_CONSTRICT && !attacker->can_constrict(defender))
            continue;

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

    aux_damage  = player_aux_stat_modify_damage(aux_damage);

    aux_damage  = random2(aux_damage);

    aux_damage  = player_apply_fighting_skill(aux_damage, true);

    aux_damage  = player_apply_misc_modifiers(aux_damage);

    aux_damage  = player_apply_slaying_bonuses(aux_damage, true);

    aux_damage  = player_apply_final_multipliers(aux_damage);

    const int pre_ac_dmg = aux_damage;
    const int post_ac_dmg = apply_defender_ac(aux_damage);

    if (atk == UNAT_CONSTRICT)
        aux_damage = 0;
    else
        aux_damage = post_ac_dmg;

    aux_damage = inflict_damage(aux_damage, BEAM_MISSILE);
    damage_done = aux_damage;

    switch (atk)
    {
        case UNAT_PUNCH:
            apply_bleeding = true;
            break;

        case UNAT_HEADBUTT:
        {
            const int horns = player_mutation_level(MUT_HORNS);
            const int stun = bestroll(min(damage_done, 7), 1 + horns);

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

        case UNAT_CONSTRICT:
            attacker->start_constricting(*defender);
            break;

        default:
            break;
    }

    if (damage_done > 0 || atk == UNAT_CONSTRICT)
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
         attack_strength_punctuation().c_str());
}

string melee_attack::player_why_missed()
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
        const string armour_name = armour ? armour->name(DESC_BASENAME)
                                          : string("armour");

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
        behaviour_event(defender->as_monster(), ME_WHACK, attacker);

    mprf("%s%s.",
         player_why_missed().c_str(),
         defender->name(DESC_THE).c_str());
}

int melee_attack::player_stat_modify_damage(int damage)
{
    int dammod = 39;
    const int dam_stat_val = calc_stat_to_dam_base();

    if (dam_stat_val > 11)
        dammod += (random2(dam_stat_val - 11) * 2);
    else if (dam_stat_val < 9)
        dammod -= (random2(9 - dam_stat_val) * 3);

    damage *= dammod;
    damage /= 39;

    return damage;
}

int melee_attack::player_aux_stat_modify_damage(int damage)
{
    int dammod = 20;
    // Use the same str/dex weighting that unarmed combat does, for now.
    const int dam_stat_val = (7 * you.strength() + 3 * you.dex())/10;

    if (dam_stat_val > 10)
        dammod += random2(dam_stat_val - 9);
    else if (dam_stat_val < 10)
        dammod -= random2(11 - dam_stat_val);

    damage *= dammod;
    damage /= 20;

    return damage;
}

int melee_attack::player_apply_weapon_skill(int damage)
{
    if (weapon && is_weapon(*weapon) && !is_range_weapon(*weapon))
    {
        damage *= 2500 + (random2(you.skill(wpn_skill, 100) + 1));
        damage /= 2500;
    }

    return damage;
}

int melee_attack::player_apply_fighting_skill(int damage, bool aux)
{
    const int base = aux? 40 : 30;

    damage *= base * 100 + (random2(you.skill(SK_FIGHTING, 100) + 1));
    damage /= base * 100;

    return damage;
}

// A couple additive modifiers that should be applied to both unarmed and
// armed attacks.
int melee_attack::player_apply_misc_modifiers(int damage)
{
    if (you.duration[DUR_MIGHT] || you.duration[DUR_BERSERK])
        damage += 1 + random2(10);

    if (you.species != SP_VAMPIRE && you.hunger_state == HS_STARVING)
        damage -= random2(5);

    return damage;
}

// Slaying and weapon enchantment. Apply this for slaying even if not
// using a weapon to attack.
int melee_attack::player_apply_slaying_bonuses(int damage, bool aux)
{
    int damage_plus = 0;
    if (!aux && weapon && is_weapon(*weapon) && !is_range_weapon(*weapon))
    {
        damage_plus = weapon->plus2;

        if (weapon->base_type == OBJ_RODS)
            damage_plus = weapon->special;
    }
    damage_plus += slaying_bonus(PWPN_DAMAGE);

    damage += (damage_plus > -1) ? (random2(1 + damage_plus))
                                 : (-random2(1 - damage_plus));
    return damage;
}

// Modifiers dependent on wielding a weapon. Does not include weapon
// enchantment, which is handled by player_apply_slaying_bonuses.
int melee_attack::player_apply_weapon_bonuses(int damage)
{
    if (weapon && is_weapon(*weapon) && !is_range_weapon(*weapon))
    {
        if (get_equip_race(*weapon) == ISFLAG_DWARVEN
            && you.species == SP_DEEP_DWARF)
        {
            damage += random2(3);
        }

        if (get_equip_race(*weapon) == ISFLAG_ORCISH
            && player_genus(GENPC_ORCISH))
        {
            if (you_worship(GOD_BEOGH) && !player_under_penance())
            {
#ifdef DEBUG_DIAGNOSTICS
                const int orig_damage = damage;
#endif

                if (you.piety > 80 || coinflip())
                    damage++;

                damage +=
                    random2avg(
                        div_rand_round(
                            min(static_cast<int>(you.piety), 180), 33), 2);

                dprf(DIAG_COMBAT, "Damage: %d -> %d, Beogh bonus: %d",
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

    if (you.duration[DUR_WEAK])
        damage = div_rand_round(damage * 3, 4);

    return damage;
}

int melee_attack::player_stab_weapon_bonus(int damage)
{
    int stab_skill = you.skill(wpn_skill, 50) + you.skill(SK_STEALTH, 50);

    if (weapon && weapon->base_type == OBJ_WEAPONS
        && (weapon->sub_type == WPN_CLUB
            || weapon->sub_type == WPN_SPEAR
            || weapon->sub_type == WPN_TRIDENT
            || weapon->sub_type == WPN_DEMON_TRIDENT
            || weapon->sub_type == WPN_TRISHULA)
        || !weapon && you.species == SP_FELID)
    {
        goto ok_weaps;
    }

    switch (wpn_skill)
    {
    case SK_SHORT_BLADES:
    {
        int bonus = (you.dex() * (stab_skill + 100)) / 500;

        if (weapon->sub_type != WPN_DAGGER)
            bonus /= 2;

        bonus   = stepdown_value(bonus, 10, 10, 30, 30);
        damage += bonus;
    }
    // fall through
    ok_weaps:
    case SK_LONG_BLADES:
        damage *= 10 + div_rand_round(stab_skill, 100 *
                       (stab_bonus + (wpn_skill == SK_SHORT_BLADES ? 0 : 2)));
        damage /= 10;
        // fall through
    default:
        damage *= 12 + div_rand_round(stab_skill, 100 * stab_bonus);
        damage /= 12;
        break;
    }

    return damage;
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
        damage = max(1, damage);

        damage = player_stab_weapon_bonus(damage);
    }

    return damage;
}

void melee_attack::set_attack_verb()
{
    int weap_type = WPN_UNKNOWN;

    if (!weapon)
        weap_type = WPN_UNARMED;
    else if (weapon->base_type == OBJ_STAVES)
        weap_type = WPN_STAFF;
    else if (weapon->base_type == OBJ_RODS)
        weap_type = WPN_ROD;
    else if (weapon->base_type == OBJ_WEAPONS
             && !is_range_weapon(*weapon))
    {
        weap_type = weapon->sub_type;
    }

    // All weak hits look the same, except for when the attacker
    // has a non-weapon in hand. - bwr
    // Exception: vampire bats only bite to allow for drawing blood.
    if (damage_done < HIT_WEAK
        && (you.species != SP_VAMPIRE || you.form != TRAN_BAT)
        && you.species != SP_FELID)
    {
        if (weap_type != WPN_UNKNOWN)
            attack_verb = "hit";
        else
            attack_verb = "clumsily bash";
        return;
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
            if (defender->is_monster()
                && defender_visible
                && defender_genus == MONS_HOG)
            {
                attack_verb = "spit";
                verb_degree = "like the proverbial pig";
            }
            else
            {
                const char* pierce_desc[][2] = {{"spit", "like a pig"},
                                                {"skewer", "like a kebab"},
                                                {"stick", "like a pincushion"},
                                                {"perforate", "like a sieve"}};
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
        else if (defender_genus == MONS_SKELETON)
        {
            attack_verb = "fracture";
            verb_degree = "into splinters";
        }
        else if (defender_genus == MONS_HOG)
        {
            attack_verb = "carve";
            verb_degree = "like a proverbial ham";
        }
        else
        {
            const char* pierce_desc[][2] = {{"open",    "like a pillowcase"},
                                            {"slice",   "like a ripe choko"},
                                            {"cut",     "into ribbons"},
                                            {"carve",   "like a ham"},
                                            {"chop",    "into pieces"}};
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
        else if (defender_genus == MONS_SKELETON)
        {
            attack_verb = "shatter";
            verb_degree = "into splinters";
        }
        else
        {

            const char* pierce_desc[][2] = {{"crush",   "like a grape"},
                                            {"beat",    "like a drum"},
                                            {"hammer",  "like a gong"},
                                            {"pound",   "like an anvil"},
                                            {"flatten", "like a pancake"}};
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
            switch (defender->holiness())
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
        switch (you.form)
        {
        case TRAN_SPIDER:
        case TRAN_BAT:
        case TRAN_PIG:
        case TRAN_PORCUPINE:
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
        case TRAN_TREE:
            if (damage_done < HIT_WEAK)
                attack_verb = "hit";
            else if (damage_done < HIT_MED)
                attack_verb = "smack";
            else if (damage_done < HIT_STRONG)
                attack_verb = "pummel";
            else
                attack_verb = "thrash";
            break;
        case TRAN_DRAGON:
            if (damage_done < HIT_WEAK)
                attack_verb = "hit";
            else if (damage_done < HIT_MED)
                attack_verb = "claw";
            else if (damage_done < HIT_STRONG)
                attack_verb = "bite";
            else
                attack_verb = coinflip() ? "maul" : "trample";
            break;
        case TRAN_WISP:
            if (damage_done < HIT_WEAK)
                attack_verb = "touch";
            else if (damage_done < HIT_MED)
                attack_verb = "hit";
            else
                attack_verb = "engulf";
            break;
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
        case TRAN_NONE:
        case TRAN_APPENDAGE:
        case TRAN_FUNGUS:
        case TRAN_ICE_BEAST:
        case TRAN_JELLY: // ?
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
                if (damage_done < HIT_WEAK)
                    attack_verb = "hit";
                else if (damage_done < HIT_MED)
                    attack_verb = "punch";
                else if (damage_done < HIT_STRONG)
                    attack_verb = "pummel";
                // XXX: detect this better
                else if (defender->is_monster()
                         && (get_mon_shape(defender->as_monster()->type)
                               == MON_SHAPE_INSECT
                             || get_mon_shape(defender->as_monster()->type)
                                == MON_SHAPE_INSECT_WINGED
                             || get_mon_shape(defender->as_monster()->type)
                                == MON_SHAPE_CENTIPEDE
                             || get_mon_shape(defender->as_monster()->type)
                                == MON_SHAPE_ARACHNID))
                {
                    attack_verb = "squash";
                    verb_degree = "like a proverbial bug";
                }
                else
                {
                    const char* punch_desc[][2] =
                        {{"pound",     "into fine dust"},
                         {"pummel",    "like a punching bag"},
                         {"pulverise", ""},
                         {"squash",    "like a bug"}};
                    const int choice = random2(ARRAYSZ(punch_desc));
                    // XXX: could this distinction work better?
                    if (choice == 0
                        && defender->is_monster()
                        && mons_has_blood(defender->as_monster()->type))
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
        } // transformations
        break;

    case WPN_UNKNOWN:
    default:
        attack_verb = "hit";
        break;
    }
}

void melee_attack::player_exercise_combat_skills()
{
    if (defender->cannot_fight())
        return;

    int damage = 10; // Default for unarmed.
    if (weapon && is_weapon(*weapon) && !is_range_weapon(*weapon))
        damage = property(*weapon, PWPN_DAMAGE);

    // Slow down the practice of low-damage weapons.
    if (x_chance_in_y(damage, 20))
        practise(EX_WILL_HIT, wpn_skill);
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
        return false;

    // These effects apply only to monsters that are still alive:

    // Returns true if a head was cut off *and* the wound was cauterized,
    // in which case the cauterization was the ego effect, so don't burn
    // the hydra some more.
    //
    // Also returns true if the hydra's last head was cut off, in which
    // case nothing more should be done to the hydra.
    if (decapitate_hydra(damage_done))
        return defender->alive();

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
    }

    special_damage = inflict_damage(special_damage);

    if (stab_attempt && stab_bonus > 0 && weapon
        && weapon->base_type == OBJ_WEAPONS && weapon->sub_type == WPN_CLUB
        && damage_done + special_damage > random2(defender->get_experience_level())
        && defender->alive()
        && !defender->as_monster()->has_ench(ENCH_CONFUSION)
        && mons_class_is_confusable(defender->type))
    {
        if (!defender->as_monster()->check_clarity(false) &&
            defender->as_monster()->add_ench(mon_enchant(ENCH_CONFUSION, 0,
            &you, 20+random2(30)))) // 1-3 turns
        {
            mprf("%s is stunned!", defender->name(DESC_THE).c_str());
        }
    }

    return true;
}

int melee_attack::fire_res_apply_cerebov_downgrade(int res)
{
    if (weapon && weapon->special == UNRAND_CEREBOV)
        --res;

    return res;
}

void melee_attack::drain_defender()
{
    if (defender->is_monster() && one_chance_in(3))
        return;

    if (defender->holiness() != MH_NATURAL)
        return;

    special_damage = resist_adjust_damage(defender, BEAM_NEG,
                                          defender->res_negative_energy(),
                                          (1 + random2(damage_done)) / 2);

    if (defender->drain_exp(attacker, true, 20 + min(35, damage_done)))
    {
        if (defender->is_player())
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
        // XXX: why is this message separate here?
        if (defender->is_player())
        {
            special_damage_message =
                make_stringf("You feel your flesh %s away!",
                             immediate > 0 ? "rotting" : "start to rot");
        }
        else if (defender->is_monster() && defender_visible)
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
    if (defender->is_monster()
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
        return false;
    }

    if (one_chance_in(3))
    {
        special_damage_message =
            make_stringf("Space bends around %s.",
            def_name(DESC_THE).c_str());
        special_damage += 1 + random2avg(7, 2);
        return false;
    }

    if (one_chance_in(3))
    {
        special_damage_message =
            make_stringf("Space warps horribly around %s!",
                         def_name(DESC_THE).c_str());
        special_damage += 3 + random2avg(24, 2);
        return false;
    }

    if (simu)
        return false;

    if (one_chance_in(3))
    {
        if (defender_visible)
            obvious_effect = true;
        if (!defender->no_tele(true, false))
            (new blink_fineff(defender))->schedule();
        return false;
    }

    // Used to be coinflip() || coinflip() for players, just coinflip()
    // for monsters; this is a compromise. Note that it makes banishment
    // a touch more likely for players, and a shade less likely for
    // monsters.
    if (!one_chance_in(3))
    {
        if (defender_visible)
            obvious_effect = true;
        if (crawl_state.game_is_sprint() && defender->is_player()
            || defender->no_tele())
        {
            if (defender->is_player())
                canned_msg(MSG_STRANGE_STASIS);
        }
        else if (coinflip())
            (new distortion_tele_fineff(defender))->schedule();
        else
            defender->teleport();
        return false;
    }

    if (!player_in_branch(BRANCH_ABYSS) && coinflip())
    {
        if (defender->is_player() && attacker_visible
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

        defender->banish(attacker, attacker->name(DESC_PLAIN, true));
        return true;
    }

    return false;
}

void melee_attack::antimagic_affects_defender()
{
    if (defender->is_player())
    {
        int mp_loss = min(you.magic_points, random2(damage_done * 2));
        if (!mp_loss)
            return;
        mpr("You feel your power leaking away.", MSGCH_WARN);
        drain_mp(mp_loss);
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

    if (x_chance_in_y(attacker->skill_rdiv(SK_NECROMANCY) + 1, 8))
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
    const bool mon        = defender->is_monster();
    const bool immune     = mon && mons_immune_magic(defender->as_monster());
    const bool is_natural = mon && defender->holiness() == MH_NATURAL;
    const bool is_shifter = mon && defender->as_monster()->is_shapeshifter();
    const bool can_clone  = mon && mons_clonable(defender->as_monster(), true);
    const bool can_poly   = is_shifter || (defender->can_safely_mutate()
                                           && !immune);
    const bool can_rage   = defender->can_go_berserk();
    const bool can_slow   = !mon || !mons_is_firewood(defender->as_monster());
    const bool can_petrify= mon && !defender->res_petrify();

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
            if (defender->is_player())
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
    string chaos_effect = "CHAOS effect: ";
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
        ASSERT(can_clone);
        ASSERT(clone_chance > 0);
        ASSERT(defender->is_monster());

        if (monster *clone = clone_mons(defender->as_monster(), true,
                                        &obvious_effect))
        {
            if (obvious_effect)
            {
                special_damage_message =
                    make_stringf("%s is duplicated!",
                                 def_name(DESC_THE).c_str());
            }

            // The player shouldn't get new permanent followers from cloning.
            if (clone->attitude == ATT_FRIENDLY && !clone->is_summoned())
                clone->mark_summoned(6, true, MON_SUMM_CLONE);

            // Monsters being cloned is interesting.
            xom_is_stimulated(clone->friendly() ? 12 : 25);
        }
        break;
    }

    case CHAOS_POLY:
        ASSERT(can_poly);
        ASSERT(poly_chance > 0);
        beam.flavour = BEAM_POLYMORPH;
        break;

    case CHAOS_POLY_UP:
        ASSERT(can_poly);
        ASSERT(poly_up_chance > 0);
        ASSERT(defender->is_monster());

        obvious_effect = you.can_see(defender);
        monster_polymorph(defender->as_monster(), RANDOM_MONSTER, PPT_MORE);
        break;

    case CHAOS_MAKE_SHIFTER:
    {
        ASSERT(can_poly);
        ASSERT(shifter_chance > 0);
        ASSERT(!is_shifter);
        ASSERT(defender->is_monster());

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
        int level1_chance = max(0, level - 7);
        int level2_chance = max(0, level - 12);
        int level3_chance = max(0, level - 17);

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
        ASSERT(can_rage);
        ASSERT(rage_chance > 0);
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
            (attacker->is_player())           ? KILL_YOU
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
        string msg = "";
        if (!you.can_see(attacker))
        {
            string noise = getSpeakString("weapon_noise");
            if (!noise.empty())
                msg = "You hear " + maybe_pick_random_substring(noise);
        }
        else
        {
            msg = getSpeakString("weapon_noises");
            string wepname = wep_name(DESC_YOUR);
            if (!msg.empty())
            {
                msg = maybe_pick_random_substring(msg);
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

    if (attacker->is_player() && !you.form && one_chance_in(500))
    {
        // Non-weapon using forms are uncool here: you'd need to run away
        // instead of continuing the fight.
        transformation_type form = coinflip() ? TRAN_TREE : TRAN_APPENDAGE;
        // Waiting it off is boring.
        if (form == TRAN_TREE && !there_are_monsters_nearby(true, false, false))
            form = TRAN_APPENDAGE;
        if (one_chance_in(5))
            form = coinflip() ? TRAN_STATUE : TRAN_LICH;
        if (transform(0, form))
        {
#ifdef NOTE_DEBUG_CHAOS_EFFECTS
            take_note(Note(NOTE_MESSAGE, 0, 0,
                           (string("CHAOS affects attacker: transform into ")
                           + transform_name(form)).c_str()), true);
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
    ASSERT_RANGE(miscast_level, 0, 4);
    ASSERT(count_bits(miscast_type) == 1);

    if (!miscast_target->alive())
        return;

    if (miscast_target->is_player() && you.banished)
        return;

    const bool chaos_brand =
        weapon && get_weapon_brand(*weapon) == SPWPN_CHAOS;

    // If the miscast is happening on the attacker's side and is due to
    // a chaos weapon then make smoke/sand/etc pour out of the weapon
    // instead of the attacker's hands.
    string hand_str;

    string cause = atk_name(DESC_THE);
    int    source;

    const int ignore_mask = ISFLAG_KNOW_CURSE | ISFLAG_KNOW_PLUSES;

    if (attacker->is_player())
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
            {
                susceptible = false;
            }
            break;
        default:
            break;
        }

        if (susceptible)
            break;
    }
#ifdef NOTE_DEBUG_CHAOS_BRAND
    string brand_name = "CHAOS brand: ";
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
    return brand;
}

attack_flavour melee_attack::random_chaos_attack_flavour()
{
    attack_flavour flavours[] =
        {AF_FIRE, AF_COLD, AF_ELEC, AF_POISON_NASTY, AF_VAMPIRIC, AF_DISTORT,
         AF_CONFUSE, AF_CHAOS};
    return RANDOM_ELEMENT(flavours);
}

bool melee_attack::apply_damage_brand()
{
    bool brand_was_known = false;
    int brand, res = 0;
    bool ret = false;

    if (weapon)
    {
        if (is_artefact(*weapon))
            brand_was_known = artefact_known_wpn_property(*weapon, ARTP_BRAND);
        else
            brand_was_known = item_type_known(*weapon);
    }

    special_damage = 0;
    obvious_effect = false;
    brand = damage_brand == SPWPN_CHAOS ? random_chaos_brand() : damage_brand;

    if (brand != SPWPN_FLAMING && brand != SPWPN_FREEZING
        && brand != SPWPN_ELECTROCUTION && brand != SPWPN_VAMPIRICISM
        && !defender->alive())
    {
        // Most brands have no extra effects on just killed enemies, and the
        // effect would be often inappropriate.
        return false;
    }

    if (!damage_done
        && (brand == SPWPN_FLAMING || brand == SPWPN_FREEZING
            || brand == SPWPN_HOLY_WRATH || brand == SPWPN_DRAGON_SLAYING
            || brand == SPWPN_VORPAL || brand == SPWPN_VAMPIRICISM
            || brand == SPWPN_ANTIMAGIC))
    {
        // These brands require some regular damage to function.
        return false;
    }

    switch (brand)
    {
    case SPWPN_FLAMING:
        res = fire_res_apply_cerebov_downgrade(defender->res_fire());
        calc_elemental_brand_damage(BEAM_FIRE, res,
                                    defender->is_icy() ? "melt" : "burn");
        defender->expose_to_element(BEAM_FIRE);
        noise_factor += 400 / max(1, damage_done);
        break;

    case SPWPN_FREEZING:
        calc_elemental_brand_damage(BEAM_COLD, defender->res_cold(), "freeze");
        defender->expose_to_element(BEAM_COLD, 2, false);
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
        noise_factor += 800 / max(1, damage_done);
        if (defender->res_elec() > 0)
            break;
        else if (one_chance_in(3))
        {
            special_damage_message =
                defender->is_player()?
                   "You are electrocuted!"
                :  "There is a sudden explosion of sparks!";
            special_damage = 10 + random2(15);
            special_damage_flavour = BEAM_ELECTRICITY;

            // Check for arcing in water, and add the final effect.
            const coord_def& pos = defender->pos();

            // We know the defender isn't electricity resistant, from
            // above, but we still have to make sure it is in water.
            if (_conduction_affected(pos))
                (new lightning_fineff(attacker, pos))->schedule();
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

            if (defender->is_player())
                old_poison = you.duration[DUR_POISONING];
            else
            {
                old_poison =
                    (defender->as_monster()->get_ench(ENCH_POISON)).degree;
            }

            // Weapons of venom do two levels of poisoning to the player,
            // but only one level to monsters.
            defender->poison(attacker, 2);

            if (defender->is_player()
                   && old_poison < you.duration[DUR_POISONING]
                || !defender->is_player()
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
        special_damage = 1 + random2(damage_done) / 3;
        // Note: Leaving special_damage_message empty because there isn't one.
        break;

    case SPWPN_VAMPIRICISM:
    {
        if (x_chance_in_y(defender->res_negative_energy(), 3))
            break;

        if (!weapon || defender->holiness() != MH_NATURAL || damage_done < 1
            || attacker->stat_hp() == attacker->stat_maxhp()
            || !defender->is_player()
               && defender->as_monster()->is_summoned()
            || attacker->is_player() && you.duration[DUR_DEATHS_DOOR]
            || !attacker->is_player()
               && attacker->as_monster()->has_ench(ENCH_DEATHS_DOOR)
            || one_chance_in(5))
        {
            break;
        }

        obvious_effect = true;

        // Handle weapon effects.
        // We only get here if we've done base damage, so no
        // worries on that score.
        if (attacker->is_player())
            mpr("You feel better.");
        else if (attacker_visible)
        {
            if (defender->is_player())
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

        dprf(DIAG_COMBAT, "Vampiric Healing: damage %d, healed %d",
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
        // occasionally come up with this brand. -cao
        if (defender->is_player())
            break;

        const int hdcheck =
            (defender->holiness() == MH_NATURAL ? random2(30) : random2(22));

        if (mons_class_is_confusable(defender->type)
            && hdcheck >= defender->get_experience_level()
            && !one_chance_in(5)
            && !defender->as_monster()->check_clarity(false))
        {
            // Declaring these just to pass to the enchant function.
            bolt beam_temp;
            beam_temp.thrower =
                attacker->is_player() ? KILL_YOU : KILL_MON;
            beam_temp.flavour = BEAM_CONFUSION;
            beam_temp.beam_source = attacker->mindex();
            beam_temp.apply_enchantment_to_monster(defender->as_monster());
            obvious_effect = beam_temp.obvious_effect;
        }

        if (attacker->is_player() && damage_brand == SPWPN_CONFUSE
            && you.duration[DUR_CONFUSING_TOUCH])
        {
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

    if (attacker->is_player() && damage_brand == SPWPN_CHAOS)
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

        special_damage_message.clear();
        // Don't do message-only miscasts along with a special
        // damage message.
        if (miscast_level == 0)
            miscast_level = -1;
    }

    if (special_damage > 0)
        inflict_damage(special_damage, special_damage_flavour);

    if (obvious_effect && attacker_visible && weapon != NULL)
    {
        if (is_artefact(*weapon))
            artefact_wpn_learn_prop(*weapon, ARTP_BRAND);
        else
            set_ident_flags(*weapon, ISFLAG_KNOW_TYPE);
    }

    return ret;
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
//  * For player, stealth skill and/or weapon skill reducing noise.
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
        level = max(1, level);

    // Cap melee noise at shouting volume.
    level = min(12, level);

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
    // XXX: Tentatively making an exception for spectral weapons
    if (attacker->is_monster() && attacker->type!= MONS_SPECTRAL_WEAPON
        && !one_chance_in(4))
    {
        return false;
    }

    // Only cutting implements.
    if (dam_type != DVORP_SLICING && dam_type != DVORP_CHOPPING
        && dam_type != DVORP_CLAWING || dam <= 0)
    {
        return false;
    }

    if (dam < 4 && wpn_brand != SPWPN_VORPAL && coinflip())
        return false;

    // Small claws are not big enough.
    if (dam_type == DVORP_CLAWING && attacker->has_claws() < 3)
        return false;

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
        {
            bleed_onto_floor(defender->pos(), defender->type,
                             defender->as_monster()->hit_points, true);
        }

        defender->hurt(attacker, INSTANT_DEATH);

        return true;
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
                return true;
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

    return false;
}

bool melee_attack::decapitate_hydra(int dam, int damage_type)
{
    if (defender->is_monster()
        && defender->as_monster()->has_hydra_multi_attack()
        && defender->type != MONS_SPECTRAL_THING)
    {
        const int dam_type = (damage_type != -1) ? damage_type
                                                 : attacker->damage_type();
        const brand_type wpn_brand = attacker->damage_brand();

        return chop_hydra_head(dam, dam_type, wpn_brand);
    }
    return false;
}

void melee_attack::attacker_sustain_passive_damage()
{
    // If the defender has been cleaned up, it's too late for anything.
    if (defender->type == MONS_PROGRAM_BUG)
        return;

    if (mons_class_flag(defender->type, M_ACID_SPLASH)
        || defender->is_player() && you.form == TRAN_JELLY)
    {
        int rA = attacker->res_acid();
        if (rA < 3)
        {
            int acid_strength = resist_adjust_damage(attacker, BEAM_ACID, rA, 5);
            item_def *weap = weapon;

            if (!weap)
                weap = attacker->slot_item(EQ_GLOVES);

            if (weap)
            {
                if (x_chance_in_y(acid_strength + 1, 20))
                    corrode_item(*weap, attacker);
            }
            else if (attacker->is_player())
            {
                mprf("Your %s burn!", you.hand_name(true).c_str());
                ouch(roll_dice(1, acid_strength), defender->mindex(),
                     KILLED_BY_ACID);
            }
            else
            {
                simple_monster_message(attacker->as_monster(),
                                       " is burned by acid!");
                attacker->hurt(defender, roll_dice(1, acid_strength),
                    BEAM_ACID, false);
            }
        }
    }
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
    if (!weapon || attacker->suppressed())
        return;

    if (weapon->base_type == OBJ_RODS && weapon->sub_type == ROD_STRIKING)
    {
        if (weapon->plus < ROD_CHARGE_MULT)
            return;

        weapon->plus -= ROD_CHARGE_MULT;
        if (attacker->is_player())
            you.wield_change = true;

        special_damage = 1 + random2(attacker->skill(SK_EVOCATIONS, 150)) / 100;
        special_damage = apply_defender_ac(special_damage);
        return;
    }

    if (weapon->base_type != OBJ_STAVES)
        return;

    switch (weapon->sub_type)
    {
    case STAFF_AIR:
        if (damage_done + attacker->skill_rdiv(SK_AIR_MAGIC) <= random2(20))
            break;

        special_damage =
            resist_adjust_damage(defender,
                                 BEAM_ELECTRICITY,
                                 defender->res_elec(),
                                 staff_damage(SK_AIR_MAGIC));

        if (special_damage)
        {
            special_damage_message =
                make_stringf("%s %s electrocuted!",
                             defender->name(DESC_THE).c_str(),
                             defender->is_player() ? "are" : "is");
            special_damage_flavour = BEAM_ELECTRICITY;
        }

        break;

    case STAFF_COLD:
        special_damage =
            resist_adjust_damage(defender,
                                 BEAM_COLD,
                                 defender->res_cold(),
                                 staff_damage(SK_ICE_MAGIC));

        if (special_damage)
        {
            special_damage_message =
                make_stringf(
                    "%s freeze%s %s!",
                    attacker->name(DESC_THE).c_str(),
                    attacker->is_player() ? "" : "s",
                    defender->name(DESC_THE).c_str());
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
                                 defender->res_fire(),
                                 staff_damage(SK_FIRE_MAGIC));

        if (special_damage)
        {
            special_damage_message =
                make_stringf(
                    "%s burn%s %s!",
                    attacker->name(DESC_THE).c_str(),
                    attacker->is_player() ? "" : "s",
                    defender->name(DESC_THE).c_str());
        }
        break;

    case STAFF_POISON:
    {
        if (random2(300) >= attacker->skill(SK_EVOCATIONS, 20) + attacker->skill(SK_POISON_MAGIC, 10))
            return;

        // Base chance at 50% -- like mundane weapons.
        if (coinflip() || x_chance_in_y(attacker->skill(SK_POISON_MAGIC, 10), 80))
        {
            defender->poison(attacker, 2, defender->has_lifeforce()
                & x_chance_in_y(attacker->skill(SK_POISON_MAGIC, 10), 160));
        }
        break;
    }

    case STAFF_DEATH:
        if (defender->res_negative_energy())
            break;

        special_damage = staff_damage(SK_NECROMANCY);

        if (special_damage)
        {
            special_damage_message =
                make_stringf(
                    "%s convulse%s in agony!",
                    defender->name(DESC_THE).c_str(),
                    defender->is_player() ? "" : "s");

            attacker->god_conduct(DID_NECROMANCY, 4);
        }
        break;

    case STAFF_SUMMONING:
        if (!defender->is_summoned())
            break;

        if (x_chance_in_y(attacker->skill(SK_EVOCATIONS, 20)
                        + attacker->skill(SK_SUMMONINGS, 10), 300))
        {
            cast_abjuration((attacker->skill(SK_SUMMONINGS, 100)
                            + attacker->skill(SK_EVOCATIONS, 50)) / 80,
                            defender->pos());
        }
        break;

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

/* Calculate the to-hit for an attacker
 *
 * @param random deterministic or stochastic calculation(s)
 */
int melee_attack::calc_to_hit(bool random)
{
    const bool fighter = attacker->is_monster()
                         && attacker->as_monster()->is_fighter();
    const int hd_mult = fighter ? 25 : 15;
    int mhit = attacker->is_player() ?
                15 + (calc_stat_to_hit_base() / 2)
              : 18 + attacker->get_experience_level() * hd_mult / 10;

#ifdef DEBUG_DIAGNOSTICS
    const int base_hit = mhit;
#endif

    // This if statement is temporary, it should be removed when the
    // implementation of a more universal (and elegant) to-hit calculation
    // is designed. The actual code is copied from the old mons_to_hit and
    // player_to_hit methods.
    if (attacker->is_player())
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
        else if (_form_uses_xl())
            mhit += maybe_random_div(you.experience_level * 100, 100, random);
        else
        {                       // ...you must be unarmed
            // Members of clawed species have presumably been using the claws,
            // making them more practiced and thus more accurate in unarmed
            // combat.  They keep this benefit even the claws are covered (or
            // missing because of a change in form).
            mhit += species_has_claws(you.species) ? 4 : 2;

            mhit += maybe_random_div(you.skill(SK_UNARMED_COMBAT, 100), 100,
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
                         && you_worship(GOD_BEOGH) && !player_under_penance())
                {
                    mhit++;
                }

            }
            else if (weapon->base_type == OBJ_STAVES)
                mhit += property(*weapon, PWPN_HIT);
            else if (weapon->base_type == OBJ_RODS)
            {
                mhit += property(*weapon, PWPN_HIT);
                mhit += weapon->special;
            }
        }

        // slaying bonus
        mhit += slaying_bonus(PWPN_HIT);

        // hunger penalty
        if (you.hunger_state == HS_STARVING)
            mhit -= 3;

        // armour penalty
        mhit -= (attacker_armour_tohit_penalty + attacker_shield_tohit_penalty);

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

            // TODO: Review this later (transformations getting extra hit
            // almost across the board seems bad) - Cryp71c
            switch (you.form)
            {
            case TRAN_SPIDER:
            case TRAN_ICE_BEAST:
            case TRAN_DRAGON:
            case TRAN_LICH:
            case TRAN_FUNGUS:
            case TRAN_TREE:
            case TRAN_WISP:
                mhit += maybe_random2(10, random);
                break;
            case TRAN_BAT:
            case TRAN_BLADE_HANDS:
                mhit += maybe_random2(12, random);
                break;
            case TRAN_STATUE:
                mhit += maybe_random2(9, random);
                break;
            case TRAN_PORCUPINE:
            case TRAN_PIG:
            case TRAN_APPENDAGE:
            case TRAN_JELLY:
            case TRAN_NONE:
                break;
            }
        }
    }
    else    // Monster to-hit.
    {
        if (weapon && is_weapon(*weapon) && !is_range_weapon(*weapon))
            mhit += weapon->plus + property(*weapon, PWPN_HIT);

        const int jewellery = attacker->as_monster()->inv[MSLOT_JEWELLERY];
        if (jewellery != NON_ITEM
            && mitm[jewellery].base_type == OBJ_JEWELLERY
            && mitm[jewellery].sub_type == RING_SLAYING)
        {
            mhit += mitm[jewellery].plus;
        }

        mhit += attacker->scan_artefacts(ARTP_ACCURACY);

        if (weapon && weapon->base_type == OBJ_RODS)
            mhit += weapon->special;
    }

    // Penalties for both players and monsters:

    if (attacker->inaccuracy())
        mhit -= 5;

    // If you can't see yourself, you're a little less accurate.
    if (!attacker->visible_to(attacker))
        mhit -= 5;

    if (attacker->confused())
        mhit -= 5;

    if (weapon && is_unrandom_artefact(*weapon) && weapon->special == UNRAND_WOE)
        return AUTOMATIC_HIT;

    // If no defender, we're calculating to-hit for debug-display
    // purposes, so don't drop down to defender code below
    if (defender == NULL)
        return mhit;

    if (!defender->visible_to(attacker))
        if (attacker->is_player())
            mhit -= 6;
        else
            mhit = mhit * 65 / 100;
    else
    {
        // This can only help if you're visible!
        if (defender->is_player()
            && player_mutation_level(MUT_TRANSLUCENT_SKIN) >= 3)
        {
            mhit -= 5;
        }

        if (defender->backlit(true, false))
            mhit += 2 + random2(8);
        else if (!attacker->nightvision()
                 && defender->umbra(true, true))
            mhit -= 2 + random2(4);
    }
    // Don't delay doing this roll until test_hit().
    if (!attacker->is_player())
        mhit = random2(mhit + 1);

    dprf(DIAG_COMBAT, "%s: Base to-hit: %d, Final to-hit: %d",
         attacker->name(DESC_PLAIN).c_str(),
         base_hit, mhit);

    return mhit;
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
 * @param apply haste/finesse scaling
 */
int melee_attack::calc_attack_delay(bool random, bool scaled)
{
    if (attacker->is_player())
    {
        random_var attack_delay = weapon ? player_weapon_speed()
                                         : player_unarmed_speed();
        // At the moment it never gets this low anyway.
        attack_delay = rv::max(attack_delay, constant(3));

        // Calculate this separately to avoid overflowing the weights in
        // the random_var.
        random_var shield_penalty = constant(0);
        if (attacker_shield_penalty)
        {
            shield_penalty = div_rand_round(rv::min(rv::roll_dice(1, attacker_shield_penalty),
                                                    rv::roll_dice(1, attacker_shield_penalty)),
                                            20);
        }
        // Give unarmed shield-users a slight penalty always.
        if (!weapon && player_wearing_slot(EQ_SHIELD))
            shield_penalty += rv::random2(2);

        int final_delay = random ? attack_delay.roll() + shield_penalty.roll()
                                 : attack_delay.expected() + shield_penalty.expected();
        // Stop here if we just want the unmodified value.
        if (!scaled)
            return final_delay;

        const int scaling = finesse_adjust_delay(you.time_taken);
        return max(2, div_rand_round(scaling * final_delay, 10));
    }
    else
    {
        if (!weapon)
            return 10;

        int delay = property(*weapon, PWPN_SPEED);
        if (damage_brand == SPWPN_SPEED)
            delay = (delay + 1) / 2;
        return random ? div_rand_round(10 + delay, 2) : (10 + delay) / 2;
    }

    return 0;
}

/* Check for stab and prepare combat for stab-values
 *
 * Grant an automatic stab if paralyzed or sleeping (with highest damage value)
 * stab_bonus is used as the divisor in damage calculations, so lower values
 * will yield higher damage. Normal stab chance is (stab_skill + dex + 1 / roll)
 * This averages out to about 1/3 chance for a non extended-endgame stabber.
 */
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
    case NUM_UCAT:
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
        stab_attempt = x_chance_in_y(you.skill_rdiv(wpn_skill, 1, 2)
                                     + you.skill_rdiv(SK_STEALTH, 1, 2)
                                     + you.dex() + 1,
                                     roll);
    }

    if (stab_attempt)
        count_action(CACT_STAB, uat);
}


// TODO: Unify this and player_unarmed_speed (if possible), then unify with
// monster weapon/attack speed
random_var melee_attack::player_weapon_speed()
{
    random_var attack_delay = constant(15);

    if (weapon && is_weapon(*weapon) && !is_range_weapon(*weapon))
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

        // never go faster than speed 3 (ie 3.33 attacks per round)
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

    return attack_delay;
}

random_var melee_attack::player_unarmed_speed()
{
    if (_form_uses_xl())
    {
        return constant(10)
               - div_rand_round(constant(you.experience_level * 10), 54);
    }

    random_var unarmed_delay =
        rv::max(constant(10),
                (rv::roll_dice(1, 10) +
                 div_rand_round(rv::roll_dice(2, attacker_body_armour_penalty), 20)));

    // Unarmed speed. Min delay is 10 - 270/54 = 5.
    if (you.burden_state == BS_UNENCUMBERED)
        unarmed_delay -= div_rand_round(constant(you.skill(SK_UNARMED_COMBAT, 10)), 54);
    // Bats are faster (for what good it does them).
    if (you.form == TRAN_BAT)
        unarmed_delay = div_rand_round(unarmed_delay * constant(3), 5);
    return unarmed_delay;
}

bool melee_attack::attack_warded_off()
{
    const int WARDING_CHECK = 60;

    if (defender->warding()
        && attacker->is_summoned()
        && attacker->as_monster()->check_res_magic(WARDING_CHECK) <= 0)
    {
        if (needs_message)
        {
            mprf("%s tries to attack %s, but flinches away.",
                 atk_name(DESC_THE).c_str(),
                 defender_name().c_str());
        }

        if (defender->is_player()
            && you.wearing(EQ_AMULET, AMU_WARDING, true)
            && !you.wearing(EQ_STAFF, STAFF_SUMMONING, true))
        {
            item_def *amu = you.slot_item(EQ_AMULET);
            ASSERT(amu);
            wear_id_type(*amu);
        }
        return true;
    }

    return false;
}

/* Determine whether a block occurred
 *
 * No blocks if defender is incapacitated, would be nice to eventually expand
 * this method to handle partial blocks as well as full blocks (although this
 * would serve as a nerf to shields and - while more realistic - may not be a
 * good mechanic for shields.
 *
 * Returns (block_occurred)
 */
bool melee_attack::attack_shield_blocked(bool verbose)
{
    if (!defender_shield && !defender->is_player())
        return false;

    if (defender->incapacitated())
        return false;

    const int con_block = random2(attacker->shield_bypass_ability(to_hit)
                                  + defender->shield_block_penalty());
    int pro_block = defender->shield_bonus();

    if (!attacker->visible_to(defender))
        pro_block /= 3;

    dprf(DIAG_COMBAT, "Defender: %s, Pro-block: %d, Con-block: %d",
         def_name(DESC_PLAIN).c_str(), pro_block, con_block);

    if (pro_block >= con_block)
    {
        perceived_attack = true;

        if (attacker->is_monster()
            && attacker->as_monster()->type == MONS_PHANTASMAL_WARRIOR)
        {
            if (needs_message && verbose)
            {
                mprf("%s blade passes through %s shield.",
                    atk_name(DESC_ITS).c_str(),
                    def_name(DESC_ITS).c_str());
                return false;
            }
        }

        if (needs_message && verbose)
        {
            mprf("%s %s %s attack.",
                 def_name(DESC_THE).c_str(),
                 defender->conj_verb("block").c_str(),
                 atk_name(DESC_ITS).c_str());
        }

        defender->shield_block_succeeded(attacker);

        return true;
    }

    return false;
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
        return "slap";
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
        "splash",
        "pounce on",
        "sting",
    };

    ASSERT(attk_type < (int)ARRAYSZ(attack_types));
    return (attack_types[attk_type]);
}

string melee_attack::mons_attack_desc()
{
    if (!you.can_see(attacker))
        return "";

    string ret;
    int dist = (attacker->pos() - defender->pos()).abs();
    if (dist > 2)
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
        bool force = false;

        if (attk_flavour == AF_POISON_NASTY)
            amount++;
        else if (attk_flavour == AF_POISON_MEDIUM)
            amount += random2(3);
        else if (attk_flavour == AF_POISON_STRONG)
        {
            if (defender->res_poison() > 0 && defender->has_lifeforce())
            {
                amount += random2(3);
                force = true;
            }
            else
                amount += roll_dice(2, 5);
        }

        if (!defender->poison(attacker, amount, force))
            return;

        if (needs_message)
        {
            mprf("%s poisons %s!",
                 atk_name(DESC_THE).c_str(),
                 defender_name().c_str());
            if (force)
            {
                mprf("%s partially resist%s.",
                    defender_name().c_str(),
                    defender->is_player() ? "" : "s");
            }
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

        if (defender->is_player())
            napalm_player(random2avg(7, 3) + 1, atk_name(DESC_A));
        else
        {
            napalm_monster(
                defender->as_monster(),
                attacker,
                min(4, 1 + random2(attacker->get_experience_level())/2));
        }
    }
}

void melee_attack::splash_defender_with_acid(int strength)
{
    if (defender->is_player())
    {
        mpr("You are splashed with acid!");
        splash_with_acid(strength, attacker->mindex());
    }
    else
    {
        special_damage += roll_dice(2, 4);
        if (defender_visible)
            mprf("%s is splashed with acid.", defender->name(DESC_THE).c_str());
        corrode_monster(defender->as_monster(), attacker);
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

void melee_attack::mons_apply_attack_flavour()
{
    // Most of this is from BWR 4.1.2.
    int base_damage = 0;

    attack_flavour flavour = attk_flavour;
    if (flavour == AF_CHAOS)
        flavour = random_chaos_attack_flavour();

    switch (flavour)
    {
    default:
        break;

    case AF_MUTATE:
        if (one_chance_in(4))
        {
            defender->malmutate(you.can_see(attacker) ?
                apostrophise(attacker->name(DESC_PLAIN)) + " mutagenic touch" :
                "mutagenic touch");
        }
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
        if (defender->poison(attacker, 1))
        {
            if (one_chance_in(3))
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
        base_damage = attacker->get_experience_level()
                      + random2(attacker->get_experience_level());
        special_damage =
            resist_adjust_damage(defender,
                                 BEAM_FIRE,
                                 defender->res_fire(),
                                 base_damage);
        special_damage_flavour = BEAM_FIRE;

        if (needs_message && base_damage)
        {
            mprf("%s %s engulfed in flames%s",
                 def_name(DESC_THE).c_str(),
                 defender->conj_verb("are").c_str(),
                 special_attack_punctuation().c_str());

            _print_resist_messages(defender, base_damage, BEAM_FIRE);
        }

        defender->expose_to_element(BEAM_FIRE, 2);
        break;

    case AF_COLD:
        base_damage = attacker->get_experience_level()
                      + random2(2 * attacker->get_experience_level());
        special_damage =
            resist_adjust_damage(defender,
                                 BEAM_COLD,
                                 defender->res_cold(),
                                 base_damage);
        special_damage_flavour = BEAM_COLD;

        if (needs_message && base_damage)
        {
            mprf("%s %s %s%s",
                 atk_name(DESC_THE).c_str(),
                 attacker->conj_verb("freeze").c_str(),
                 defender_name().c_str(),
                 special_attack_punctuation().c_str());

            _print_resist_messages(defender, base_damage, BEAM_COLD);
        }

        defender->expose_to_element(BEAM_COLD, 2);
        break;

    case AF_ELEC:
        base_damage = attacker->get_experience_level()
                      + random2(attacker->get_experience_level() / 2);

        special_damage =
            resist_adjust_damage(defender,
                                 BEAM_ELECTRICITY,
                                 defender->res_elec(),
                                 base_damage);
        special_damage_flavour = BEAM_ELECTRICITY;

        if (needs_message && base_damage)
        {
            mprf("%s %s %s%s",
                 atk_name(DESC_THE).c_str(),
                 attacker->conj_verb("shock").c_str(),
                 defender_name().c_str(),
                 special_attack_punctuation().c_str());

            _print_resist_messages(defender, base_damage, BEAM_ELECTRICITY);
        }

        dprf(DIAG_COMBAT, "Shock damage: %d", special_damage);
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
            if (attacker->heal(1 + random2(damage_done), coinflip())
                && needs_message)
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
        // blinking can kill, delay the call
        if (one_chance_in(3))
            (new blink_fineff(attacker))->schedule();
        break;

    case AF_CONFUSE:
        if (attk_type == AT_SPORE)
        {
            if (defender->is_unbreathing())
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
        // Only wasps at the moment, so Zin vitalisation
        // protects from the paralysis and slow.
        if (you.duration[DUR_DIVINE_STAMINA] > 0)
        {
            mpr("Your divine stamina protects you from poison!");
            break;
        }

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
        if (!defender->is_player())
            break;

        attacker->as_monster()->steal_item_from_player();
        break;

    case AF_STEAL_FOOD:
    {
        // Monsters don't carry food.
        if (!defender->is_player())
            break;

        // The expose_ message doesn't convey the agent.
        if (needs_message)
            mprf("%s lunges at you hungrily!", atk_name(DESC_THE).c_str());

        expose_player_to_element(BEAM_DEVOUR_FOOD, 10);
        const bool ground = expose_items_to_element(BEAM_DEVOUR_FOOD, you.pos(),
                                                    10);

        if (needs_message && ground)
            mpr("Some of the food beneath you is devoured!");
        break;
    }

    case AF_HOLY:
        if (defender->undead_or_demonic())
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
                 defender_name().c_str());
        }
        attacker->start_constricting(*defender);
        // if you got grabbed, interrupt stair climb and passwall
        if (defender->is_player())
            stop_delay(true);
        break;

    case AF_DROWN:
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
                     defender_name().c_str());
            }
        }
        break;

    case AF_PURE_FIRE:
        if (attacker->type == MONS_FIRE_VORTEX)
            attacker->as_monster()->suicide(-10);

        special_damage = (attacker->get_experience_level() * 3 / 2
                          + random2(attacker->get_experience_level()));
        special_damage = defender->apply_ac(special_damage, 0, AC_HALF);
        special_damage = resist_adjust_damage(defender,
                                              BEAM_FIRE,
                                              defender->res_fire(),
                                              special_damage);

        if (needs_message && special_damage)
        {
            mprf("%s %s %s!",
                    atk_name(DESC_THE).c_str(),
                    attacker->conj_verb("burn").c_str(),
                    def_name(DESC_THE).c_str());

            _print_resist_messages(defender, special_damage, BEAM_FIRE);
        }

        defender->expose_to_element(BEAM_FIRE, 2);
        break;

    case AF_DRAIN_SPEED:
        if (x_chance_in_y(3, 5) && !defender->res_negative_energy())
        {
            if (needs_message)
            {
                mprf("%s %s %s vigor!",
                     atk_name(DESC_THE).c_str(),
                     attacker->conj_verb("drain").c_str(),
                     def_name(DESC_ITS).c_str());
            }

            special_damage = 1 + random2(damage_done) / 2;
            defender->slow_down(attacker, 5 + random2(7));
        }
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
                mon_enchant lowered_mr(ENCH_LOWERED_MR, 1, attacker, 20 + random2(20));
                defender->as_monster()->add_ench(lowered_mr);
            }

            if (needs_message && visible_effect)
            {
                mprf("%s magical defenses are stripped away!",
                     def_name(DESC_ITS).c_str());
            }
        }
        break;

    case AF_PLAGUE:
        if (defender->sicken(30 + random2(50), true, true))
        {
            if (defender->is_player())
            {
                you.increase_duration(DUR_RETCHING, 7 + random2(9), 25);
                mpr("You feel violently ill.");
            }
            else
            {
                if (!defender->as_monster()->has_ench(ENCH_RETCHING)
                    && you.can_see(defender))
                {
                    simple_monster_message(defender->as_monster(),
                                           " looks violently ill.");
                }
                defender->as_monster()->add_ench(ENCH_RETCHING);
            }
        }
        break;

    case AF_WEAKNESS_POISON:
        if (defender->poison(attacker, 1))
        {
            if (coinflip())
                defender->weaken(attacker, 12);
        }
        break;

    case AF_SHADOWSTAB:
        attacker->as_monster()->del_ench(ENCH_INVIS, true);
        break;

    case AF_WATERPORT:
        if (!defender->no_tele())
            waterport_touch(attacker->as_monster(), defender);
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

        simple_monster_message(mon, " is very cold.");

#ifndef USE_TILE_LOCAL
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

        simple_monster_message(mon, " is singed by your heat.");

#ifndef USE_TILE
        flash_monster_colour(mon, LIGHTRED, 200);
#endif

        mon->hurt(&you, hurted);

        if (mon->alive())
        {
            mon->expose_to_element(BEAM_FIRE, orig_hurted);
            print_wounds(mon);
        }
    }
}

void melee_attack::mons_do_eyeball_confusion()
{
    if (you.mutation[MUT_EYEBALLS]
        && attacker->alive()
        && adjacent(you.pos(), attacker->as_monster()->pos())
        && x_chance_in_y(player_mutation_level(MUT_EYEBALLS), 20))
    {
        const int ench_pow = player_mutation_level(MUT_EYEBALLS) * 30;
        monster* mon = attacker->as_monster();

        if (mon->check_res_magic(ench_pow) <= 0
            && mons_class_is_confusable(mon->type))
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

void melee_attack::tendril_disarm()
{
    monster *mon = attacker->as_monster();
    item_def *mons_wpn = mon->mslot_item(MSLOT_WEAPON);

    if (!mons_wpn)
        return;

    // assume the player would not pull weapons into terrain that would destroy them
    if (!((feat_has_solid_floor(grd(you.pos()))
           && feat_has_solid_floor(grd(mon->pos())))
          || (feat_is_watery(grd(you.pos())) && species_likes_water(you.species)
              && grd(mon->pos()) != DNGN_LAVA)))
    {
        return;
    }

    if (you.mutation[MUT_TENDRILS]
        && attacker->alive()
        && (!mons_class_is_animated_weapon(mon->type))
        && adjacent(you.pos(), mon->pos())
        && you.can_see(mon)
        && one_chance_in(5)
        && (random2(you.dex()) > (mons_class_flag(mon->type, M_FIGHTER)) ? mon->hit_dice * 1.5 : mon->hit_dice
            || random2(you.strength()) > (mons_class_flag(mon->type, M_FIGHTER)) ? mon->hit_dice * 1.5 : mon->hit_dice)
        && !mons_wpn->cursed())
    {
        mprf("Your tendrils lash around %s %s and pull it to the ground!",
             apostrophise(mon->name(DESC_THE)).c_str(), mons_wpn->name(DESC_PLAIN).c_str());

        mon->drop_item(MSLOT_WEAPON, false);
        move_top_item(mon->pos(), you.pos());
    }
}

void melee_attack::do_spines()
{
    // Monsters only get struck on their first attack per round
    if (attacker->is_monster() && effective_attack_number > 0)
        return;

    if (defender->is_player())
    {
        const item_def *body = you.slot_item(EQ_BODY_ARMOUR, false);
        const int evp = body ? -property(*body, PARM_EVASION) : 0;
        const int mut = (you.form == TRAN_PORCUPINE) ? 3
                        : player_mutation_level(MUT_SPINY);

        if (mut && attacker->alive() && one_chance_in(evp / 3 + 1)
            && x_chance_in_y(2, 13 - (mut * 2)))
        {
            int dmg = roll_dice(2 + div_rand_round(mut - 1, 2), 5);
            int hurt = attacker->apply_ac(dmg) - evp / 3;

            dprf(DIAG_COMBAT, "Spiny: dmg = %d hurt = %d", dmg, hurt);

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
    else if (defender->as_monster()->spiny_degree() > 0)
    {
        // Thorn hunters can attack their own brambles without injury
        if (defender->as_monster()->type == MONS_BRIAR_PATCH
            && attacker->is_monster()
            && attacker->as_monster()->type == MONS_THORN_HUNTER)
        {
            return;
        }

        const int degree = defender->as_monster()->spiny_degree();

        if (attacker->alive() && (x_chance_in_y(2, 5)
            || random2(div_rand_round(attacker->armour_class(), 2)) < degree))
        {
            int dmg = (attacker->is_monster() ? roll_dice(degree, 3)
                                              : roll_dice(degree, 4));
            int hurt = attacker->apply_ac(dmg, AC_HALF);
            dprf(DIAG_COMBAT, "Spiny: dmg = %d hurt = %d", dmg, hurt);

            if (hurt <= 0)
                return;
            if (you.can_see(defender) || attacker->is_player())
            {
                mprf("%s %s struck by %s %s.", attacker->name(DESC_THE).c_str(),
                     attacker->conj_verb("are").c_str(),
                     defender->name(DESC_ITS).c_str(),
                     defender->as_monster()->type == MONS_BRIAR_PATCH ? "thorns"
                                                                      : "spines");
            }
            if (attacker->is_player())
                ouch(hurt, defender->mindex(), KILLED_BY_SPINES);
            else
                attacker->hurt(defender, hurt);
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
            && env.cgrid(mon->pos()) == EMPTY_CLOUD)
        {
            mpr("You emit a cloud of foul miasma!");
            place_cloud(CLOUD_MIASMA, mon->pos(), 5 + random2(6), &you);
        }
    }
}

void melee_attack::do_minotaur_retaliation()
{
    if (defender->cannot_act()
        || defender->confused()
        || !attacker->alive()
        || attacker->is_monster() && mons_wall_shielded(attacker->as_monster())
        || defender->is_player() && you.duration[DUR_LIFESAVING])
    {
        return;
    }

    if (!defender->is_player())
    {
        // monsters have no STR or DEX
        if (!x_chance_in_y(2, 5))
            return;

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
                     get_exclams(hurt).c_str());
            }
        }
        if (hurt > 0)
        {
            if (attacker->is_player())
                ouch(hurt, defender->mindex(), KILLED_BY_HEADBUTT);
            else
                attacker->hurt(defender, hurt);
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
        dmg = player_aux_stat_modify_damage(dmg);
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
                 get_exclams(hurt).c_str());
            attacker->hurt(&you, hurt);
        }
    }
}

bool melee_attack::do_knockback(bool trample)
{
    do
    {
        monster* def_monster = defender->as_monster();
        if (def_monster && mons_is_stationary(def_monster))
            return false; // don't even print a message

        int size_diff =
            attacker->body_size(PSIZE_BODY) - defender->body_size(PSIZE_BODY);
        if (!x_chance_in_y(size_diff + 3, 6))
            break;

        coord_def old_pos = defender->pos();
        coord_def new_pos = defender->pos() + defender->pos() - attacker->pos();

        // need a valid tile
        if (grd(new_pos) < DNGN_SHALLOW_WATER
            && !defender->is_habitable_feat(grd(new_pos)))
        {
            break;
        }

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

        // Schedule following _before_ actually trampling -- if the defender
        // is a player, a shaft trap will unload the level.  If trampling will
        // somehow fail, move attempt will be ignored.
        if (trample)
            (new trample_follow_fineff(attacker, old_pos))->schedule();

        if (defender->as_player())
            move_player_to_grid(new_pos, false, true);
        else
            defender->move_to_pos(new_pos);

        // Interrupt stair travel and passwall.
        if (defender->is_player())
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

//cleave can cover up to 7 cells (not the one opposite to the target), but is
// stopped by solid features. Allies are passed through without harm.
void melee_attack::cleave_setup()
{
    if (feat_is_solid(grd(defender->pos())))
        return;

    // Don't cleave on a self-attack.
    if (attacker->pos() == defender->pos())
        return;

    int dir = coinflip() ? -1 : 1;
    get_cleave_targets(attacker, defender->pos(), dir, cleave_targets);
    cleave_targets.reverse();
    attack_cleave_targets(attacker, cleave_targets, attack_number,
                          effective_attack_number);

    // We need to get the list of the remaining potential targets now because
    // if the main target dies, its position will be lost.
    get_cleave_targets(attacker, defender->pos(), -dir, cleave_targets);
}

// cleave damage modifier for additional attacks: 75% of base damage
int melee_attack::cleave_damage_mod(int dam)
{
    return div_rand_round(dam * 3, 4);
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
    case UNAT_PECK:
    case UNAT_HEADBUTT:
    case UNAT_PUNCH:
        return (you.form == TRAN_ICE_BEAST
                || you.form == TRAN_DRAGON
                || you.form == TRAN_SPIDER
                || you.form == TRAN_BAT);

    case UNAT_CONSTRICT:
        return !form_keeps_mutations();

    default:
        return false;
    }
}

bool melee_attack::_extra_aux_attack(unarmed_attack_type atk, bool is_uc)
{
    // No extra unarmed attacks for disabled mutations.
    if (_tran_forbid_aux_attack(atk))
        return false;

    if (atk == UNAT_CONSTRICT)
        return (is_uc
                 || you.species == SP_NAGA && you.experience_level > 12
                 || you.species == SP_OCTOPODE && you.has_usable_tentacle());

    if (you.strength() + you.dex() <= random2(50))
        return false;

    switch (atk)
    {
    case UNAT_KICK:
        return (is_uc
                 || player_mutation_level(MUT_HOOVES)
                 || you.has_usable_talons()
                 || player_mutation_level(MUT_TENTACLE_SPIKE));

    case UNAT_PECK:
        return ((is_uc
                 || player_mutation_level(MUT_BEAK))
                && !one_chance_in(3));

    case UNAT_HEADBUTT:
        return ((is_uc
                 || player_mutation_level(MUT_HORNS))
                && !one_chance_in(3));

    case UNAT_TAILSLAP:
        return ((is_uc
                 || you.has_usable_tail())
                && coinflip());

    case UNAT_PSEUDOPODS:
        return ((is_uc
                 || you.has_usable_pseudopods())
                && !one_chance_in(3));

    case UNAT_TENTACLES:
        return ((is_uc
                 || you.has_usable_tentacles())
                && !one_chance_in(3));

    case UNAT_BITE:
        return ((is_uc
                 || you.has_usable_fangs()
                 || player_mutation_level(MUT_ACIDIC_BITE))
                && x_chance_in_y(2, 5));

    case UNAT_PUNCH:
        return (is_uc && !one_chance_in(3));

    default:
        return false;
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
                + you.dex() * 60
                + you.strength() * 15
                + you.skill(SK_FIGHTING, 30);
    your_to_hit /= 100;

    if (you.inaccuracy())
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
    const int weight = weapon ? weapon_str_weight(*weapon) : 4;

    // dex is modified by strength towards the average, by the
    // weighted amount weapon_str_weight() / 20.
    return you.dex() + (you.strength() - you.dex()) * weight / 20;
}

int melee_attack::test_hit(int to_land, int ev, bool randomise_ev)
{
    int margin = AUTOMATIC_HIT;
    if (randomise_ev)
        ev = random2avg(2*ev, 2);
    if (to_land >= AUTOMATIC_HIT)
        return true;
    else if (x_chance_in_y(MIN_HIT_MISS_PERCENTAGE, 100))
        margin = (random2(2) ? 1 : -1) * AUTOMATIC_HIT;
    else
        margin = to_land - ev;

#ifdef DEBUG_DIAGNOSTICS
    dprf(DIAG_COMBAT, "to hit: %d; ev: %d; result: %s (%d)",
         to_hit, ev, (margin >= 0) ? "hit" : "miss", margin);
#endif

    return margin;
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

    if (attacker->is_player())
    {
        if (is_weapon(*weapon) && !is_range_weapon(*weapon))
            damage = property(*weapon, PWPN_DAMAGE);
    }

    return damage;
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

    if (attacker->is_player())
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
        case TRAN_FUNGUS:
        case TRAN_TREE:
            damage = 12;
            break;
        case TRAN_BLADE_HANDS:
            damage = 8 + div_rand_round(you.strength() + you.dex(), 3);
            break;
        case TRAN_STATUE: // multiplied by 1.5 later
            damage = 6 + div_rand_round(you.strength(), 3);
            break;
        case TRAN_DRAGON: // +6 from claws
            damage = 12 + div_rand_round(you.strength() * 2, 3);
            break;
        case TRAN_LICH:
        case TRAN_WISP:
            damage = 5;
            break;
        case TRAN_PIG:
        case TRAN_PORCUPINE:
        case TRAN_JELLY:
            break;
        case TRAN_NONE:
        case TRAN_APPENDAGE:
            break;
        }

        if (you.has_usable_claws())
        {
            // Claw damage only applies for bare hands.
            damage += you.has_claws(false) * 2;
            apply_bleeding = true;
        }

        if (_form_uses_xl())
            damage += you.experience_level;
        else if (you.form == TRAN_BAT || you.form == TRAN_PORCUPINE)
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
    // Constriction deals damage over time, not when grabbing.
    if (attk_flavour == AF_CRUSH)
        return 0;

    if (attacker->is_monster())
    {
        monster *as_mon = attacker->as_monster();

        int damage = 0;
        int damage_max = 0;
        if (weapon
            && (weapon->base_type == OBJ_WEAPONS
                && !is_range_weapon(*weapon)
                || weapon->base_type == OBJ_RODS))
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
            if (weapon->base_type == OBJ_RODS)
                wpn_damage_plus = weapon->special;

            const int jewellery = attacker->as_monster()->inv[MSLOT_JEWELLERY];
            if (jewellery != NON_ITEM
                && mitm[jewellery].base_type == OBJ_JEWELLERY
                && mitm[jewellery].sub_type == RING_SLAYING)
            {
                wpn_damage_plus += mitm[jewellery].plus2;
            }

            wpn_damage_plus += attacker->scan_artefacts(ARTP_DAMAGE);

            if (wpn_damage_plus >= 0)
                damage += random2(wpn_damage_plus);
            else
                damage -= random2(1 - wpn_damage_plus);

            damage -= 1 + random2(3);
        }

        damage_max += attk_damage;
        damage     += 1 + random2(attk_damage);
        int frenzy_degree = -1;

        // Berserk/mighted monsters get bonus damage.
        if (as_mon->has_ench(ENCH_MIGHT)
            || as_mon->has_ench(ENCH_BERSERK))
        {
            damage = damage * 3 / 2;
        }
        else if (as_mon->has_ench(ENCH_BATTLE_FRENZY))
            frenzy_degree = as_mon->get_ench(ENCH_BATTLE_FRENZY).degree;
        else if (as_mon->has_ench(ENCH_ROUSED))
            frenzy_degree = as_mon->get_ench(ENCH_ROUSED).degree;

        if (frenzy_degree != -1)
        {
#ifdef DEBUG_DIAGNOSTICS
            const int orig_damage = damage;
#endif

            damage = damage * (115 + frenzy_degree * 15) / 100;

            dprf(DIAG_COMBAT, "%s frenzy damage: %d->%d",
                 attacker->name(DESC_PLAIN).c_str(), orig_damage, damage);
        }

        if (as_mon->has_ench(ENCH_WEAK))
            damage = damage * 2 / 3;

        if (weapon && get_weapon_brand(*weapon) == SPWPN_SPEED)
            damage = div_rand_round(damage * 9, 10);

        bool half_ac = (as_mon->type == MONS_PHANTASMAL_WARRIOR);

        // If the defender is asleep, the attacker gets a stab.
        if (defender && (defender->asleep()
                         || (attk_flavour == AF_SHADOWSTAB
                             &&!defender->can_see(attacker))))
        {
            if (mons_class_flag(as_mon->type, M_STABBER))
                half_ac = true;

            damage = damage * 5 / 2;
            dprf(DIAG_COMBAT, "Stab damage vs %s: %d",
                 defender->name(DESC_PLAIN).c_str(),
                 damage);
        }

        if (cleaving)
            damage = cleave_damage_mod(damage);

        return apply_defender_ac(damage, damage_max, half_ac);
    }
    else
    {
        int potential_damage;

        potential_damage =
            !weapon ? calc_base_unarmed_damage()
                    : calc_base_weapon_damage();

        potential_damage = player_stat_modify_damage(potential_damage);

        damage_done =
            potential_damage > 0 ? one_chance_in(3) + random2(potential_damage) : 0;

        damage_done = player_apply_weapon_skill(damage_done);
        damage_done = player_apply_fighting_skill(damage_done, false);
        damage_done = player_apply_misc_modifiers(damage_done);
        damage_done = player_apply_slaying_bonuses(damage_done, false);
        damage_done = player_apply_weapon_bonuses(damage_done);
        damage_done = player_apply_final_multipliers(damage_done);

        damage_done = player_stab(damage_done);
        damage_done = apply_defender_ac(damage_done);

        set_attack_verb();
        damage_done = max(0, damage_done);

        return damage_done;
    }

    return 0;
}

int melee_attack::apply_defender_ac(int damage, int damage_max, bool half_ac)
{
    int stab_bypass = 0;
    if (stab_bonus)
    {
        stab_bypass = you.skill(wpn_skill, 50) + you.skill(SK_STEALTH, 50);
        stab_bypass = random2(div_rand_round(stab_bypass, 100 * stab_bonus));
    }
    int after_ac = defender->apply_ac(damage, damage_max,
                                      half_ac ? AC_HALF : AC_NORMAL,
                                      stab_bypass);
    dprf(DIAG_COMBAT, "AC: att: %s, def: %s, ac: %d, gdr: %d, dam: %d -> %d",
                 attacker->name(DESC_PLAIN, true).c_str(),
                 defender->name(DESC_PLAIN, true).c_str(),
                 defender->armour_class(),
                 defender->gdr_perc(),
                 damage,
                 after_ac);

    return after_ac;
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

    if (!_vamp_wants_blood_from_monster(mon) ||
        (!adjacent(defender->pos(), attacker->pos()) && needs_bite_msg))
    {
        return false;
    }

    const corpse_effect_type chunk_type = mons_corpse_effect(mon->type);

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
        int heal = 1 + random2(damage);
        if (chunk_type == CE_CLEAN)
            heal += 1 + random2(damage);
        if (heal > you.experience_level)
            heal = you.experience_level;

        // Decrease healing when done in bat form.
        if (you.form == TRAN_BAT)
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
        if (you.form == TRAN_BAT)
            food_value /= 2;

        food_value /= reduction;

        lessen_hunger(food_value, false);
    }

    did_god_conduct(DID_DRINK_BLOOD, 5 + random2(4));

    return true;
}

// weighted average of strength and dex, between str and (str+dex)/2
int melee_attack::calc_stat_to_dam_base()
{
    const int weight = weapon ? 10 - weapon_str_weight(*weapon) : 6;
    return you.strength() + (you.dex() - you.strength()) * weight / 20;
}

void melee_attack::stab_message()
{
    defender->props["helpless"] = true;

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

    defender->props.erase("helpless");
}

bool melee_attack::_vamp_wants_blood_from_monster(const monster* mon)
{
    if (you.species != SP_VAMPIRE)
        return false;

    if (you.hunger_state == HS_ENGORGED)
        return false;

    if (mon->is_summoned())
        return false;

    if (!mons_has_blood(mon->type))
        return false;

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
