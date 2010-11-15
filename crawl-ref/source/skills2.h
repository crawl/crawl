/*
 *  File:       skills2.h
 *  Summary:    More skill related functions.
 *  Written by: Linley Henzell
 */


#ifndef SKILLS2_H
#define SKILLS2_H

const int MAX_SKILL_ORDER = 100;

#include "enum.h"
#include "player.h"

enum sk_menu_flags
{
    SK_MENU_NONE         = 0,
    SK_MENU_SHOW_APT     = 0x01,
    SK_MENU_SHOW_DESC    = 0x02,
    SK_MENU_SHOW_RESKILL = 0x04,
    SK_MENU_SHOW_ALL     = 0x08,
    SK_MENU_RESKILL      = 0x10
};

const char *skill_name(skill_type which_skill);
skill_type str_to_skill(const std::string &skill);

std::string skill_title(
    skill_type best_skill, uint8_t skill_lev,
    // these used for ghosts and hiscores:
    int species = -1, int str = -1, int dex = -1, int god = -1);
std::string skill_title_by_rank(
    skill_type best_skill, uint8_t skill_rank,
    // these used for ghosts and hiscores:
    int species = -1, int str = -1, int dex = -1, int god = -1);
unsigned get_skill_rank(unsigned skill_lev);

std::string player_title();

skill_type best_skill(skill_type min_skill, skill_type max_skill,
                      skill_type excl_skill = SK_NONE);
void init_skill_order();

void calc_mp();
void calc_hp();

int species_apt(skill_type skill, species_type species = you.species);
float species_apt_factor(skill_type sk, species_type sp = you.species);
unsigned int skill_exp_needed(int lev);
unsigned int skill_exp_needed(int lev, skill_type sk,
                              species_type sp = you.species);

float crosstrain_bonus(skill_type sk);
bool crosstrain_other(skill_type sk);
bool is_antitrained(skill_type sk);
bool antitrain_other(skill_type sk);

void show_skills();
void wield_warning(bool newWeapon = true);
bool is_invalid_skill(skill_type skill);
void dump_skills(std::string &text);
skill_type select_skill(skill_type from_skill = SK_NONE, int skill_points = 0,
                        bool show_all = false);
int transfer_skill_points(skill_type fsk, skill_type tsk, int skp_max,
                          bool simu);
#endif
