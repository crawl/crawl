/*
 *  File:       fight.cc
 *  Summary:    Functions used during combat.
 *  Written by: Linley Henzell
 *
 *  Change History (most recent first):
 *
 *               <1>     -/--/--        LRH             Created
 */


#ifndef FIGHT_H
#define FIGHT_H


#include "externs.h"

// added Sept 18, 2000 -- bwr
/* ***********************************************************************
 * called from: item_use.cc
 * *********************************************************************** */
int effective_stat_bonus( int wepType = -1 );

// added Sept 18, 2000 -- bwr
/* ***********************************************************************
 * called from: describe.cc
 * *********************************************************************** */
int weapon_str_weight( int wpn_class, int wpn_type );


// last updated: 08jun2000 {dlb}
/* ***********************************************************************
 * called from: acr - it_use3
 * *********************************************************************** */
void you_attack(int monster_attacked, bool unarmed_attacks);


// last updated: 08jun2000 {dlb}
/* ***********************************************************************
 * called from: monstuff
 * *********************************************************************** */
void monster_attack(int monster_attacking);


// last updated: 08jun2000 {dlb}
/* ***********************************************************************
 * called from: monstuff
 * *********************************************************************** */
bool monsters_fight(int monster_attacking, int monster_attacked);


#endif
