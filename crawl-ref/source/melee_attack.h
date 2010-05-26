#ifndef MELEE_ATTACK_H
#define MELEE_ATTACK_H

#include "artefact.h"
#include "random-var.h"

class melee_attack
{
public:
    actor     *attacker, *defender;

    bool      cancel_attack;
    bool      did_hit, perceived_attack, obvious_effect;

    // If all or part of the action is visible to the player, we need a message.
    bool      needs_message;
    bool      attacker_visible, defender_visible;
    bool      attacker_invisible, defender_invisible;

    bool      unarmed_ok;
    int       attack_number;

    int       to_hit;
    int       damage_done;
    int       special_damage;
    int       aux_damage;

    bool      stab_attempt;
    int       stab_bonus;

    int       min_delay;
    int       final_attack_delay;

    int       noise_factor;
    int       extra_noise;

    // Attacker's damage output potential:

    item_def  *weapon;
    int       damage_brand;  // Can be special even if unarmed (transforms)
    int       wpn_skill, hands;
    bool      hand_half_bonus;

    // If weapon is an artefact, its properties.
    artefact_properties_t art_props;

    // If a weapon is an unrandart, its unrandart entry.
    unrandart_entry *unrand_entry;

    // Attack messages
    std::string attack_verb, verb_degree;
    std::string no_damage_message;
    std::string special_damage_message;
    std::string aux_attack, aux_verb;
    beam_type special_damage_flavour;

    item_def  *shield;
    item_def  *defender_shield;

    // Armour penalties?
    // Adjusted EV penalty for body armour and shields.
    int       player_body_armour_penalty;
    int       player_shield_penalty;

    // Combined to-hit penalty from armour and shield.
    int       player_armour_tohit_penalty;
    int       player_shield_tohit_penalty;

    bool      can_do_unarmed;

    // Miscast to cause after special damage is done.  If miscast_level == 0
    // the miscast is discarded if special_damage_message isn't empty.
    int    miscast_level;
    int    miscast_type;
    actor* miscast_target;

public:
    melee_attack(actor *attacker, actor *defender,
                 bool allow_unarmed = true, int attack_num = -1);

    // Applies attack damage and other effects.
    bool attack();

    int  calc_to_hit(bool random = true);
    random_var player_calc_attack_delay();

    static std::string anon_name(description_level_type desc,
                                 bool actor_invisible);
    static std::string actor_name(const actor *a, description_level_type desc,
                                  bool actor_visible, bool actor_invisible);
    static std::string pronoun(const actor *a, pronoun_type ptyp,
                               bool actor_visible);
    static std::string anon_pronoun(pronoun_type ptyp);

private:
    void init_attack();
    bool is_banished(const actor *) const;
    void check_autoberserk();
    bool check_unrand_effects(bool mondied = false);
    void emit_nodmg_hit_message();
    void identify_mimic(actor *mon);

    std::string debug_damage_number();
    std::string special_attack_punctuation();
    std::string attack_strength_punctuation();

    std::string atk_name(description_level_type desc) const;
    std::string def_name(description_level_type desc) const;
    std::string wep_name(description_level_type desc = DESC_NOCAP_YOUR,
                         unsigned long ignore_flags = ISFLAG_KNOW_CURSE
                                                    | ISFLAG_KNOW_PLUSES) const;

    bool attack_shield_blocked(bool verbose);
    bool apply_damage_brand();
    void calc_elemental_brand_damage(beam_type flavour,
                                     int res,
                                     const char *verb);
    int fire_res_apply_cerebov_downgrade(int res);
    void drain_defender();
    void rot_defender(int amount, int immediate = 0);
    void check_defender_train_armour();
    void check_defender_train_dodging();
    void splash_defender_with_acid(int strength);
    void splash_monster_with_acid(int strength);
    bool decapitate_hydra(int damage_done, int damage_type = -1);
    bool chop_hydra_head( int damage_done,
                          int dam_type,
                          int wpn_brand );

    // Returns true if the defender is banished.
    bool distortion_affects_defender();

    void chaos_affects_defender();
    void chaos_affects_attacker();
    void chaos_killed_defender(monsters* def_copy);
    int  random_chaos_brand();
    void do_miscast();

    std::vector<attack_final_effect> final_effects;

    void handle_noise(const coord_def & pos);

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
    std::string mons_attack_desc(const mon_attack_def &attk);
    void mons_announce_hit(const mon_attack_def &attk);
    void mons_announce_dud_hit(const mon_attack_def &attk);
    void mons_set_weapon(const mon_attack_def &attk);
    void mons_do_poison(const mon_attack_def &attk);
    void mons_do_napalm();
    std::string mons_defender_name();
    void wasp_paralyse_defender();
    void mons_do_passive_freeze();
    void mons_do_spines();
    void mons_do_eyeball_confusion();

    mon_attack_flavour random_chaos_attack_flavour();

private:
    // Player-attack specific stuff
    bool player_attack();

    // Auxiliary unarmed attacks.
    bool player_aux_unarmed();
    unarmed_attack_type player_aux_choose_baseattack();
    void player_aux_setup(unarmed_attack_type atk);
    bool player_aux_test_hit();
    bool player_aux_apply();

    int  player_stat_modify_damage(int damage);
    int  player_aux_stat_modify_damage(int damage);
    int  player_to_hit(bool random_factor);
    void calc_player_armour_shield_tohit_penalty(bool random_factor);
    void player_apply_attack_delay();
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
    void player_exercise_combat_skills();
    bool player_monattk_final_hit_effects(bool mondied);
    bool player_monattk_hit_effects(bool mondied);
    void player_sustain_passive_damage();
    int  player_staff_damage(int skill);
    void player_apply_staff_damage();
    bool player_check_monster_died();
    void player_calc_hit_damage();
    void player_stab_check();
    random_var player_weapon_speed();
    random_var player_unarmed_speed();
    void player_announce_hit();
    std::string player_why_missed();
    void player_warn_miss();
    void player_check_weapon_effects();
    void _monster_die(monsters *monster, killer_type killer, int killer_index);
};

#endif
