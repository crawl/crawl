/**
 * @file
 * @brief Item creation routines.
**/

#ifndef MAKEITEM_H
#define MAKEITEM_H

#include "itemprop-enum.h"

int create_item_named(string name, coord_def pos, string *error);

int items(bool allow_uniques, object_class_type force_class, int force_type,
           bool dont_place, int item_level,
           int rune_type = 0,
           uint32_t mapmask = 0, int force_ego = 0, int agent = -1,
           bool mundane = false);

void item_colour(item_def &item);
void init_rod_mp(item_def &item, int ncharges = -1, int item_level = -1);

jewellery_type get_random_ring_type();
jewellery_type get_random_amulet_type();
void item_set_appearance(item_def &item);

bool is_weapon_brand_ok(int type, int brand, bool strict);
bool is_armour_brand_ok(int type, int brand, bool strict);
bool is_missile_brand_ok(int type, int brand, bool strict);

bool got_curare_roll(const int item_level);
void reroll_brand(item_def &item, int item_level);

bool is_high_tier_wand(int type);

#if defined(DEBUG_DIAGNOSTICS) || defined(DEBUG_TESTS)
void makeitem_tests();
#endif
#endif
