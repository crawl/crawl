/*
 * File:       makeitem.h
 * Summary:    Item creation routines.
 *
 *  Modified for Crawl Reference by $Author: haranp $ on $Date: 2007-03-15T20:10:20.648083Z $
 */

#ifndef MAKEITEM_H
#define MAKEITEM_H

#include "dungeon.h"

int items( int allow_uniques, int force_class, int force_type, 
           bool dont_place, int item_level, int item_race,
           const dgn_region_list &forbidden = dgn_region_list() );

void item_colour( item_def &item );
void init_rod_mp(item_def &item);
void give_item(int mid, int level_number);

#endif
