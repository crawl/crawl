#pragma once

#include "attack.h"

class ranged_attack : public attack
{
// Public Properties
public:
    int range_used;
    bool reflected;

    bool will_mulch;
    bool pierce;

// Public Methods
public:
    ranged_attack(actor *attacker, actor *defender,
                  const item_def *wpn,
                  bool teleport = false, actor *blame = 0);

    // Applies attack damage and other effects.
    bool attack();
    int post_roll_to_hit_modifiers(int mhit, bool random) override;

    bool did_net() const;

    void set_projectile_prefix(string prefix);
    string projectile_name() const;
    bool is_piercing() const;

    void copy_params_to(ranged_attack &other) const;

private:
    /* Attack Phases */
    bool handle_phase_attempted() override;
    void handle_phase_blocked() override;
    void handle_phase_dodged() override;
    bool handle_phase_hit() override;
    bool ignores_shield() override;

    /* Combat Calculations */
    bool using_weapon() const override;
    int weapon_damage() const override;
    int calc_mon_to_hit_base() override;
    int apply_mon_damage_modifiers(int damage) override;
    int player_apply_final_multipliers(int damage, bool aux = false) override;
    int player_apply_postac_multipliers(int damage) override;
    special_missile_type random_chaos_missile_brand();
    bool dart_check(special_missile_type type);
    int dart_duration_roll(special_missile_type type);
    bool apply_missile_brand();
    bool throwing() const;

    /* Weapon Effects */
    bool check_unrand_effects() override;

    /* Attack Effects */
    bool mons_attack_effects() override;
    void player_stab_check() override;
    bool player_good_stab() override;

    /* Output */
    void set_attack_verb(int damage) override;
    void announce_hit() override;

    bool mulch_bonus() const;

private:
    string proj_name;
    bool teleport;
    bool _did_net;
};
