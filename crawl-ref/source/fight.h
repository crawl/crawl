/*
 *  File:       fight.cc
 *  Summary:    Functions used during combat.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 *
 *  Change History (most recent first):
 *
 *               <1>     -/--/--        LRH             Created
 */


#ifndef FIGHT_H
#define FIGHT_H


#include "externs.h"
#include "randart.h"

// added Sept 18, 2000 -- bwr
/* ***********************************************************************
 * called from: item_use.cc
 * *********************************************************************** */
int effective_stat_bonus( int wepType = -1 );

// added Sept 18, 2000 -- bwr
/* ***********************************************************************
 * called from: describe.cc
 * *********************************************************************** */
int weapon_str_weight( int wpn_class, int wpn_type );


// last updated: 08jun2000 {dlb}
/* ***********************************************************************
 * called from: acr - it_use3
 * *********************************************************************** */
bool you_attack(int monster_attacked, bool unarmed_attacks);


// last updated: 08jun2000 {dlb}
/* ***********************************************************************
 * called from: monstuff
 * *********************************************************************** */
void monster_attack(int monster_attacking);


// last updated: 08jun2000 {dlb}
/* ***********************************************************************
 * called from: monstuff
 * *********************************************************************** */
bool monsters_fight(int monster_attacking, int monster_attacked);

int calc_your_to_hit( bool random_factor );

int calc_heavy_armour_penalty( bool random_factor );

struct melee_attack
{
public:
    // At the moment this only covers players fighting monsters
    actor     *attacker, *defender;

    // If attacker and/or defender are monsters, these are set.
    monsters  *atk, *def;

    bool      cancel_attack;
    bool      did_hit, perceived_attack;

    // If all or part of the action is visible to the player, we need a message.
    bool      needs_message;
    bool      attacker_visible, defender_visible;
    
    bool      unarmed_ok;
    int       attack_number;
    
    int       to_hit;
    int       base_damage;
    int       potential_damage;
    int       damage_done;
    int       special_damage;
    int       aux_damage;

    bool      stab_attempt;
    int       stab_bonus;

    int       min_delay;
    int       final_attack_delay;

    // Attacker's damage output potential:
    
    item_def  *weapon;
    int       damage_brand;  // Can be special even if unarmed (transforms)
    int       wpn_skill, hands;
    int       spwld;         // Special wield effects?
    bool      hand_half_bonus;

    // If weapon is a randart, its properties.
    randart_properties_t art_props;

    // Attack messages
    std::string attack_verb, verb_degree;
    std::string no_damage_message;
    std::string special_damage_message;
    std::string unarmed_attack;

    item_def  *shield;
    item_def  *defender_shield;

    // Armour penalties?
    int       heavy_armour_penalty;
    bool      can_do_unarmed;
    
    // Attacker uses watery terrain to advantage vs defender. Implies that
    // both attacker and defender are in water.
    bool      water_attack;

public:
    melee_attack(actor *attacker, actor *defender,
                 bool allow_unarmed = true, int attack_num = -1);

    // Applies attack damage and other effects.
    bool attack();

    int  calc_to_hit(bool random = true);

private:
    void init_attack();
    bool is_water_attack(const actor *, const actor *) const;
    void check_hand_half_bonus_eligible();
    void check_autoberserk();
    void check_special_wield_effects();
    void emit_nodmg_hit_message();
    void identify_mimic(monsters *mon);

    std::string debug_damage_number();
    std::string special_attack_punctuation();
    std::string attack_strength_punctuation();

    bool attack_shield_blocked(bool verbose);
    bool apply_damage_brand();
    void calc_elemental_brand_damage(int res, const char *verb);
    int resist_adjust_damage(int res, int rawdamage);
    int fire_res_apply_cerebov_downgrade(int res);
    void drain_defender();
    void drain_player();
    void drain_monster();
    void check_defender_train_armour();
    void check_defender_train_dodging();
    void splash_defender_with_acid(int strength);
    void splash_monster_with_acid(int strength);

    // Returns true if the defender is banished.
    bool distortion_affects_defender();

private:
    // Monster-attack specific stuff
    bool mons_attack_you();
    bool mons_attack_mons();
    int  mons_to_hit();
    bool mons_self_destructs();
    bool mons_attack_warded_off();
    int mons_attk_delay();
    int mons_calc_damage(const mon_attack_def &attk);
    void mons_apply_attack_flavour(const mon_attack_def &attk);
    int mons_apply_defender_ac(int damage, int damage_max);
    bool mons_perform_attack();
    void mons_perform_attack_rounds();
    void mons_check_attack_perceived();
    std::string mons_attack_verb(const mon_attack_def &attk);
    std::string mons_weapon_desc();
    void mons_announce_hit(const mon_attack_def &attk);
    void mons_announce_dud_hit(const mon_attack_def &attk);
    void mons_set_weapon(const mon_attack_def &attk);
    void mons_do_poison(const mon_attack_def &attk);
    std::string mons_defender_name();
    void wasp_paralyse_defender();

private:
    // Player-attack specific stuff
    bool player_attack();
    bool player_aux_unarmed();
    bool player_apply_aux_unarmed();
    int  player_stat_modify_damage(int damage);
    int  player_aux_stat_modify_damage(int damage);
    int  player_to_hit(bool random_factor);
    void player_apply_attack_delay();
    int  player_apply_water_attack_bonus(int damage);
    int  player_apply_weapon_bonuses(int damage);
    int  player_apply_weapon_skill(int damage);
    int  player_apply_fighting_skill(int damage, bool aux);
    int  player_apply_misc_modifiers(int damage);
    int  player_apply_monster_ac(int damage);
    void player_weapon_auto_id();
    int  player_stab_weapon_bonus(int damage);
    int  player_stab(int damage);
    int  player_weapon_type_modify(int damage);
    
    bool player_hits_monster();
    int  player_calc_base_weapon_damage();
    int  player_calc_base_unarmed_damage();
    bool player_hurt_monster();
    void player_exercise_combat_skills();
    bool player_monattk_hit_effects(bool mondied);
    void player_sustain_passive_damage();
    int  player_staff_damage(int skill);
    void player_apply_staff_damage();
    bool player_check_monster_died();
    void player_calc_hit_damage();
    void player_stab_check();
    int  player_weapon_speed();
    int  player_unarmed_speed();
    int  player_apply_shield_delay(int attack_delay);
    void player_announce_hit();
    std::string player_why_missed();
    void player_warn_miss();
};

#endif
