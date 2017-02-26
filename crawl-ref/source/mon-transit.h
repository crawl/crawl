/**
 * @file
 * @brief Tracking monsters in transit between levels.
**/

#pragma once

#include <list>
#include <map>

#include "monster.h"

struct follower
{
    monster mons;
    FixedVector<item_def, NUM_MONSTER_SLOTS> items;

    follower() : mons(), items() { }
    follower(const monster& m);
    bool place(bool near_player = false);
    void load_mons_items();
    void restore_mons_items(monster& m);
};

// Several erase() calls rely on this being a linked list (so erasing does not
// invalidate the iterators).
typedef list<follower> m_transit_list;
typedef map<level_id, m_transit_list> monsters_in_transit;

// This one too.
typedef list<item_def> i_transit_list;
typedef map<level_id, i_transit_list> items_in_transit;

extern monsters_in_transit the_lost_ones;
extern items_in_transit    transiting_items;

void transit_lists_clear();

m_transit_list *get_transit_list(const level_id &where);
void add_monster_to_transit(const level_id &dest, const monster& m);
void add_item_to_transit(const level_id &dest, const item_def &i);

void remove_monster_from_transit(const level_id &lid, mid_t mid);

// Places (some of the) monsters eligible to be placed on this level.
void place_transiting_monsters();
void place_followers();

void place_transiting_items();

void tag_followers();
void untag_followers();

void apply_daction_to_transit(daction_type act);
int count_daction_in_transit(daction_type act);
