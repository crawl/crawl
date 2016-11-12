/**
 * @file
 * @brief Functions used during combat.
**/

#ifndef FIGHT_H
#define FIGHT_H

#include <list>

#include "target.h"

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
    NUM_STABS
};

bool fight_melee(actor *attacker, actor *defender, bool *did_hit = nullptr,
                 bool simu = false);

int resist_adjust_damage(const actor *defender, beam_type flavour,
                         int rawdamage);

int apply_chunked_AC(int dam, int ac);

int melee_confuse_chance(int HD);

bool wielded_weapon_check(item_def *weapon);

stab_type find_stab_type(const actor *attacker,
                         const actor &defender,
                         bool actual = true);

int stab_bonus_denom(stab_type stab);

void get_cleave_targets(const actor &attacker, const coord_def& def,
                        list<actor*> &targets, int which_attack = -1);
void attack_cleave_targets(actor &attacker, list<actor*> &targets,
                           int attack_number = 0,
                           int effective_attack_number = 0);

int weapon_min_delay_skill(const item_def &weapon);
int weapon_min_delay(const item_def &weapon, bool check_speed = true);

int mons_weapon_damage_rating(const item_def &launcher);
int mons_missile_damage(monster* mons, const item_def *launch,
                        const item_def *missile);
int mons_usable_missile(monster* mons, item_def **launcher);

bool bad_attack(const monster *mon, string& adj, string& suffix,
                bool& would_cause_penance,
                coord_def attack_pos = coord_def(0, 0),
                bool check_landing_only = false);

bool stop_attack_prompt(const monster* mon, bool beam_attack,
                        coord_def beam_target, bool *prompted = nullptr,
                        coord_def attack_pos = coord_def(0, 0),
                        bool check_landing_only = false);

bool stop_attack_prompt(targetter &hitfunc, const char* verb,
                        bool (*affects)(const actor *victim) = 0,
                        bool *prompted = nullptr);

#endif
