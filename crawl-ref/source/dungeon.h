/*
 *  File:       dungeon.cc
 *  Summary:    Functions used when building new levels.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 *
 *  Change History (most recent first):
 *
 *               <1>     -/--/--        LRH             Created
 */


#ifndef DUNGEON_H
#define DUNGEON_H

#include "FixVec.h"
#include "externs.h"

#define MAKE_GOOD_ITEM          351

void item_colour( item_def &item );

// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: files
 * *********************************************************************** */
void builder(int level_number, char level_type);


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: abyss - debug - dungeon - effects - religion - spells4
 * *********************************************************************** */
int items( int allow_uniques, int force_class, int force_type, 
           bool dont_place, int item_level, int item_race );

// last updated 13mar2001 {gdl}
/* ***********************************************************************
 * called from: dungeon monplace
 * *********************************************************************** */
void give_item(int mid, int level_number);


// last updated 13mar2001 {gdl}
/* ***********************************************************************
 * called from: dungeon monplace
 * *********************************************************************** */
void define_zombie(int mid, int ztype, int cs, int power);

bool is_wall(int feature);

bool place_specific_trap(unsigned char spec_x, unsigned char spec_y,
                         unsigned char spec_type);

void place_spec_shop(int level_number, unsigned char shop_x,
                         unsigned char shop_y, unsigned char force_s_type,
                         bool representative = false );

#endif
