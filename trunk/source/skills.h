/*
 *  File:       skills.cc
 *  Summary:    Skill exercising functions.
 *  Written by: Linley Henzell
 *
 *  Change History (most recent first):
 *
 *               <1>     -/--/--        LRH             Created
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
void exercise(char exsk, int deg);


#endif
