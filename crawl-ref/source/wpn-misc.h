/*
 *********************************************************************
 *  File:       wpn-misc.h                                           *
 *  Summary:    temporary home for weapon f(x) until struct'ed       *
 *  Written by: don brodale <dbrodale@bigfootinteractive.com>        *
 *                                                                   *
 *  Modified for Crawl Reference by $Author$ on $Date$               *
 *                                                                   *
 *  Changelog(most recent first):                                    *
 *                                                                   *
 *  <00>     12jun2000     dlb     created after little thought      *
 *********************************************************************
*/


#ifndef WPNMISC_H
#define WPNMISC_H

#include "externs.h"

// last updated: 10jun2000 {dlb}
/* ***********************************************************************
 * called from: describe - fight - item_use
 * *********************************************************************** */
int hands_reqd_for_weapon(int wclass, int wtype);

// last updated: 10jun2000 {dlb}
/* ***********************************************************************
 * called from: describe - dungeon - fight - item_use - mstuff2 - randart -
 *              spells2 - spells3
 * *********************************************************************** */
bool launches_things( unsigned char weapon_subtype );

// last updated: 10jun2000 {dlb}
/* ***********************************************************************
 * called from: dungeon - item_use - mstuff2
 * *********************************************************************** */
unsigned char launched_by(unsigned char weapon_subtype);

#endif
