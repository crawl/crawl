/**
 * @file
 * @brief Item creation routines.
**/

#ifndef MAKEITEM_H
#define MAKEITEM_H

#include "itemprop-enum.h"

int create_item_named(string name, coord_def pos, string *error);

int items(bool allow_uniques, object_class_type force_class, int force_type,
          int item_level, int force_ego = 0, int agent = -1);

void item_colour(item_def &item);
void init_rod_mp(item_def &item, int ncharges = -1, int item_level = -1);

jewellery_type get_random_ring_type();
jewellery_type get_random_amulet_type();
void item_set_appearance(item_def &item);

bool is_weapon_brand_ok(int type, int brand, bool strict);
bool is_armour_brand_ok(int type, int brand, bool strict);
bool is_missile_brand_ok(int type, int brand, bool strict);

int determine_nice_weapon_plusses(int item_level);
brand_type determine_weapon_brand(const item_def& item, int item_level);

bool got_curare_roll(const int item_level);
void reroll_brand(item_def &item, int item_level);

bool is_high_tier_wand(int type);

void squash_plusses(int item_slot);

#if defined(DEBUG_DIAGNOSTICS) || defined(DEBUG_TESTS)
void makeitem_tests();
#endif
#endif
