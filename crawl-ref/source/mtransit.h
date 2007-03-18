/*
 * File:       mtransit.h
 * Summary:    Tracking monsters in transit between levels.
 * Written by: Darshan Shaligram
 *
 *  Modified for Crawl Reference by $Author: dshaligram $ on $Date: 2007-03-15T20:10:20.648083Z $
 */

#ifndef MTRANSIT_H
#define MTRANSIT_H

#include "AppHdr.h"
#include "travel.h"
#include <map>
#include <list>

struct follower
{
    monsters mons;
    FixedVector<item_def, NUM_MONSTER_SLOTS> items;

    follower() : mons(), items() { }
    follower(const monsters &m);
    bool place(bool near_player = false);
    void load_mons_items();
    void restore_mons_items(monsters &m);
};

typedef std::list<follower> m_transit_list;
typedef std::map<level_id, m_transit_list> monsters_in_transit;

extern monsters_in_transit the_lost_ones;

void add_monster_to_transit(level_id dest, const monsters &m);

// Places (some of the) monsters eligible to be placed on this level.
void place_transiting_monsters();

#endif
