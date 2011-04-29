/*
 *  File:       skills.cc
 *  Summary:    Skill exercising functions.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author: dshaligram $ on $Date: 2006-11-22 08:41:20 +0000 (Wed, 22 Nov 2006) $
 *
 *  Modified for Hexcrawl by Martin Bays, 2007
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
int exercise(int exsk, int deg);

#endif
