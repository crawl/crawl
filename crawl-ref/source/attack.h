#ifndef ATTACK_H
#define ATTACK_H

#include "artefact.h"
#include "itemprop-enum.h"

// Used throughout inheriting classes, define them here for universal access
const int HIT_WEAK   = 7;
const int HIT_MED    = 18;
const int HIT_STRONG = 36;

enum phase
{
    ATK_ATTEMPTED,                      //  0
    ATK_DODGED,
    ATK_BLOCKED,
    ATK_HIT,
    ATK_DAMAGED,
    ATK_KILLED
};

class attack
{
// Public Properties
public:
    // General attack properties, set on instantiation or through a normal
    // thread of execution
    actor   *attacker, *defender;

    bool    cancel_attack;
    bool    did_hit;
    bool    needs_message;
    bool    attacker_visible, defender_visible;
    bool    attacker_invisible, defender_invisible;

    int     to_hit;
    int     damage_done;
    int     special_damage; // TODO: We'll see if we can remove this
    int     aux_damage;

    bool    stab_attempt;
    int     stab_bonus;

    int     min_delay;
    int     final_attack_delay;

    bool    apply_bleeding;
    bool    unarmed_ok;

    int     noise_factor;
    // int     extra_noise; May not be needed??

    // Fetched/Calculated from the attacker, stored to save execution time
    bool            unarmed_capable;
    bool            ev_margin;

    item_def        *weapon;
    brand_type      damage_brand;
    skill_type      wpn_skill;
    hands_reqd_type hands;
    bool            hand_half_bonus;
    // Attacker's shield, stored so we can reference it and determine
    // the attacker's combat effectiveness (staff + shield == bad)
    item_def  *shield;

    // If weapon is an artefact, its properties.
    artefact_properties_t art_props;

    // If a weapon is an unrandart, its unrandart entry.
    unrandart_entry *unrand_entry;

    int     attacker_to_hit_penalty;

    // Attack messages
    std::string     attack_verb, verb_degree;
    std::string     no_damage_message;
    std::string     special_damage_message;
    std::string     aux_attack, aux_verb;

    // Fetched/Calculated from the defender, stored to save execution time
    int     defender_body_armour_penalty;
    int     defender_shield_penalty;

    item_def        *defender_shield;

// Public Methods
public:
    attack(actor *attk, actor *defn, bool allow_unarmed);

    // Applies damage and effect(s)
    // TODO: Maybe this should actually occur in fight (eg damage is calculated
    // within a particular attack, but fight is responsible for performing the
    // damage.
    /* virtual bool do_attack(); */

    // To-hit is a function of attacker/defender, defined in sub-classes
    virtual int calc_to_hit(bool) = 0;

    // Exact copies of their melee_attack predecessors
    std::string actor_name(const actor *a, description_level_type desc,
                           bool actor_visible, bool actor_invisible);
    std::string actor_pronoun(const actor *a, pronoun_type ptyp,
                              bool actor_visible);
    std::string anon_name(description_level_type desc,
                          bool actor_invisible);
    std::string anon_pronoun(pronoun_type ptyp);

// Private Methods
protected:
    virtual void init_attack();
    virtual bool check_unrand_effects() = 0;

    // Methods which produce output
    std::string debug_damage_number();
    std::string special_attack_punctuation();
    std::string attack_strength_punctuation();
    std::string evasion_margin_adverb();

    // Determine if we're blocking (partially or entirely)
    virtual bool attack_shield_blocked(bool verbose) = 0;
    virtual bool apply_damage_brand() = 0;

    // Ouput methods
    std::string atk_name(description_level_type desc);
    std::string def_name(description_level_type desc);
    std::string wep_name(description_level_type desc = DESC_YOUR,
                         iflags_t ignore_flags = ISFLAG_KNOW_CURSE
                                               | ISFLAG_KNOW_PLUSES);

    // TODO: Used in elemental brand dmg, definitely want to get rid of this
    // which we can't really do until we refactor the whole pronoun / desc
    // usage from these lowly classes all the way up to monster/player (and
    // actor) classes.
    std::string defender_name();

    void calc_elemental_brand_damage(beam_type flavour,
                                     int res,
                                     const char *verb);
};

#endif
