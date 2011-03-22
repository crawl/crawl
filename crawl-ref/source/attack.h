#ifndef ATTACK_H
#define ATTACK_H

#include "itemprop-enum.h"

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
    virtual bool do_attack();

    // To-hit is a function of attacker/defender, but we may adjust it
    // for a particular attack, especially if there are temporary effects
    virtual int adjust_to_hit(bool random = true);

    // Exact copies of their melee_attack predecessors
    std::string actor_name(const actor *a, description_level_type desc,
                           bool actor_visible, bool actor_invisible);
    std::string actor_pronoun(const actor *a, pronoun_type ptyp,
                              bool actor_visible);
    std::string anon_name(description_level_type desc,
                          bool actor_invisible);
    std::string anon_pronoun(pronoun_type ptyp);
    
// Private Methods
private:
    virtual void init_attack();
    virtual void check_unrand_effects();

    // Methods which produce output
    std::string debug_damage_number();
    std::string special_attack_punctuation();
    std::string attack_strength_punctuation();
    std::string evasion_margin_adverb();

    // Determine if we're blocking (partially or entirely)
    virtual bool attack_shield_blocked(bool verbose);
    virtual bool apply_damage_brand();

    // Ouput methods
    std::string atk_name(description_level_type desc) const;
    std::string def_name(description_level_type desc) const;
    std::string wep_name(description_level_type desc = DESC_NOCAP_YOUR,
                         iflags_t ignore_flags = ISFLAG_KNOW_CURSE
                                               | ISFLAG_KNOW_PLUSES) const;

    // TODO: Used in elemental brand dmg, definitely want to get rid of this
    std::string defender_name();
    
    void calc_elemental_brand_damage(beam_type flavour,
                                     int res,
                                     const char *verb);    
};

#endif