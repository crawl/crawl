/*
 * File:       makeitem.h
 * Summary:    Item creation routines.
 * Created by: haranp on Sat Apr 21 11:31:42 2007 UTC
 */

#ifndef MAKEITEM_H
#define MAKEITEM_H

#include "decks.h"
#include "itemprop-enum.h"

enum item_make_species_type
{
    MAKE_ITEM_ELVEN       = 1,
    MAKE_ITEM_DWARVEN     = 2,
    MAKE_ITEM_ORCISH      = 3,

    MAKE_ITEM_NO_RACE     = 100,
    MAKE_ITEM_RANDOM_RACE = 250,
};

int create_item_named(std::string name, coord_def pos,
                      std::string *error);

int items( int allow_uniques, object_class_type force_class, int force_type,
           bool dont_place, int item_level, int item_race,
           unsigned mapmask = 0, int force_ego = 0, int agent = -1 );

// Create a corpse item for the given monster with the supplied spec.
struct item_spec;
enum monster_type;
int item_corpse(monster_type monster, const item_spec &ispec);

void item_colour(item_def &item);
void init_rod_mp(item_def &item, int ncharges = -1, int item_level = -1);

int wand_max_charges(int subtype);
jewellery_type get_random_ring_type();
jewellery_type get_random_amulet_type();
armour_type get_random_body_armour_type(int level);
armour_type get_random_armour_type(int item_level);
void item_set_appearance(item_def &item);

bool is_weapon_brand_ok(int type, int brand, bool strict);
bool is_armour_brand_ok(int type, int brand, bool strict);
bool is_missile_brand_ok(int type, int brand, bool strict);

bool got_curare_roll(const int item_level);
void reroll_brand(item_def &item, int item_level);

deck_rarity_type random_deck_rarity();

#if defined(DEBUG_DIAGNOSTICS) || defined(DEBUG_TESTS)
void makeitem_tests();
#endif
#endif
