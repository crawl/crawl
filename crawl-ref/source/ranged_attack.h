#ifndef RANGED_ATTACK_H
#define RANGED_ATTACK_H

#include "attack.h"

class ranged_attack : public attack
{
// Public Methods
public:
    ranged_attack(actor *attacker, actor *defender, item_def *projectile);

    int calc_to_hit(bool);

private:
    /* Attack Phases */
    bool handle_phase_attempted();
    bool handle_phase_dodged();
    bool handle_phase_blocked();
    bool handle_phase_hit();
    bool handle_phase_damaged();
    bool handle_phase_killed();
    bool handle_phase_end();

    /* Combat Calculations */
    bool attack_shield_blocked(bool verbose);
    bool apply_damage_brand();

    /* Weapon Effects */
    bool check_unrand_effects();

private:
    const item_def *projectile;
};

#endif
