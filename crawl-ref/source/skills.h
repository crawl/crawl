/**
 * @file
 * @brief Skill exercising functions.
**/


#ifndef SKILLS_H
#define SKILLS_H

#include "player.h"

int skill_cost_needed(int level);
void calc_total_skill_points(void);
int calc_skill_cost(int skill_cost_level);
void check_skill_cost_change();

void reassess_starting_skills();
void check_selected_skills();
void init_training();
void reset_training();
void check_skill_level_change(skill_type sk, bool do_level_up = true);
void change_skill_level(skill_type exsk, int num_level);
void change_skill_points(skill_type sk, int points, bool do_level_up);

void exercise(skill_type exsk, int deg);
void train_skills(bool simu = false);
void train_skills(int exp, const int cost, const bool simu);
void train_skill(skill_type skill, int exp);
void gain_skill(skill_type sk);
void lose_skill(skill_type sk);

inline bool skill_known(skill_type sk)
{
    return (you.skills[sk] || you.manual_skill == sk);
}

inline bool skill_known(int i)
{
    return skill_known(static_cast<skill_type>(i));
}

#endif
