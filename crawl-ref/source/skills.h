/*
 *  File:       skills.h
 *  Summary:    Skill exercising functions.
 *  Written by: Linley Henzell
 */


#ifndef SKILLS_H
#define SKILLS_H

int skill_cost_needed(int level);
void calc_total_skill_points(void);

int check_skill_level_change(skill_type sk, bool just_check = false);
void change_skill_level(skill_type exsk, int num_level);
void change_skill_points(skill_type sk, int points, bool change_level = true);

bool is_antitrained(skill_type sk);
float crosstrain_bonus(skill_type exsk);


int exercise(skill_type exsk, int deg, bool change_level = true);

#endif
