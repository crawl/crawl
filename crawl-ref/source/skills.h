/*
 *  File:       skills.h
 *  Summary:    Skill exercising functions.
 *  Written by: Linley Henzell
 */


#ifndef SKILLS_H
#define SKILLS_H

int skill_cost_needed( int level );
void calc_total_skill_points( void );

// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: ability - bang - beam - debug - fight - it_use3 - item_use -
 *              items - misc - spell
 * *********************************************************************** */
int exercise(int exsk, int deg);

#endif
