/**
 * @file
 * @brief Item creation routines.
**/

#pragma once

#include "item-prop-enum.h"

static const int NO_AGENT = -1;

// item_def::plus is a short
constexpr short GOOD_STAT_RING_PLUS = 6;
constexpr short GOOD_RING_PLUS = 4;

int create_item_named(string name, coord_def pos, string *error);

int items(bool allow_uniques, object_class_type force_class, int force_type,
          int item_level, int force_ego = 0, int agent = NO_AGENT,
          string custom_name = "",
          CrawlHashTable const *fixed_props = nullptr);

void item_colour(item_def &item);

jewellery_type get_random_ring_type();
jewellery_type get_random_amulet_type();
misc_item_type get_misc_item_type(int force_type, bool exclude = true);
void item_set_appearance(item_def &item);

bool is_weapon_brand_ok(int type, int brand, bool strict);
bool is_armour_brand_ok(int type, int brand, bool strict);
bool is_missile_brand_ok(int type, int brand, bool strict);

int determine_nice_weapon_plusses(int item_level);
brand_type determine_weapon_brand(const item_def& item, int item_level);

void set_artefact_brand(item_def &item, int brand);

bool got_curare_roll(const int item_level);
void reroll_brand(item_def &item, int item_level);

void generate_wand_item(item_def& item, int force_type, int item_level);
bool is_high_tier_wand(int type);

void squash_plusses(int item_slot);
#if defined(DEBUG_DIAGNOSTICS) || defined(DEBUG_TESTS)
void makeitem_tests();
#endif
