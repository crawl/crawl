/**
 * @file
 * @brief Skill exercising functions.
**/

#ifndef SKILLS_H
#define SKILLS_H

#include "player.h"

typedef set<skill_type> skill_set;
typedef set<skill_type>::iterator skill_set_iter;

string skill_names(skill_set &skills);

unsigned int skill_cost_needed(int level);
int calc_skill_cost(int skill_cost_level);
void check_skill_cost_change();

bool training_restricted(skill_type sk);
void reassess_starting_skills();
bool check_selected_skills();
void init_train();
void init_can_train();
void init_training();
void update_can_train();
void reset_training();
void check_skill_level_change(skill_type sk, bool do_level_up = true);
void change_skill_level(skill_type exsk, int num_level);
void change_skill_points(skill_type sk, int points, bool do_level_up);

bool is_magic_skill(skill_type sk);

void exercise(skill_type exsk, int deg);
void train_skills(bool simu = false);
void train_skill(skill_type skill, int exp);
bool skill_trained(int i);
static inline bool skill_trained(skill_type sk) { return skill_trained((int) sk); }
void redraw_skill(skill_type exsk, skill_type old_best_skill = SK_NONE);
void set_skill_level(skill_type skill, double amount);
#endif
