/**
 * @file
 * @brief Functions used during combat.
**/

#pragma once

#include <list>
#include <functional>

#include "mon-enum.h"
#include "target.h"
#include "wu-jian-attack-type.h"

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

bool wielded_weapon_check(const item_def *weapon, string attack_verb="");

stab_type find_stab_type(const actor *attacker,
                         const actor &defender,
                         bool actual = true);

int stab_bonus_denom(stab_type stab);

bool force_player_cleave(coord_def target);
bool attack_cleaves(const actor &attacker, int which_attack = -1);
bool weapon_cleaves(const item_def &item);
void get_cleave_targets(const actor &attacker, const coord_def& def,
                        list<actor*> &targets, int which_attack = -1);
void attack_cleave_targets(actor &attacker, list<actor*> &targets,
                           int attack_number = 0,
                           int effective_attack_number = 0,
                           wu_jian_attack_type wu_jian_attack
                               = WU_JIAN_ATTACK_NONE,
                           bool is_projected = false);

class attack;
int to_hit_pct(const monster_info& mi, attack &atk, bool melee);
int mon_to_hit_base(int hd, bool skilled);
int mon_to_hit_pct(int to_land, int ev);
int mon_shield_bypass(int hd);
int mon_beat_sh_pct(int shield_bypass, int shield_class);

int weapon_min_delay_skill(const item_def &weapon);
int weapon_min_delay(const item_def &weapon, bool check_speed = true);

int mons_weapon_damage_rating(const item_def &launcher);

bool bad_attack(const monster *mon, string& adj, string& suffix,
                bool& would_cause_penance,
                coord_def attack_pos = coord_def(0, 0));

bool stop_attack_prompt(const monster* mon, bool beam_attack,
                        coord_def beam_target, bool *prompted = nullptr,
                        coord_def attack_pos = coord_def(0, 0));

bool stop_attack_prompt(targeter &hitfunc, const char* verb,
                        function<bool(const actor *victim)> affects = nullptr,
                        bool *prompted = nullptr,
                        const monster *mons = nullptr);

string stop_summoning_reason(resists_t resists, monclass_flags_t flags);
bool stop_summoning_prompt(resists_t resists = MR_NO_FLAGS,
                           string verb = "summon");

bool can_reach_attack_between(coord_def source, coord_def target,
                              reach_type range);
dice_def spines_damage(monster_type mon);
int archer_bonus_damage(int hd);

int aux_to_hit();

bool weapon_uses_strength(skill_type wpn_skill, bool using_weapon);
int stat_modify_damage(int base_dam, skill_type wpn_skill, bool using_weapon);
int apply_weapon_skill(int base_dam, skill_type wpn_skill, bool random);
int apply_fighting_skill(int base_dam, bool aux, bool random);
int throwing_base_damage_bonus(const item_def &projectile);

int unarmed_base_damage();
int unarmed_base_damage_bonus(bool random);
