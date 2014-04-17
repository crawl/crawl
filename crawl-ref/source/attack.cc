/*
 *  File:       attack.cc
 *  Summary:    Methods of the attack class, generalized functions which may
 *              be overloaded by inheriting classes.
 *  Written by: Robert Burnham
 */

#include "AppHdr.h"

#include "attack.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <algorithm>

#include "art-enum.h"
#include "delay.h"
#include "exercise.h"
#include "externs.h"
#include "enum.h"
#include "env.h"
#include "fight.h"
#include "fineff.h"
#include "itemprop.h"
#include "misc.h"
#include "mon-behv.h"
#include "mon-death.h"
#include "monster.h"
#include "libutil.h"
#include "player.h"
#include "state.h"
#include "stuff.h"
#include "xom.h"

/*
 **************************************************
 *             BEGIN PUBLIC FUNCTIONS             *
 **************************************************
*/
attack::attack(actor *attk, actor *defn)
    : attacker(attk), defender(defn), attack_occurred(false),
    cancel_attack(false), did_hit(false), needs_message(false),
    attacker_visible(false), defender_visible(false),
    perceived_attack(false), obvious_effect(false), to_hit(0),
    damage_done(0), special_damage(0), aux_damage(0), min_delay(0),
    final_attack_delay(0), special_damage_flavour(BEAM_NONE),
    stab_attempt(false), stab_bonus(0), apply_bleeding(false), noise_factor(0),
    ev_margin(0), weapon(NULL),
    damage_brand(SPWPN_NORMAL), wpn_skill(SK_UNARMED_COMBAT),
    shield(NULL), art_props(0), unrand_entry(NULL), attacker_to_hit_penalty(0),
    attack_verb("bug"), verb_degree(), no_damage_message(),
    special_damage_message(), aux_attack(), aux_verb(),
    defender_body_armour_penalty(0), defender_shield_penalty(0),
    attacker_body_armour_penalty(0), attacker_shield_penalty(0),
    attacker_armour_tohit_penalty(0), attacker_shield_tohit_penalty(0),
    defender_shield(NULL), aux_source(""), kill_type(KILLED_BY_MONSTER)
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
        && in_bounds(defender->pos()))
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

    return true;
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

/* Calculate the to-hit for an attacker
 *
 * @param random deterministic or stochastic calculation(s)
 */
int attack::calc_to_hit(bool random)
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
        else if (form_uses_xl())
            mhit += maybe_random_div(you.experience_level * 100, 100, random);
        else
        {                       // ...you must be unarmed
            // Members of clawed species have presumably been using the claws,
            // making them more practiced and thus more accurate in unarmed
            // combat. They keep this benefit even when the claws are covered
            // (or missing because of a change in form).
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
    switch (pron)
    {
    default:
    case PRONOUN_SUBJECTIVE:    return "it";
    case PRONOUN_POSSESSIVE:    return "its";
    case PRONOUN_REFLEXIVE:     return "itself";
    }
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
    damage_brand    = attacker->damage_brand(attack_number);

    wpn_skill       = weapon ? weapon_skill(*weapon) : unarmed_skill;
    if (form_uses_xl())
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
        artefact_wpn_properties(*weapon, art_props);
        if (is_unrandom_artefact(*weapon))
            unrand_entry = get_unrand_entry(weapon->special);
    }

    attacker_visible   = attacker->observable();
    defender_visible   = defender && defender->observable();
    needs_message      = (attacker_visible || defender_visible);

    attacker_body_armour_penalty = attacker->adjusted_body_armour_penalty(20);
    attacker_shield_penalty = attacker->adjusted_shield_penalty(20);

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
    if (defender->is_player())
    {
        ouch(dam, defender->mindex(), kill_type, aux_source.c_str(),
             you.can_see(attacker));
        return dam;
    }
    return defender->hurt(attacker, dam, flavour, clean);
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

/* Returns special punctuation
 *
 * Used (mostly) for elemental or branded attacks (napalm, dragon slaying, orc
 * slaying, holy, etc.)
 */
string attack::special_attack_punctuation()
{
    if (special_damage < 6)
        return ".";
    else
        return "!";
}

/* Returns standard attack punctuation
 *
 * Used in player / monster (both primary and aux) attacks
 */
string attack::attack_strength_punctuation()
{
    if (attacker->is_player())
        return get_exclams(damage_done);
    else
        return damage_done < HIT_WEAK ? "." : "!";
}

