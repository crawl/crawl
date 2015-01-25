/*
 *  File:       attack.cc
 *  Summary:    Methods of the attack class, generalized functions which may
 *              be overloaded by inheriting classes.
 *  Written by: Robert Burnham
 */

#include "AppHdr.h"

#include "attack.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "art-enum.h"
#include "delay.h"
#include "english.h"
#include "env.h"
#include "exercise.h"
#include "fight.h"
#include "fineff.h"
#include "godconduct.h"
#include "itemname.h"
#include "itemprop.h"
#include "message.h"
#include "misc.h"
#include "mon-behv.h"
#include "mon-clone.h"
#include "mon-death.h"
#include "mon-poly.h"
#include "spl-miscast.h"
#include "state.h"
#include "stepdown.h"
#include "stringutil.h"
#include "transform.h"
#include "xom.h"

/*
 **************************************************
 *             BEGIN PUBLIC FUNCTIONS             *
 **************************************************
*/
attack::attack(actor *attk, actor *defn, actor *blame)
    : attacker(attk), defender(defn), responsible(blame ? blame : attk),
      attack_occurred(false), cancel_attack(false), did_hit(false),
      needs_message(false), attacker_visible(false), defender_visible(false),
      perceived_attack(false), obvious_effect(false), to_hit(0),
      damage_done(0), special_damage(0), aux_damage(0), min_delay(0),
      final_attack_delay(0), special_damage_flavour(BEAM_NONE),
      stab_attempt(false), stab_bonus(0), apply_bleeding(false),
      ev_margin(0), weapon(nullptr),
      damage_brand(SPWPN_NORMAL), wpn_skill(SK_UNARMED_COMBAT),
      shield(nullptr), art_props(0), unrand_entry(nullptr),
      attacker_to_hit_penalty(0), attack_verb("bug"), verb_degree(),
      no_damage_message(), special_damage_message(), aux_attack(), aux_verb(),
      attacker_armour_tohit_penalty(0), attacker_shield_tohit_penalty(0),
      defender_shield(nullptr), miscast_level(-1), miscast_type(SPTYP_NONE),
      miscast_target(nullptr), fake_chaos_attack(false), simu(false),
      aux_source(""), kill_type(KILLED_BY_MONSTER)
{
    // No effective code should execute, we'll call init_attack again from
    // the child class, since initializing an attack will vary based the within
    // type of attack actually being made (melee, ranged, etc.)
}

bool attack::handle_phase_attempted()
{
    return true;
}

bool attack::handle_phase_blocked()
{
    damage_done = 0;
    return true;
}

bool attack::handle_phase_damaged()
{
    // We have to check in_bounds() because removed kraken tentacles are
    // temporarily returned to existence (without a position) when they
    // react to damage.
    if (defender->can_bleed()
        && !defender->is_summoned()
        && !defender->submerged()
        && in_bounds(defender->pos())
        && !simu)
    {
        int blood = damage_done;
        if (blood > defender->stat_hp())
            blood = defender->stat_hp();
        if (blood)
            blood_fineff::schedule(defender, defender->pos(), blood);
    }

    announce_hit();
    // Inflict stored damage
    damage_done = inflict_damage(damage_done);

    // TODO: Unify these, added here so we can get rid of player_attack
    if (attacker->is_player())
    {
        if (damage_done)
            player_exercise_combat_skills();

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
        if (!mons_attack_effects())
            return false;
    }

    // It's okay if a monster took lethal damage, but we should stop
    // the combat if it was already reset (e.g. a spectral weapon that
    // took damage and then noticed that its caster is gone).
    return defender->is_player() || !invalid_monster(defender->as_monster());
}

bool attack::handle_phase_killed()
{
    monster * const mon = defender->as_monster();
    if (!invalid_monster(mon))
        monster_die(mon, attacker);

    return true;
}

bool attack::handle_phase_end()
{
    // This may invalidate both the attacker and defender.
    fire_final_effects();

    return true;
}

/**
 * Calculate the to-hit for an attacker
 *
 * @param random If false, calculate average to-hit deterministically.
 */
