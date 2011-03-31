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
attack::attack(actor *attk, actor *defn, bool allow_unarmed)
    : attacker(attk), defender(defn), attack_phase(ATK_ATTEMPTED),
    cancel_attack(false), did_hit(false), needs_message(false),
    attacker_visible(false), defender_visible(false),
    attacker_invisible(false), defender_invisible(false), to_hit(0),
    damage_done(0), aux_damage(0), stab_attempt(false), stab_bonus(0),
    min_delay(0), final_attack_delay(0), unarmed_ok(allow_unarmed),
    noise_factor(0), unarmed_capable(false), ev_margin(0), weapon(NULL),
    damage_brand(SPWPN_NORMAL), wpn_skill(SK_UNARMED_COMBAT), hands(HANDS_ONE),
    hand_half_bonus(false), shield(NULL), art_props(0), unrand_entry(NULL),
    attacker_to_hit_penalty(0), attack_verb("bug"), verb_degree(),
    no_damage_message(), special_damage_message(), aux_attack(), aux_verb(),
    defender_body_armour_penalty(0), defender_shield_penalty(0),
    defender_shield(NULL)
{
    // No effective code should execute, we'll call init_attack from within
    // the child class, since initializing an attack will vary based the type
    // of attack actually being made (melee, ranged, etc.)
}

bool attack::handle_phase_attempted() {
    return true;
}

/* Returns an actor's name
 *
 * Takes into account actor visibility/invisibility and the type of description
 * to be used (capitalization, possessiveness, etc.)
 */
std::string attack::actor_name(const actor *a, description_level_type desc,
                               bool actor_visible, bool actor_invisible)
{
    return (actor_visible ? a->name(desc) : anon_name(desc, actor_invisible));
}

/* Returns an actor's pronoun
 *
 * Takes into account actor visibility
 */
std::string attack::actor_pronoun(const actor *a, pronoun_type pron,
                                  bool actor_visible)
{
    return (actor_visible ? a->pronoun(pron) : anon_pronoun(pron));
}

/* Returns an anonymous actor's name
 *
 * Given the actor visible or invisible, returns the
 * appropriate possessive, capitalized pronoun.
 */
std::string attack::anon_name(description_level_type desc,
                                    bool actor_invisible)
{
    switch (desc)
    {
    case DESC_NONE:
        return ("");
    case DESC_YOUR:
    case DESC_ITS:
        return ("its");
    case DESC_THE:
    case DESC_A:
    case DESC_PLAIN:
    default:
        return (actor_invisible? "it" : "something");
    }
}

/* Returns an anonymous actor's pronoun
 *
 * Given invisibility (whether out of LOS or just invisible), returns the
 * appropriate possessive, inflexive, capitalized pronoun.
 */
std::string attack::anon_pronoun(pronoun_type pron)
{
    switch (pron)
    {
    default:
    case PRONOUN_CAP:              return "It";
    case PRONOUN_NOCAP:            return "it";
    case PRONOUN_CAP_POSSESSIVE:   return "Its";
    case PRONOUN_NOCAP_POSSESSIVE: return "its";
    case PRONOUN_REFLEXIVE:        return "itself";
    }
}

/* Initializes an attack, setting up base variables and values
 *
 * Does not make any changes to any actors, items, or the environment,
 * in case the attack is canceled or otherwise fails. Only initializations
 * that are universal to all types of attacks should go into this method,
 * any initialization properties that are specific to one attack or another
 * should go into their respective init_attack.
 *
 * Although this method will get overloaded by the derived class, we are
 * calling it from attack::attack(), before the overloading has taken place.
 */
void attack::init_attack()
{
    mprf("attack::init_attack()");
}

/* If debug, return formatted damage done
 *
 */
std::string attack::debug_damage_number()
{
#ifdef DEBUG_DIAGNOSTICS
    return make_stringf(" for %d", damage_done);
#else
    return ("");
#endif
}

/* Returns special punctuation
 *
 * Used (mostly) for elemental or branded attacks (napalm, dragon slaying, orc
 * slaying, holy, etc.)
 */
std::string attack::special_attack_punctuation()
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
std::string attack::attack_strength_punctuation()
{
    if (attacker->atype() == ACT_PLAYER)
    {
        if (damage_done < HIT_WEAK)
            return ".";
        else if (damage_done < HIT_MED)
            return "!";
        else if (damage_done < HIT_STRONG)
            return "!!";
        else
            return "!!!";
    }
    else
        return (damage_done < HIT_WEAK ? "." : "!");
}

/* Returns evasion adverb
 *
 */
std::string attack::evasion_margin_adverb()
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
std::string attack::atk_name(description_level_type desc)
{
    return actor_name(attacker, desc, attacker_visible, attacker_invisible);
}

/* Returns the defender's name
 *
 * Helper method to easily access the defender's name
 */
std::string attack::def_name(description_level_type desc)
{
    return actor_name(defender, desc, defender_visible, defender_invisible);
}

/* Returns the attacking weapon's name
 *
 * Sets upthe appropriate descriptive level and obtains the name of a weapon
 * based on if the attacker is a player or non-player (non-players use a
 * plain name and a manually entered pronoun)
 */
std::string attack::wep_name(description_level_type desc, iflags_t ignre_flags)
{
    ASSERT(weapon != NULL);

    if (attacker->atype() == ACT_PLAYER)
        return weapon->name(desc, false, false, false, false, ignre_flags);

    std::string name;
    bool possessive = false;
    if (desc == DESC_YOUR)
    {
        desc       = DESC_THE;
        possessive = true;
    }

    if (possessive)
        name = apostrophise(atk_name(desc)) + " ";

    name += weapon->name(DESC_PLAIN, false, false, false, false, ignre_flags);

    return (name);
}

/* TODO: Remove this!
 *
 */
std::string attack::defender_name()
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
