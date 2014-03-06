/**
 * @file
 * @brief Functions used during combat.
**/

#ifndef FIGHT_H
#define FIGHT_H

#include <list>

#include "random-var.h"

enum stab_type
{
    STAB_NO_STAB,                    //    0
    STAB_DISTRACTED,
    STAB_CONFUSED,
    STAB_FLEEING,
    STAB_INVISIBLE,
    STAB_HELD_IN_NET,
    STAB_PETRIFYING,
    STAB_PETRIFIED,
    STAB_PARALYSED,
    STAB_SLEEPING,
    STAB_ALLY,
    NUM_STAB
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

stab_type find_stab_type(const actor *attacker,
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
#endif
