/*
 *********************************************************************
 *  File:       wpn-misc.h                                           *
 *  Summary:    temporary home for weapon f(x) until struct'ed       *
 *  Written by: don brodale <dbrodale@bigfootinteractive.com>        *
 *                                                                   *
 *  Changelog(most recent first):                                    *
 *                                                                   *
 *  <00>     12jun2000     dlb     created after little thought      *
 *********************************************************************
*/


#ifndef WPNMISC_H
#define WPNMISC_H

#include "externs.h"


/* ***********************************************************************
 * called from: food.h 
 * *********************************************************************** */
bool can_cut_meat(int wclass, int wtype);

/* ***********************************************************************
 * called from: acr - fight - food - item_use - itemname - spells2
 * *********************************************************************** */
int damage_type(int wclass, int wtype);
int damage_type(const item_def &item);


// last updated: 10jun2000 {dlb}
/* ***********************************************************************
 * called from: describe - fight - item_use
 * *********************************************************************** */
int hands_reqd_for_weapon(int wclass, int wtype);


// last updated: 10jun2000 {dlb}
/* ***********************************************************************
 * called from: dungeon - fight - item_use - randart
 * *********************************************************************** */
bool is_demonic(unsigned char weapon_subtype);


// last updated: 10jun2000 {dlb}
/* ***********************************************************************
 * called from: dungeon - item_use - mstuff2
 * *********************************************************************** */
unsigned char launched_by(unsigned char weapon_subtype);


// last updated: 10jun2000 {dlb}
/* ***********************************************************************
 * called from: describe - dungeon - fight - item_use - mstuff2 - randart -
 *              spells2 - spells3
 * *********************************************************************** */
bool launches_things( unsigned char weapon_subtype );


#endif
