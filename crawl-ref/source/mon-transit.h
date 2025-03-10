/**
 * @file
 * @brief Tracking monsters in transit between levels.
**/

#pragma once

#include <list>
#include <map>

#include "monster.h"
#include "tag-version.h"

struct follower
{
    monster mons;
    FixedVector<item_def, NUM_MONSTER_SLOTS> items;
    int transit_start_time;

    follower() : mons(), items() { }
    follower(const monster& m);

    // if placement was successful, returns a pointer to the placed monster
    monster* place(bool near_player = false);
    void load_mons_items();
    void restore_mons_items(monster& m);

    // Serialize piecemeal, to allow storing in a CrawlStoreValue
    void write_to_prop(CrawlVector& vec);
    void read_from_prop(CrawlVector& vec);

    // Reconstitutes the monster and their equipment, but does NOT place the
    // monster onto the floor. Use to examine stats that include equipment,
    // but may not be safe for other purposes.
    monster* peek();
};

// Several erase() calls rely on this being a linked list (so erasing does not
// invalidate the iterators).
typedef list<follower> m_transit_list;
typedef map<level_id, m_transit_list> monsters_in_transit;

// This one too.
#if TAG_MAJOR_VERSION == 34
typedef list<item_def> i_transit_list;
typedef map<level_id, i_transit_list> items_in_transit;
#endif

extern monsters_in_transit the_lost_ones;

void transit_lists_clear();

m_transit_list *get_transit_list(const level_id &where);
void add_monster_to_transit(const level_id &dest, const monster& m);

void remove_monster_from_transit(const level_id &lid, mid_t mid);

// Places (some of the) monsters eligible to be placed on this level.
void place_transiting_monsters();
void place_followers();
void handle_followers(const coord_def &from,
                      bool (*handler)(const coord_def &pos,
                                      const coord_def &from));
void tag_followers();
void untag_followers();
void transport_followers_from(const coord_def &from);

void apply_daction_to_transit(daction_type act);
int count_daction_in_transit(daction_type act);
