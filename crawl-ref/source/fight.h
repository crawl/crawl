/**
 * @file
 * @brief Functions used during combat.
**/

#ifndef FIGHT_H
#define FIGHT_H

#include <list>

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
    NUM_UCAT
};

bool fight_melee(actor *attacker, actor *defender, bool *did_hit = NULL,
                 bool simu = false);

bool fight_jump(actor *attacker, actor *defender, coord_def attack_pos,
                coord_def landing_pos, set<coord_def> landing_sites,
                bool jump_blocked, bool *did_hit = NULL);

int resist_adjust_damage(actor *defender, beam_type flavour,
                         int res, int rawdamage, bool ranged = false);

bool wielded_weapon_check(item_def *weapon, bool no_message = false);
int calc_heavy_armour_penalty(bool random_factor);

unchivalric_attack_type is_unchivalric_attack(const actor *attacker,
                                              const actor *defender);

void chaos_affect_actor(actor *victim);
void get_cleave_targets(const actor* attacker, const coord_def& def, int dir,
                        list<actor*> &targets);
void get_all_cleave_targets(const actor* attacker, const coord_def& def,
                            list<actor*> &targets);
void attack_cleave_targets(actor* attacker, list<actor*> &targets,
                           int attack_number = 0,
                           int effective_attack_number = 0);

int weapon_min_delay(const item_def &weapon);
int finesse_adjust_delay(int delay);

bool conduction_affected(const coord_def &pos);
#endif
