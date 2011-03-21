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
#include "itemprop.h"

/*
 **************************************************
 *             BEGIN PUBLIC FUNCTIONS             *
 **************************************************
*/
attack::attack(actor *attk, actor *defn, bool allow_unarmed)
    : attacker(attk), defender(defn), cancel_attack(false), did_hit(false),
    needs_message(false), attacker_visible(false), defender_visible(false),
    attacker_invisible(false), defender_invisible(false),
    unarmed_ok(allow_unarmed), to_hit(0), damage_done(0), aux_damage(0), 
    stab_attempt(false), stab_bonus(0), min_delay(0), final_attack_delay(0), 
    noise_factor(0), unarmed_capable(false), ev_margin(0), weapon(NULL),
    damage_brand(SPWPN_NORMAL), wpn_skill(SK_UNARMED_COMBAT), hands(HANDS_ONE),
    hand_half_bonus(false), attacker_to_hit_penalty(0), attack_verb("bug"), 
    verb_degree(), no_damage_message(), special_damage_message(), aux_attack(), 
    aux_verb(), defender_body_armour_penalty(0), defender_shield_penalty(0),
    defender_shield(NULL)
{
    // No effective code should execute, we'll call init_attack from within 
    // the child class, since initializing an attack will vary based the type
    // of attack actually being made (melee, ranged, etc.)
}

std::string attack::actor_name(const actor *a, description_level_type desc,
                               bool actor_visible, bool actor_invisible)
{
    return (actor_visible ? a->name(desc) : anon_name(desc, actor_invisible));
}

std::string attack::pronoun(const actor *a, pronoun_type pron, 
                            bool actor_visible)
{
    return (actor_visible ? a->pronoun(pron) : anon_pronoun(pron));
}

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

std::string attack::anon_name(description_level_type desc,
                                    bool actor_invisible)
{
    switch (desc)
    {
    case DESC_CAP_THE:
    case DESC_CAP_A:
        return (actor_invisible ? "It" : "Something");
    case DESC_CAP_YOUR:
        return ("Its");
    case DESC_NOCAP_YOUR:
    case DESC_NOCAP_ITS:
        return ("its");
    case DESC_NONE:
        return ("");
    case DESC_NOCAP_THE:
    case DESC_NOCAP_A:
    case DESC_PLAIN:
    default:
        return (actor_invisible? "it" : "something");
    }
}

std::string attack::atk_name(description_level_type desc) const
{
    return actor_name(attacker, desc, attacker_visible, attacker_invisible);
}

std::string attack::def_name(description_level_type desc) const
{
    return actor_name(defender, desc, defender_visible, defender_invisible);
}

std::string attack::wep_name(description_level_type desc,
                             iflags_t ignore_flags) const
{
    ASSERT(weapon != NULL);

    if (attacker->atype() == ACT_PLAYER)
        return weapon->name(desc, false, false, false, false, ignore_flags);

    std::string name;
    bool possessive = false;
    if (desc == DESC_CAP_YOUR)
    {
        desc       = DESC_CAP_THE;
        possessive = true;
    }
    else if (desc == DESC_NOCAP_YOUR)
    {
        desc       = DESC_NOCAP_THE;
        possessive = true;
    }

    if (possessive)
        name = apostrophise(atk_name(desc)) + " ";

    name += weapon->name(DESC_PLAIN, false, false, false, false, ignore_flags);

    return (name);
}