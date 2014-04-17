#ifndef RANGED_ATTACK_H
#define RANGED_ATTACK_H

#include "attack.h"

class ranged_attack : public attack
{
// Public Properties
public:
    int range_used;

// Public Methods
public:
    ranged_attack(actor *attacker, actor *defender, item_def *projectile,
                  bool teleport);

    int calc_to_hit(bool random);

    // Applies attack damage and other effects.
    bool attack();

private:
    /* Attack Phases */
    bool handle_phase_attempted();
    bool handle_phase_blocked();
    bool handle_phase_dodged();
    bool handle_phase_hit();

    /* Combat Calculations */
    bool using_weapon();
    int weapon_damage();
    int apply_damage_modifiers(int damage, int damage_max, bool &half_ac);
    bool attack_ignores_shield(bool verbose);
    bool apply_damage_brand();

    /* Weapon Effects */
    bool check_unrand_effects();

    /* Attack Effects */
    bool mons_attack_effects();

    /* Output */
    void adjust_noise();
    void set_attack_verb();
    void announce_hit();

private:
    const item_def *projectile;
    bool teleport;
    int orig_to_hit;
};

#endif
