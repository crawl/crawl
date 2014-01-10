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

#include "externs.h"
#include "enum.h"
#include "fight.h"
#include "libutil.h"
#include "player.h"

/*
 **************************************************
 *             BEGIN PUBLIC FUNCTIONS             *
 **************************************************
*/
attack::attack(actor *attk, actor *defn)
    : attacker(attk), defender(defn), attack_occurred(false),
    cancel_attack(false), did_hit(false), needs_message(false),
    attacker_visible(false), defender_visible(false), to_hit(0),
    damage_done(0), special_damage(0), aux_damage(0), min_delay(0),
    final_attack_delay(0), apply_bleeding(false), noise_factor(0),
    ev_margin(0), weapon(NULL),
    damage_brand(SPWPN_NORMAL), wpn_skill(SK_UNARMED_COMBAT),
    shield(NULL), art_props(0), unrand_entry(NULL), attacker_to_hit_penalty(0),
    attack_verb("bug"), verb_degree(), no_damage_message(),
    special_damage_message(), aux_attack(), aux_verb(),
    defender_body_armour_penalty(0), defender_shield_penalty(0),
    attacker_body_armour_penalty(0), attacker_shield_penalty(0),
    attacker_armour_tohit_penalty(0), attacker_shield_tohit_penalty(0),
    defender_shield(NULL)
{
    // No effective code should execute, we'll call init_attack again from
    // the child class, since initializing an attack will vary based the within
    // type of attack actually being made (melee, ranged, etc.)

    init_attack();
}

bool attack::handle_phase_attempted()
{
    return true;
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
void attack::init_attack()
{
    ;
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
