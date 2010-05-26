/*
 *  File:       fight.h
 *  Summary:    Functions used during combat.
 *  Written by: Linley Henzell
 */


#ifndef FIGHT_H
#define FIGHT_H

#include "mon-enum.h"
#include "random-var.h"

enum unarmed_attack_type
{
    UNAT_NO_ATTACK,                    //    0
    UNAT_KICK,
    UNAT_HEADBUTT,
    UNAT_TAILSLAP,
    UNAT_PUNCH,
    UNAT_BITE,
    UNAT_PSEUDOPODS,
    UNAT_FIRST_ATTACK = UNAT_KICK,
    UNAT_LAST_ATTACK = UNAT_PSEUDOPODS
};

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
    UCAT_ALLY
};

enum attack_final_effect_flavor
{
    FINEFF_LIGHTNING_DISCHARGE
};
struct attack_final_effect
{
    attack_final_effect_flavor flavor;
    coord_def location;
};

struct mon_attack_def;

int effective_stat_bonus( int wepType = -1 );

int resist_adjust_damage(actor *defender, beam_type flavour,
                         int res, int rawdamage, bool ranged = false);

int weapon_str_weight( object_class_type wpn_class, int wpn_type );
bool you_attack(int monster_attacked, bool unarmed_attacks);
bool monster_attack_actor(monsters *attacker, actor *defender,
                          bool allow_unarmed);
bool monster_attack(monsters* attacker, bool allow_unarmed = true);
bool monsters_fight(monsters* attacker, monsters* attacked,
                    bool allow_unarmed = true);

bool wielded_weapon_check(item_def *weapon, bool no_message = false);
int calc_your_to_hit(bool random_factor);
int calc_heavy_armour_penalty(bool random_factor);
random_var calc_your_attack_delay();

unchivalric_attack_type is_unchivalric_attack(const actor *attacker,
                                              const actor *defender);

#endif
