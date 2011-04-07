/**
 * @file
 * @brief Skill exercising functions.
**/


#ifndef SKILLS_H
#define SKILLS_H

int skill_cost_needed(int level);
void calc_total_skill_points(void);
int calc_skill_cost(int skill_cost_level, int skill_level);

void reassess_starting_skills();
void check_skill_level_change(skill_type sk, bool do_level_up = true);
void change_skill_level(skill_type exsk, int num_level);
void change_skill_points(skill_type sk, int points, bool do_level_up);

int exercise(skill_type exsk, int deg);

#endif