string attack::get_exclams(int dmg)
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
    ASSERT(weapon != NULL);

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
string attack::defender_name()
{
    if (attacker == defender)
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

// Slaying and weapon enchantment. Apply this for slaying even if not
// using a weapon to attack.
int attack::player_apply_slaying_bonuses(int damage, bool aux)
{
    int damage_plus = 0;
    if (!aux && weapon && using_weapon())
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

int attack::player_apply_final_multipliers(int damage)
{
    return damage;
}

void attack::player_exercise_combat_skills()
{
}

int attack::calc_base_unarmed_damage()
{
    return 0;
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
        if (using_weapon())
        {
            damage_max = weapon_damage();
            damage += random2(damage_max);

            int wpn_damage_plus = 0;
            if (weapon) // can be 0 for throwing projectiles
            {
                wpn_damage_plus = (weapon->base_type == OBJ_RODS)
                                  ? weapon->special
                                  : weapon->plus2;
            }

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

        bool half_ac = false;
        damage = apply_damage_modifiers(damage, damage_max, half_ac);

        set_attack_verb();
        return apply_defender_ac(damage, damage_max, half_ac);
    }
    else
    {
        int potential_damage;

        potential_damage =
            using_weapon() ? weapon_damage()
                           : calc_base_unarmed_damage();

        potential_damage = player_stat_modify_damage(potential_damage);

        damage_done =
            potential_damage > 0 ? one_chance_in(3) + random2(potential_damage) : 0;

        damage_done = player_apply_weapon_skill(damage_done);
        damage_done = player_apply_fighting_skill(damage_done, false);
        damage_done = player_apply_misc_modifiers(damage_done);
        damage_done = player_apply_slaying_bonuses(damage_done, false);
        damage_done = player_apply_final_multipliers(damage_done);

        damage_done = player_stab(damage_done);
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

        if (attack_ignores_shield(verbose))
            return false;

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

/* Calculates special damage, prints appropriate combat text
 *
 * Applies a particular damage brand to the current attack, the setup and
 * calculation of base damage and other effects varies based on the type
 * of attack, but the calculation of elemental damage should be consistent.
 */
void attack::calc_elemental_brand_damage(beam_type flavour,
                                                int res,
                                                const char *verb)
{
    special_damage = resist_adjust_damage(defender, flavour, res,
                                          random2(damage_done) / 2 + 1);

    if (needs_message && special_damage > 0 && verb)
    {
        special_damage_message = make_stringf(
            "%s %s %s%s",
            atk_name(DESC_THE).c_str(),
            attacker->conj_verb(verb).c_str(),
            defender_name().c_str(),
            special_attack_punctuation().c_str());
    }
}

/* Scales the amount of blood sprayed around the room
 *
 * One could justify that blood amount varies based on the sharpness of the
 * weapon or some other arbitrary element of combat.
 */
int attack::modify_blood_amount(const int damage, const int dam_type)
{
    int factor = 0; // DVORP_NONE

    switch (dam_type)
    {
    case DVORP_CRUSHING: // flails, also unarmed
    case DVORP_TENTACLE: // unarmed, tentacles
        factor =  2;
        break;
    case DVORP_SLASHING: // whips
        factor =  4;
        break;
    case DVORP_PIERCING: // pole-arms
        factor =  5;
        break;
    case DVORP_STABBING: // knives, daggers
        factor =  8;
        break;
    case DVORP_SLICING:  // other short/long blades, also blade hands
        factor = 10;
        break;
    case DVORP_CHOPPING: // axes
        factor = 17;
        break;
    case DVORP_CLAWING:  // unarmed, claws
        factor = 24;
        break;
    }

    return damage * factor / 10;
}

int attack::player_stab_weapon_bonus(int damage)
{
    int stab_skill = you.skill(wpn_skill, 50) + you.skill(SK_STEALTH, 50);
    int modified_wpn_skill = wpn_skill;

    if (player_equip_unrand(UNRAND_BOOTS_ASSASSIN)
        && (!weapon || is_melee_weapon(*weapon)))
    {
        modified_wpn_skill = SK_SHORT_BLADES;
    }
    else if (weapon && weapon->base_type == OBJ_WEAPONS
             && (weapon->sub_type == WPN_CLUB
                 || weapon->sub_type == WPN_SPEAR
                 || weapon->sub_type == WPN_TRIDENT
                 || weapon->sub_type == WPN_DEMON_TRIDENT
                 || weapon->sub_type == WPN_TRISHULA)
             || !weapon && you.species == SP_FELID)
    {
        modified_wpn_skill = SK_LONG_BLADES;
    }

    switch (modified_wpn_skill)
    {
    case SK_SHORT_BLADES:
    {
        int bonus = (you.dex() * (stab_skill + 100)) / 500;

        // We might be unarmed if we're using the boots of the Assassin.
        if (!weapon || weapon->sub_type != WPN_DAGGER)
            bonus /= 2;

        bonus   = stepdown_value(bonus, 10, 10, 30, 30);
        damage += bonus;
    }
    // fall through
    case SK_LONG_BLADES:
        damage *= 10 + div_rand_round(stab_skill, 100 *
                       (stab_bonus + (modified_wpn_skill == SK_SHORT_BLADES ? 0 : 2)));
        damage /= 10;
        // fall through
    default:
        damage *= 12 + div_rand_round(stab_skill, 100 * stab_bonus);
        damage /= 12;
        break;
    }

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
    if (st == STAB_INVISIBLE && !mons_sense_invis(defender->as_monster()))
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

bool attack::form_uses_xl()
{
    // No body parts that translate in any way to something fisticuffs could
    // matter to, the attack mode is different.  Plus, it's weird to have
    // users of one particular [non-]weapon be effective for this
    // unintentional form while others can just run or die.  I believe this
    // should apply to more forms, too.  [1KB]
    return attacker->is_player()
           && (you.form == TRAN_WISP || you.form == TRAN_FUNGUS);
}
