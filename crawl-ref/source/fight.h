/**
 * @file
 * @brief Functions used during combat.
**/

#ifndef FIGHT_H
#define FIGHT_H

#include "melee_attack.h"
#include "mon-enum.h"
#include "random-var.h"

enum unchivalric_attack_type
{
    UCAT_NO_ATTACK,                    //    0
    UCAT_DISTRACTED,
    UCAT_CONFUSED,
    UCAT_FLEEING,
    UCAT_INVISIBLE,
    UCAT_HELD_IN_NET,
    UCAT_PETRIFYING,
    UCAT_PETRIFIED,
    UCAT_PARALYSED,
    UCAT_SLEEPING,
    UCAT_ALLY,
};

bool fight_melee(actor *attacker, actor *defender, bool allow_unarmed = true);

int effective_stat_bonus(int wepType = -1);

int resist_adjust_damage(actor *defender, beam_type flavour,
                         int res, int rawdamage, bool ranged = false);

int weapon_str_weight(object_class_type wpn_class, int wpn_type);

bool wielded_weapon_check(item_def *weapon, bool no_message = false);
int calc_heavy_armour_penalty(bool random_factor);

unchivalric_attack_type is_unchivalric_attack(const actor *attacker,
                                              const actor *defender);

void chaos_affect_actor(actor *victim);
int player_weapon_str_weight();
int player_weapon_dex_weight();

#endif
