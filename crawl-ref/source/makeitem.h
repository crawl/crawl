/*
 * File:       makeitem.h
 * Summary:    Item creation routines.
 * Created by: haranp on Sat Apr 21 11:31:42 2007 UTC
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 */

#ifndef MAKEITEM_H
#define MAKEITEM_H

#include "itemprop.h"

enum item_make_species_type
{
    MAKE_ITEM_ELVEN       = 1,
    MAKE_ITEM_DWARVEN     = 2,
    MAKE_ITEM_ORCISH      = 3,

    MAKE_ITEM_NO_RACE     = 100,
    MAKE_ITEM_RANDOM_RACE = 250
};

int items( int allow_uniques, object_class_type force_class, int force_type,
           bool dont_place, int item_level, int item_race,
           unsigned mapmask = 0, int force_ego = 0, int agent = -1 );

void item_colour(item_def &item);
void init_rod_mp(item_def &item, int ncharges = -1);
void give_item(int mid, int level_number, bool mons_summoned);

jewellery_type get_random_ring_type();
jewellery_type get_random_amulet_type();
armour_type get_random_body_armour_type(int level);
armour_type get_random_armour_type(int item_level);
void item_set_appearance(item_def &item);

#endif
