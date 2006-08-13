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
bool can_cut_meat(unsigned char wclass, unsigned char wtype);

/* ***********************************************************************
 * called from: acr - fight - food - item_use - itemname - spells2
 * *********************************************************************** */
char damage_type(unsigned char wclass, unsigned char wtype);


// last updated: 10jun2000 {dlb}
/* ***********************************************************************
 * called from: describe - fight - item_use
 * *********************************************************************** */
int hands_reqd_for_weapon(unsigned char wclass, unsigned char wtype);


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


// last updated: 10jun2000 {dlb}
/* ***********************************************************************
 * called from: describe - fight - files - it_use3 - newgame - spells1
 * *********************************************************************** */
char weapon_skill(unsigned char wclass, unsigned char wtype);


#endif
