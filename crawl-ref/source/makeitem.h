/*
 * File:       makeitem.h
 * Summary:    Item creation routines.
 *
 *  Modified for Crawl Reference by $Author: haranp $ on $Date: 2007-03-15T20:10:20.648083Z $
 */

#ifndef MAKEITEM_H
#define MAKEITEM_H

#include "dungeon.h"

int items( int allow_uniques, object_class_type force_class, int force_type, 
           bool dont_place, int item_level, int item_race,
           unsigned mapmask = 0 );

void item_colour( item_def &item );
void init_rod_mp(item_def &item);
void give_item(int mid, int level_number);

jewellery_type get_random_ring_type();
jewellery_type get_random_amulet_type();
armour_type get_random_body_armour_type(int level);
armour_type get_random_armour_type(int item_level);

#endif
