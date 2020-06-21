#pragma once

#include "attack.h"

const int PPROJ_TO_HIT_DIV = 8;

class ranged_attack : public attack
{
// Public Properties
public:
    int range_used;
    bool reflected;

// Public Methods
public:
    ranged_attack(actor *attacker, actor *defender, item_def *projectile,
                  bool teleport, actor *blame = 0);

    // Applies attack damage and other effects.
    bool attack();
    int calc_to_hit(bool random) override;
    int post_roll_to_hit_modifiers(int mhit, bool random) override;

private:
    /* Attack Phases */
    bool handle_phase_attempted() override;
    bool handle_phase_blocked() override;
    bool handle_phase_dodged() override;
    bool handle_phase_hit() override;
    bool ignores_shield(bool verbose) override;

    /* Combat Calculations */
    bool using_weapon() const override;
    int weapon_damage() override;
    int calc_base_unarmed_damage() override;
    int calc_mon_to_hit_base() override;
    int apply_damage_modifiers(int damage) override;
    bool apply_damage_brand(const char *what = nullptr) override;
    special_missile_type random_chaos_missile_brand();
    bool dart_check(special_missile_type type);
    int dart_duration_roll(special_missile_type type);
    bool apply_missile_brand();

    /* Weapon Effects */
    bool check_unrand_effects() override;

    /* Attack Effects */
    bool mons_attack_effects() override;
    void player_stab_check() override;
    bool player_good_stab() override;

    /* Output */
    void set_attack_verb(int damage) override;
    void announce_hit() override;

private:
    const item_def *projectile;
    bool teleport;
    int orig_to_hit;
    bool should_alert_defender;
    launch_retval launch_type;
};
