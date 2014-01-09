#ifndef ATTACK_H
#define ATTACK_H

#include "artefact.h"
#include "itemprop-enum.h"
#include "mon-enum.h"

// Used throughout inheriting classes, define them here for universal access
const int HIT_WEAK   = 7;
const int HIT_MED    = 18;
const int HIT_STRONG = 36;

class attack
{
// Public Properties
public:
    // General attack properties, set on instantiation or through a normal
    // thread of execution
    actor   *attacker, *defender;

    bool    attack_occurred;
    bool    cancel_attack;
    bool    did_hit;
    bool    needs_message;
    bool    attacker_visible, defender_visible;

    int     to_hit;
    int     damage_done;
    int     special_damage; // TODO: We'll see if we can remove this
    int     aux_damage;     // TOOD: And this too

    int     min_delay;
    int     final_attack_delay;

    bool    apply_bleeding;

    int     noise_factor;

    // Fetched/Calculated from the attacker, stored to save execution time
    int             ev_margin;

    // TODO: Expand the implementation of attack_type/attack_flavour to be used
    // by players as well as monsters. It could be a good middle ground for
    // brands and attack types (presently, players don't really have an attack
    // type)
    attack_type     attk_type;
    attack_flavour  attk_flavour;
    int             attk_damage;

    item_def        *weapon;
    brand_type      damage_brand;
    skill_type      wpn_skill;

    // Attacker's shield, stored so we can reference it and determine
    // the attacker's combat effectiveness
    item_def  *shield;

    // If weapon is an artefact, its properties.
    artefact_properties_t art_props;

    // If a weapon is an unrandart, its unrandart entry.
    const unrandart_entry *unrand_entry;

    int     attacker_to_hit_penalty;

    // Attack messages
    string     attack_verb, verb_degree;
    string     no_damage_message;
    string     special_damage_message;
    string     aux_attack, aux_verb;

    // Adjusted EV penalties for defender and attacker
    int             defender_body_armour_penalty;
    int             defender_shield_penalty;
    int             attacker_body_armour_penalty;
    int             attacker_shield_penalty;
    // Combined to-hit penalty from armour and shield.
    int             attacker_armour_tohit_penalty;
    int             attacker_shield_tohit_penalty;

    item_def        *defender_shield;

// Public Methods
public:
    attack(actor *attk, actor *defn);

    // To-hit is a function of attacker/defender, defined in sub-classes
    virtual int calc_to_hit(bool) = 0;

    // Exact copies of their melee_attack predecessors
    string actor_name(const actor *a, description_level_type desc,
                      bool actor_visible);
    string actor_pronoun(const actor *a, pronoun_type ptyp, bool actor_visible);
    string anon_name(description_level_type desc);
    string anon_pronoun(pronoun_type ptyp);

// Private Methods
protected:
    virtual void init_attack();

    /* Attack Phases */
    virtual bool handle_phase_attempted();
    virtual bool handle_phase_dodged() = 0;
    virtual bool handle_phase_blocked() = 0;
    virtual bool handle_phase_hit() = 0;
    virtual bool handle_phase_damaged() = 0;
    virtual bool handle_phase_killed() = 0;
    virtual bool handle_phase_end() = 0;

    /* Combat Calculations */
    // Determine if we're blocking (partially or entirely)
    virtual bool attack_shield_blocked(bool verbose) = 0;
    virtual bool apply_damage_brand() = 0;
    void calc_elemental_brand_damage(beam_type flavour,
                                     int res,
                                     const char *verb);

    /* Weapon Effects */
    virtual bool check_unrand_effects() = 0;

    /* Output */
    string debug_damage_number();
    string special_attack_punctuation();
    string attack_strength_punctuation();
    string get_exclams(int dmg);
    string evasion_margin_adverb();

    string atk_name(description_level_type desc);
    string def_name(description_level_type desc);
    string wep_name(description_level_type desc = DESC_YOUR,
                    iflags_t ignore_flags = ISFLAG_KNOW_CURSE
                                            | ISFLAG_KNOW_PLUSES);

    int modify_blood_amount(const int damage, const int dam_type);
    // TODO: Used in elemental brand dmg, definitely want to get rid of this
    // which we can't really do until we refactor the whole pronoun / desc
    // usage from these lowly classes all the way up to monster/player (and
    // actor) classes.
    string defender_name();
};

#endif