int attack::calc_to_hit(bool random)
{
    int mhit = attacker->is_player() ?
                15 + (calc_stat_to_hit_base() / 2)
              : calc_mon_to_hit_base();

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
        if (using_weapon())
        {
            if (wpn_skill != SK_FIGHTING)
            {
                if (you.skill(wpn_skill) < 1 && player_in_a_dangerous_place())
                    xom_is_stimulated(10); // Xom thinks that is mildly amusing.

                mhit += maybe_random_div(you.skill(wpn_skill, 100), 100,
                                         random);
            }
        }
        else if (you.form_uses_xl())
            mhit += maybe_random_div(you.experience_level * 100, 100, random);
        else
        {                       // ...you must be unarmed or throwing
            // Members of clawed species have presumably been using the claws,
            // making them more practiced and thus more accurate in unarmed
            // combat. They keep this benefit even when the claws are covered
            // (or missing because of a change in form).
            mhit += species_has_claws(you.species)
                    && wpn_skill == SK_UNARMED_COMBAT ? 4 : 2;

            mhit += maybe_random_div(you.skill(wpn_skill, 100), 100,
                                     random);
        }

        // weapon bonus contribution
        if (using_weapon())
        {
            if (weapon->base_type == OBJ_WEAPONS)
            {
                mhit += weapon->plus;
                mhit += property(*weapon, PWPN_HIT);
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
        mhit += slaying_bonus(wpn_skill == SK_THROWING
                              || (weapon && is_range_weapon(*weapon)
                                         && using_weapon()));

        // horror penalty
        if (you.duration[DUR_HORROR])
            mhit -= you.props[HORROR_PENALTY_KEY].get_int();

        // hunger penalty
        if (you.hunger_state == HS_STARVING)
            mhit -= 3;

        // armour penalty
        mhit -= (attacker_armour_tohit_penalty + attacker_shield_tohit_penalty);

        // mutation
        if (player_mutation_level(MUT_EYEBALLS))
            mhit += 2 * player_mutation_level(MUT_EYEBALLS) + 1;

        // hit roll
        mhit = maybe_random2(mhit, random);

        if (using_weapon() && wpn_skill == SK_SHORT_BLADES
            && you.duration[DUR_SURE_BLADE])
        {
            int turn_duration = you.duration[DUR_SURE_BLADE] / BASELINE_DELAY;
            mhit += 5 +
                (random ? random2limit(turn_duration, 10) :
                 turn_duration / 2);
        }
    }
    else    // Monster to-hit.
    {
        if (using_weapon())
            mhit += weapon->plus + property(*weapon, PWPN_HIT);

        const int jewellery = attacker->as_monster()->inv[MSLOT_JEWELLERY];
        if (jewellery != NON_ITEM
            && mitm[jewellery].is_type(OBJ_JEWELLERY, RING_SLAYING))
        {
            mhit += mitm[jewellery].plus;
        }

        mhit += attacker->scan_artefacts(ARTP_SLAYING);

        if (using_weapon() && weapon->base_type == OBJ_RODS)
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

    if (using_weapon() && is_unrandom_artefact(*weapon, UNRAND_WOE))
        return AUTOMATIC_HIT;

    // If no defender, we're calculating to-hit for debug-display
    // purposes, so don't drop down to defender code below
    if (defender == nullptr)
        return mhit;

    if (!defender->visible_to(attacker))
        if (attacker->is_player())
            mhit -= 6;
        else
            mhit = mhit * 65 / 100;
    else
    {
        // This can only help if you're visible!
        const int how_transparent = player_mutation_level(MUT_TRANSLUCENT_SKIN);
        if (defender->is_player() && how_transparent)
            mhit -= 2 * how_transparent;

        if (defender->backlit(false))
            mhit += 2 + random2(8);
        else if (!attacker->nightvision()
                 && defender->umbra())
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

/* Returns an actor's name
 *
 * Takes into account actor visibility/invisibility and the type of description
 * to be used (capitalization, possessiveness, etc.)
 */
string attack::actor_name(const actor *a, description_level_type desc,
                          bool actor_visible)
{
    return actor_visible ? a->name(desc) : anon_name(desc);
}

/* Returns an actor's pronoun
 *
 * Takes into account actor visibility
 */
string attack::actor_pronoun(const actor *a, pronoun_type pron,
                             bool actor_visible)
{
    return actor_visible ? a->pronoun(pron) : anon_pronoun(pron);
}

/* Returns an anonymous actor's name
 *
 * Given the actor visible or invisible, returns the
 * appropriate possessive pronoun.
 */
string attack::anon_name(description_level_type desc)
{
    switch (desc)
    {
    case DESC_NONE:
        return "";
    case DESC_YOUR:
    case DESC_ITS:
        return "its";
    case DESC_THE:
    case DESC_A:
    case DESC_PLAIN:
    default:
        return "something";
    }
}

/* Returns an anonymous actor's pronoun
 *
 * Given invisibility (whether out of LOS or just invisible), returns the
 * appropriate possessive, inflexive, capitalised pronoun.
 */
string attack::anon_pronoun(pronoun_type pron)
{
    return decline_pronoun(GENDER_NEUTER, pron);
}

/* Initializes an attack, setting up base variables and values
 *
 * Does not make any changes to any actors, items, or the environment,
 * in case the attack is cancelled or otherwise fails. Only initializations
 * that are universal to all types of attacks should go into this method,
 * any initialization properties that are specific to one attack or another
 * should go into their respective init_attack.
 *
 * Although this method will get overloaded by the derived class, we are
 * calling it from attack::attack(), before the overloading has taken place.
 */
void attack::init_attack(skill_type unarmed_skill, int attack_number)
{
    weapon          = attacker->weapon(attack_number);

    wpn_skill       = weapon ? item_attack_skill(*weapon) : unarmed_skill;
    if (attacker->is_player() && you.form_uses_xl())
        wpn_skill = SK_FIGHTING; // for stabbing, mostly

    attacker_armour_tohit_penalty =
        div_rand_round(attacker->armour_tohit_penalty(true, 20), 20);
    attacker_shield_tohit_penalty =
        div_rand_round(attacker->shield_tohit_penalty(true, 20), 20);
    to_hit          = calc_to_hit(true);

    shield = attacker->shield();
    defender_shield = defender ? defender->shield() : defender_shield;

    if (weapon && weapon->base_type == OBJ_WEAPONS && is_artefact(*weapon))
    {
        artefact_properties(*weapon, art_props);
        if (is_unrandom_artefact(*weapon))
            unrand_entry = get_unrand_entry(weapon->special);
    }

    attacker_visible   = attacker->observable();
    defender_visible   = defender && defender->observable();
    needs_message      = (attacker_visible || defender_visible);

    if (attacker->is_monster())
    {
        mon_attack_def mon_attk = mons_attack_spec(attacker->as_monster(),
                                                   attack_number);

        attk_type       = mon_attk.type;
        attk_flavour    = mon_attk.flavour;

        // Don't scale damage for YOU_FAULTLESS etc.
        if (attacker->get_experience_level() == 0)
            attk_damage = mon_attk.damage;
        else
        {
            attk_damage = div_rand_round(mon_attk.damage
                                             * attacker->get_hit_dice(),
                                         attacker->get_experience_level());
        }

        if (attk_type == AT_WEAP_ONLY)
        {
            int weap = attacker->as_monster()->inv[MSLOT_WEAPON];
            if (weap == NON_ITEM || is_range_weapon(mitm[weap]))
                attk_type = AT_NONE;
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
}

void attack::alert_defender()
{
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
        && !attacker->as_monster()->wont_attack())
    {
        if (defender->is_player())
            interrupt_activity(AI_MONSTER_ATTACKS, attacker->as_monster());
        if (you.pet_target == MHITNOT && env.sanctuary_time <= 0)
            you.pet_target = attacker->mindex();
    }
}

bool attack::distortion_affects_defender()
{
    //jmf: blink frogs *like* distortion
    // I think could be amended to let blink frogs "grow" like
    // jellies do {dlb}
    if (defender->is_monster()
        && mons_genus(defender->type) == MONS_BLINK_FROG)
    {
        if (one_chance_in(5))
        {
            if (defender_visible)
            {
                special_damage_message =
                    make_stringf("%s %s in the distortional energy.",
                                 defender_name(false).c_str(),
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
            defender_name(false).c_str());
        special_damage += 1 + random2avg(7, 2);
        return false;
    }

    if (one_chance_in(3))
    {
        special_damage_message =
            make_stringf("Space warps horribly around %s!",
                         defender_name(false).c_str());
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
            blink_fineff::schedule(defender);
        return false;
    }

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
            distortion_tele_fineff::schedule(defender);
        else
            defender->teleport();
        return false;
    }

    if (!player_in_branch(BRANCH_ABYSS) && coinflip())
    {
        if (defender->is_player() && attacker_visible
            && using_weapon()
            && !is_unrandom_artefact(*weapon))
        {
            // If the player is being sent to the Abyss by being attacked
            // with a distortion weapon, then we have to ID it before
            // the player goes to Abyss, while the weapon object is
            // still in memory.
            if (is_artefact(*weapon))
                artefact_learn_prop(*weapon, ARTP_BRAND);
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

void attack::antimagic_affects_defender(int pow)
{
    obvious_effect =
        enchant_actor_with_flavour(defender, nullptr, BEAM_DRAIN_MAGIC, pow);
}

void attack::pain_affects_defender()
{
    if (defender->res_negative_energy())
        return;

    if (!one_chance_in(attacker->skill_rdiv(SK_NECROMANCY) + 1))
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
void attack::chaos_affects_defender()
{
    monster * const mon   = defender->as_monster();
    const bool firewood   = mon && mons_is_firewood(mon);
    const bool immune     = mon && mons_immune_magic(mon);
    const bool is_natural = mon && defender->holiness() == MH_NATURAL;
    const bool is_shifter = mon && mon->is_shapeshifter();
    const bool can_clone  = mon && mons_clonable(mon, true);
    const bool can_poly   = is_shifter || (defender->can_safely_mutate()
                                           && !immune && !firewood);
    const bool can_rage   = defender->can_go_berserk();
    const bool can_slow   = !firewood;
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
        if (!fake_chaos_attack && you.see_cell(defender->pos()))
        {
            if (defender->is_player())
                mpr("You give off a flash of multicoloured light!");
            else if (you.can_see(defender))
            {
                simple_monster_message(mon, " gives off a flash of"
                                            " multicoloured light!");
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

        10,             // CHAOS_HEAL
        slowpara_chance,// CHAOS_HASTE
        10,             // CHAOS_INVIS

        slowpara_chance,// CHAOS_SLOW
        slowpara_chance,// CHAOS_PARALYSIS
        petrify_chance, // CHAOS_PETRIFY
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

        if (monster *clone = clone_mons(mon, true, &obvious_effect))
        {
            if (obvious_effect)
            {
                special_damage_message =
                    make_stringf("%s is duplicated!",
                                 defender_name(false).c_str());
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
        monster_polymorph(mon, RANDOM_MONSTER, PPT_MORE);
        break;

    case CHAOS_MAKE_SHIFTER:
    {
        ASSERT(can_poly);
        ASSERT(shifter_chance > 0);
        ASSERT(!is_shifter);
        ASSERT(defender->is_monster());

        obvious_effect = you.can_see(defender);
        mon->add_ench(one_chance_in(3) ? ENCH_GLOWING_SHAPESHIFTER
                                       : ENCH_SHAPESHIFTER);
        // Immediately polymorph monster, just to make the effect obvious.
        monster_polymorph(mon, RANDOM_MONSTER);

        // Xom loves it if this happens!
        const int friend_factor = mon->friendly() ? 1 : 2;
        const int glow_factor   = mon->has_ench(ENCH_SHAPESHIFTER) ? 1 : 2;
        xom_is_stimulated(64 * friend_factor * glow_factor);
        break;
    }
    case CHAOS_MISCAST:
    {
        int level = defender->get_hit_dice();

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
        miscast_target = defender;
        break;
    }

    case CHAOS_RAGE:
        ASSERT(can_rage);
        ASSERT(rage_chance > 0);
        defender->go_berserk(false);
        obvious_effect = you.can_see(defender);
        break;

    // For these, obvious_effect is computed during beam.fire() below.
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
        // Wielded brand is always known, but maybe this was from a call
        // to chaos_affect_actor.
        beam.effect_wanton = !fake_chaos_attack;

        if (using_weapon() && you.can_see(attacker))
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

        beam.source_id = attacker->mid;

        beam.source = defender->pos();
        beam.target = defender->pos();

        beam.damage = dice_def(damage_done + special_damage + aux_damage, 1);

        beam.ench_power = beam.damage.num;

        const bool you_could_see = you.can_see(defender);
        beam.fire();

        if (you_could_see)
            obvious_effect = beam.obvious_effect;
    }

    if (!you.can_see(attacker))
        obvious_effect = false;
}

// NOTE: random_chaos_brand() and random_chaos_attack_flavour() should
// return a set of effects that are roughly the same, to make it easy
// for chaos_affects_defender() not to do duplicate effects caused
// by the non-chaos brands/flavours they return.
brand_type attack::random_chaos_brand()
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
                     5, SPWPN_VAMPIRISM,
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
        case SPWPN_VAMPIRISM:
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
            if (!defender->holy_wrath_susceptible())
                susceptible = false;
            break;
        case SPWPN_CONFUSE:
            if (defender->holiness() == MH_NONLIVING
                || defender->holiness() == MH_PLANT)
            {
                susceptible = false;
            }
            break;
        case SPWPN_ANTIMAGIC:
            if (!defender->antimagic_susceptible())
                susceptible = false;
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
    case SPWPN_VAMPIRISM:     brand_name += "vampirism"; break;
    case SPWPN_VORPAL:          brand_name += "vorpal"; break;
    case SPWPN_ANTIMAGIC:       brand_name += "antimagic"; break;

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

void attack::do_miscast()
{
    if (miscast_level == -1)
        return;

    ASSERT(miscast_target != nullptr);
    ASSERT_RANGE(miscast_level, 0, 4);
    ASSERT(count_bits(miscast_type) == 1);

    if (!miscast_target->alive())
        return;

    if (miscast_target->is_player() && you.banished)
        return;

    const bool chaos_brand =
        using_weapon() && get_weapon_brand(*weapon) == SPWPN_CHAOS;

    // If the miscast is happening on the attacker's side and is due to
    // a chaos weapon then make smoke/sand/etc pour out of the weapon
    // instead of the attacker's hands.
    string hand_str;

    string cause = atk_name(DESC_THE);

    const int ignore_mask = ISFLAG_KNOW_CURSE | ISFLAG_KNOW_PLUSES;

    if (attacker->is_player())
    {
        if (chaos_brand)
        {
            cause = "a chaos effect from ";
            // Ignore a lot of item flags to make cause as short as possible,
            // so it will (hopefully) fit onto a single line in the death
            // cause screen.
            cause += wep_name(DESC_YOUR, ignore_mask | ISFLAG_COSMETIC_MASK);

            if (miscast_target == attacker)
                hand_str = wep_name(DESC_PLAIN, ignore_mask);
        }
    }
    else
    {
        if (chaos_brand && miscast_target == attacker
            && you.can_see(attacker))
        {
            hand_str = wep_name(DESC_PLAIN, ignore_mask);
        }
    }

    MiscastEffect(miscast_target, attacker, MELEE_MISCAST,
                  (spschool_flag_type) miscast_type, miscast_level, cause,
                  NH_NEVER, 0, hand_str, false);

    // Don't do miscast twice for one attack.
    miscast_level = -1;
}

void attack::drain_defender()
{
    if (defender->is_monster() && coinflip())
        return;

    if (defender->holiness() != MH_NATURAL)
        return;

    special_damage = resist_adjust_damage(defender, BEAM_NEG,
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
                    defender_name(true).c_str());
        }

        attacker->god_conduct(DID_NECROMANCY, 2);
    }
}

void attack::drain_defender_speed()
{
    if (!defender->res_negative_energy())
    {
        if (needs_message)
        {
            mprf("%s %s %s vigour!",
                 atk_name(DESC_THE).c_str(),
                 attacker->conj_verb("drain").c_str(),
                 def_name(DESC_ITS).c_str());
        }

        special_damage = 1 + random2(damage_done) / 2;
        defender->slow_down(attacker, 5 + random2(7));
    }
}

int attack::inflict_damage(int dam, beam_type flavour, bool clean)
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
    return defender->hurt(responsible, dam, flavour, kill_type,
                          "", aux_source.c_str(), clean);
}

/* If debug, return formatted damage done
 *
 */
string attack::debug_damage_number()
{
#ifdef DEBUG_DIAGNOSTICS
    return make_stringf(" for %d", damage_done);
#else
    return "";
#endif
}

/* Returns standard attack punctuation
 *
 * Used in player / monster (both primary and aux) attacks
 */
string attack::attack_strength_punctuation(int dmg)
{
    if (dmg < HIT_WEAK)
        return ".";
    else if (dmg < HIT_MED)
        return "!";
    else if (dmg < HIT_STRONG)
        return "!!";
    else
    {
        string ret = "!!!";
        int tmpdamage = dmg;
        while (tmpdamage >= 2*HIT_STRONG)
        {
            ret += "!";
            tmpdamage >>= 1;
        }
        return ret;
    }
}

/* Returns evasion adverb
 *
 */
string attack::evasion_margin_adverb()
{
    return (ev_margin <= -20) ? " completely" :
           (ev_margin <= -12) ? "" :
           (ev_margin <= -6)  ? " closely"
                              : " barely";
}

void attack::stab_message()
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

/* Returns the attacker's name
 *
 * Helper method to easily access the attacker's name
 */
string attack::atk_name(description_level_type desc)
{
    return actor_name(attacker, desc, attacker_visible);
}

/* Returns the defender's name
 *
 * Helper method to easily access the defender's name
 */
string attack::def_name(description_level_type desc)
{
    return actor_name(defender, desc, defender_visible);
}

/* Returns the attacking weapon's name
 *
 * Sets upthe appropriate descriptive level and obtains the name of a weapon
 * based on if the attacker is a player or non-player (non-players use a
 * plain name and a manually entered pronoun)
 */
string attack::wep_name(description_level_type desc, iflags_t ignre_flags)
{
    ASSERT(weapon != nullptr);

    if (attacker->is_player())
        return weapon->name(desc, false, false, false, false, ignre_flags);

    string name;
    bool possessive = false;
    if (desc == DESC_YOUR)
    {
        desc       = DESC_THE;
        possessive = true;
    }

    if (possessive)
        name = apostrophise(atk_name(desc)) + " ";

    name += weapon->name(DESC_PLAIN, false, false, false, false, ignre_flags);

    return name;
}

/* TODO: Remove this!
 * Removing it may not really be practical, in retrospect. Its only used
 * below, in calc_elemental_brand_damage, which is called for both frost and
 * flame brands for both players and monsters.
 */
string attack::defender_name(bool allow_reflexive)
{
    if (allow_reflexive && attacker == defender)
        return actor_pronoun(attacker, PRONOUN_REFLEXIVE, attacker_visible);
    else
        return def_name(DESC_THE);
}

int attack::player_stat_modify_damage(int damage)
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

int attack::player_apply_weapon_skill(int damage)
{
    if (using_weapon())
    {
        damage *= 2500 + (random2(you.skill(wpn_skill, 100) + 1));
        damage /= 2500;
    }

    return damage;
}

int attack::player_apply_fighting_skill(int damage, bool aux)
{
    const int base = aux? 40 : 30;

    damage *= base * 100 + (random2(you.skill(SK_FIGHTING, 100) + 1));
    damage /= base * 100;

    return damage;
}

int attack::player_apply_misc_modifiers(int damage)
{
    return damage;
}

/**
 * Get the damage bonus from a weapon's enchantment.
 */
int attack::get_weapon_plus()
{
    if (weapon->base_type == OBJ_RODS)
        return weapon->special;
    if (weapon->base_type == OBJ_STAVES || weapon->sub_type == WPN_BLOWGUN)
        return 0;
    return weapon->plus;
}

// Slaying and weapon enchantment. Apply this for slaying even if not
// using a weapon to attack.
int attack::player_apply_slaying_bonuses(int damage, bool aux)
{
    int damage_plus = 0;
    if (!aux && using_weapon())
    {
        damage_plus = get_weapon_plus();
        if (you.duration[DUR_CORROSION])
            damage_plus -= 3 * you.props["corrosion_amount"].get_int();
    }
    damage_plus += slaying_bonus(!weapon && wpn_skill == SK_THROWING
                                 || (weapon && is_range_weapon(*weapon)
                                            && using_weapon()));

    damage += (damage_plus > -1) ? (random2(1 + damage_plus))
                                 : (-random2(1 - damage_plus));
    return damage;
}

int attack::player_apply_final_multipliers(int damage)
{
    // Can't affect much of anything as a shadow.
    if (you.form == TRAN_SHADOW)
        damage = div_rand_round(damage, 2);

    return damage;
}

void attack::player_exercise_combat_skills()
{
}

/* Returns attacker base unarmed damage
 *
 * Scales for current mutations and unarmed effects
 * TODO: Complete symmetry for base_unarmed damage
 * between monsters and players.
 */
int attack::calc_base_unarmed_damage()
{
    // Should only get here if we're not wielding something that's a weapon.
    // If there's a non-weapon in hand, it has no base damage.
    if (weapon)
        return 0;

    if (!attacker->is_player())
        return 0;

    int damage = get_form()->get_base_unarmed_damage();

    if (you.has_usable_claws())
    {
        // Claw damage only applies for bare hands.
        damage += you.has_claws(false) * 2;
        apply_bleeding = true;
    }

    if (you.form_uses_xl())
        damage += you.experience_level;
    else if (you.form == TRAN_BAT || you.form == TRAN_PORCUPINE)
    {
        // Bats really don't do a lot of damage.
        damage += you.skill_rdiv(wpn_skill, 1, 5);
    }
    else
        damage += you.skill_rdiv(wpn_skill);

    if (damage < 0)
        damage = 0;

    return damage;
}

// TODO: Potentially remove, consider integrating with other to-hit or stat
// calculating methods
// weighted average of strength and dex, between (str+dex)/2 and dex
int attack::calc_stat_to_hit_base()
{
    const int weight = weapon ? weapon_str_weight(*weapon) : 4;

    // dex is modified by strength towards the average, by the
    // weighted amount weapon_str_weight() / 20.
    return you.dex() + (you.strength() - you.dex()) * weight / 20;
}

// weighted average of strength and dex, between str and (str+dex)/2
int attack::calc_stat_to_dam_base()
{
    const int weight = weapon ? 10 - weapon_str_weight(*weapon) : 6;
    return you.strength() + (you.dex() - you.strength()) * weight / 20;
}

int attack::calc_damage()
{
    if (attacker->is_monster())
    {
        int damage = 0;
        int damage_max = 0;
        if (using_weapon() || wpn_skill == SK_THROWING)
        {
            damage_max = weapon_damage();
            damage += random2(damage_max);

            int wpn_damage_plus = 0;
            if (weapon) // can be 0 for throwing projectiles
                wpn_damage_plus = get_weapon_plus();

            const int jewellery = attacker->as_monster()->inv[MSLOT_JEWELLERY];
            if (jewellery != NON_ITEM
                && mitm[jewellery].is_type(OBJ_JEWELLERY, RING_SLAYING))
            {
                wpn_damage_plus += mitm[jewellery].plus;
            }

            wpn_damage_plus += attacker->scan_artefacts(ARTP_SLAYING);

            if (wpn_damage_plus >= 0)
                damage += random2(wpn_damage_plus);
            else
                damage -= random2(1 - wpn_damage_plus);

            damage -= 1 + random2(3);
        }

        damage_max += attk_damage;
        damage     += 1 + random2(attk_damage);

        bool half_ac = false;
        damage = apply_damage_modifiers(damage, damage_max, half_ac);

        set_attack_verb();
        return apply_defender_ac(damage, damage_max, half_ac);
    }
    else
    {
        int potential_damage;

        potential_damage =
            using_weapon() || wpn_skill == SK_THROWING
            ? weapon_damage()
            : calc_base_unarmed_damage();

        potential_damage = player_stat_modify_damage(potential_damage);

        damage_done = random2(potential_damage+1);

        damage_done = player_apply_weapon_skill(damage_done);
        damage_done = player_apply_fighting_skill(damage_done, false);
        damage_done = player_apply_misc_modifiers(damage_done);
        damage_done = player_apply_slaying_bonuses(damage_done, false);
        damage_done = player_stab(damage_done);
        // A failed stab may have awakened monsters, but that could have
        // caused the defender to cease to exist (spectral weapons with
        // missing summoners; or pacified monsters on a stair).  FIXME:
        // The correct thing to do would be either to delay the call to
        // alert_nearby_monsters (currently in player_stab) until later
        // in the attack; or to avoid removing monsters in handle_behaviour.
        if (!defender->alive())
            return 0;
        damage_done = player_apply_final_multipliers(damage_done);
        damage_done = apply_defender_ac(damage_done);

        set_attack_verb();
        damage_done = max(0, damage_done);

        return damage_done;
    }

    return 0;
}

int attack::test_hit(int to_land, int ev, bool randomise_ev)
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

int attack::apply_defender_ac(int damage, int damage_max, bool half_ac)
{
    ASSERT(defender);
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

/* Determine whether a block occurred
 *
 * No blocks if defender is incapacitated, would be nice to eventually expand
 * this method to handle partial blocks as well as full blocks (although this
 * would serve as a nerf to shields and - while more realistic - may not be a
 * good mechanic for shields.
 *
 * Returns (block_occurred)
 */
bool attack::attack_shield_blocked(bool verbose)
{
    if (defender == attacker)
        return false; // You can't block your own attacks!

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

        if (attack_ignores_shield(verbose))
            return false;

        if (needs_message && verbose)
        {
            mprf("%s %s %s attack.",
                 defender_name(false).c_str(),
                 defender->conj_verb("block").c_str(),
                 attacker == defender ? "its own"
                                      : atk_name(DESC_ITS).c_str());
        }

        defender->shield_block_succeeded(attacker);

        return true;
    }

    return false;
}

attack_flavour attack::random_chaos_attack_flavour()
{
    attack_flavour flavours[] =
        {AF_FIRE, AF_COLD, AF_ELEC, AF_POISON, AF_VAMPIRIC, AF_DISTORT,
         AF_CONFUSE, AF_CHAOS};
    return RANDOM_ELEMENT(flavours);
}

bool attack::apply_damage_brand(const char *what)
{
    bool brand_was_known = false;
    int brand = 0;
    bool ret = false;

    if (using_weapon())
    {
        if (is_artefact(*weapon))
            brand_was_known = artefact_known_property(*weapon, ARTP_BRAND);
        else
            brand_was_known = item_type_known(*weapon);
    }

    special_damage = 0;
    obvious_effect = false;
    brand = damage_brand == SPWPN_CHAOS ? random_chaos_brand() : damage_brand;

    if (brand != SPWPN_FLAMING && brand != SPWPN_FREEZING
        && brand != SPWPN_ELECTROCUTION && brand != SPWPN_VAMPIRISM
        && !defender->alive())
    {
        // Most brands have no extra effects on just killed enemies, and the
        // effect would be often inappropriate.
        return false;
    }

    if (!damage_done
        && (brand == SPWPN_FLAMING || brand == SPWPN_FREEZING
            || brand == SPWPN_HOLY_WRATH || brand == SPWPN_ANTIMAGIC
            || brand == SPWPN_VORPAL || brand == SPWPN_VAMPIRISM))
    {
        // These brands require some regular damage to function.
        return false;
    }

    switch (brand)
    {
    case SPWPN_FLAMING:
        calc_elemental_brand_damage(BEAM_FIRE,
                                    defender->is_icy() ? "melt" : "burn",
                                    what);
        attacker->god_conduct(DID_FIRE, 1);
        break;

    case SPWPN_FREEZING:
        calc_elemental_brand_damage(BEAM_COLD, "freeze", what);
        break;

    case SPWPN_HOLY_WRATH:
        if (defender->holy_wrath_susceptible())
            special_damage = 1 + (random2(damage_done * 15) / 10);

        if (special_damage && defender_visible)
        {
            special_damage_message =
                make_stringf(
                    "%s %s%s",
                    defender_name(false).c_str(),
                    defender->conj_verb("convulse").c_str(),
                    attack_strength_punctuation(special_damage).c_str());
        }
        break;

    case SPWPN_ELECTROCUTION:
        if (defender->res_elec() > 0)
            break;
        else if (one_chance_in(3))
        {
            special_damage_message =
                defender->is_player()?
                   "You are electrocuted!"
                :  "There is a sudden explosion of sparks!";
            special_damage = 8 + random2(13);
            special_damage_flavour = BEAM_ELECTRICITY;
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

            defender->poison(attacker, 6 + random2(8) + random2(damage_done * 3 / 2));

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

    case SPWPN_VAMPIRISM:
    {
        if (!weapon
            || defender->holiness() != MH_NATURAL
            || defender->res_negative_energy()
            || damage_done < 1
            || attacker->stat_hp() == attacker->stat_maxhp()
            || !defender->is_player()
               && defender->as_monster()->is_summoned()
            || attacker->is_player() && you.duration[DUR_DEATHS_DOOR]
            || !attacker->is_player()
               && attacker->as_monster()->has_ench(ENCH_DEATHS_DOOR)
            || x_chance_in_y(2, 5) && !is_unrandom_artefact(*weapon, UNRAND_LEECH))
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

        int hp_boost = is_unrandom_artefact(*weapon, UNRAND_VAMPIRES_TOOTH)
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

        // Also used for players in fungus form.
        if (attacker->is_player()
            && you.form == TRAN_FUNGUS
            && !you.duration[DUR_CONFUSING_TOUCH]
            && defender->is_unbreathing())
        {
            break;
        }

        const int hdcheck =
            (defender->holiness() == MH_NATURAL ? random2(30) : random2(22));

        if (!mons_class_is_confusable(defender->type)
            || hdcheck < defender->get_hit_dice()
            || one_chance_in(5)
            || defender->as_monster()->check_clarity(false))
        {
            break;
        }

        // Declaring these just to pass to the enchant function.
        bolt beam_temp;
        beam_temp.thrower   = attacker->is_player() ? KILL_YOU : KILL_MON;
        beam_temp.flavour   = BEAM_CONFUSION;
        beam_temp.source_id = attacker->mid;
        beam_temp.apply_enchantment_to_monster(defender->as_monster());
        obvious_effect = beam_temp.obvious_effect;

        if (attacker->is_player() && damage_brand == SPWPN_CONFUSE
            && you.duration[DUR_CONFUSING_TOUCH])
        {
            you.duration[DUR_CONFUSING_TOUCH] = 1;
            obvious_effect = false;
        }
        break;
    }

    case SPWPN_CHAOS:
        chaos_affects_defender();
        break;

    case SPWPN_ANTIMAGIC:
        antimagic_affects_defender(damage_done * 8);
        break;

    default:
        if (using_weapon() && is_unrandom_artefact(*weapon, UNRAND_HELLFIRE))
        {
            calc_elemental_brand_damage(BEAM_HELLFIRE,
                                        defender->is_icy() ? "melt" : "burn",
                                        what);
            defender->expose_to_element(BEAM_HELLFIRE);
            attacker->god_conduct(DID_UNHOLY, 2 + random2(3));
            attacker->god_conduct(DID_FIRE, 10 + random2(5));
        }
        break;
    }

    if (damage_brand == SPWPN_CHAOS)
    {
        if (brand != SPWPN_CHAOS && !ret
            && miscast_level == -1 && one_chance_in(20))
        {
            miscast_level  = 0;
            miscast_type   = SPTYP_RANDOM;
            miscast_target = coinflip() ? attacker : defender;
        }

        if (responsible->is_player())
            did_god_conduct(DID_CHAOS, 2 + random2(3), brand_was_known);
    }

    if (!obvious_effect)
        obvious_effect = !special_damage_message.empty();

    if (needs_message && !special_damage_message.empty())
    {
        mpr(special_damage_message);

        special_damage_message.clear();
        // Don't do message-only miscasts along with a special
        // damage message.
        if (miscast_level == 0)
            miscast_level = -1;
    }

    if (special_damage > 0)
        inflict_damage(special_damage, special_damage_flavour);

    if (defender->alive())
    {
        switch (brand)
        {
        case SPWPN_FLAMING:
            defender->expose_to_element(BEAM_FIRE);
            break;

        case SPWPN_FREEZING:
            defender->expose_to_element(BEAM_COLD, 2);
            break;
        default:
            break;
        }
    }

    if (obvious_effect && attacker_visible && using_weapon())
    {
        if (is_artefact(*weapon))
            artefact_learn_prop(*weapon, ARTP_BRAND);
        else
            set_ident_flags(*weapon, ISFLAG_KNOW_TYPE);
    }

    return ret;
}

/* Calculates special damage, prints appropriate combat text
 *
 * Applies a particular damage brand to the current attack, the setup and
 * calculation of base damage and other effects varies based on the type
 * of attack, but the calculation of elemental damage should be consistent.
 */
void attack::calc_elemental_brand_damage(beam_type flavour,
                                         const char *verb,
                                         const char *what)
{
    special_damage = resist_adjust_damage(defender, flavour,
                                          random2(damage_done) / 2 + 1);

    if (needs_message && special_damage > 0 && verb)
    {
        // XXX: assumes "what" is singular
        special_damage_message = make_stringf(
            "%s %s %s%s",
            what ? what : atk_name(DESC_THE).c_str(),
            what ? conjugate_verb(verb, false).c_str()
                 : attacker->conj_verb(verb).c_str(),
            // Don't allow reflexive if the subject wasn't the attacker.
            defender_name(!what).c_str(),
            attack_strength_punctuation(special_damage).c_str());
    }
}

int attack::player_stab_weapon_bonus(int damage)
{
    int stab_skill = you.skill(wpn_skill, 50) + you.skill(SK_STEALTH, 50);

    if (player_good_stab())
    {
        // We might be unarmed if we're using the boots of the Assassin.
        const bool extra_good = using_weapon() && weapon->sub_type == WPN_DAGGER;
        int bonus = you.dex() * (stab_skill + 100) / (extra_good ? 500 : 1000);

        bonus   = stepdown_value(bonus, 10, 10, 30, 30);
        damage += bonus;
        damage *= 10 + div_rand_round(stab_skill, 100 * stab_bonus);
        damage /= 10;
    }

    damage *= 12 + div_rand_round(stab_skill, 100 * stab_bonus);
    damage /= 12;

    return damage;
}

int attack::player_stab(int damage)
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

/* Check for stab and prepare combat for stab-values
 *
 * Grant an automatic stab if paralyzed or sleeping (with highest damage value)
 * stab_bonus is used as the divisor in damage calculations, so lower values
 * will yield higher damage. Normal stab chance is (stab_skill + dex + 1 / roll)
 * This averages out to about 1/3 chance for a non extended-endgame stabber.
 */
void attack::player_stab_check()
{
    if (you.stat_zero[STAT_DEX] || you.confused())
    {
        stab_attempt = false;
        stab_bonus = 0;
        return;
    }

    const stab_type st = find_stab_type(&you, defender);
    stab_attempt = (st != STAB_NO_STAB);
    const bool roll_needed = (st != STAB_SLEEPING && st != STAB_PARALYSED);

    int roll = 100;
    if (st == STAB_INVISIBLE)
        roll -= 10;

    switch (st)
    {
    case STAB_NO_STAB:
    case NUM_STAB:
        stab_bonus = 0;
        break;
    case STAB_SLEEPING:
    case STAB_PARALYSED:
        stab_bonus = 1;
        break;
    case STAB_HELD_IN_NET:
    case STAB_PETRIFYING:
    case STAB_PETRIFIED:
        stab_bonus = 2;
        break;
    case STAB_INVISIBLE:
    case STAB_CONFUSED:
    case STAB_FLEEING:
    case STAB_ALLY:
        stab_bonus = 4;
        break;
    case STAB_DISTRACTED:
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
        count_action(CACT_STAB, st);
}
