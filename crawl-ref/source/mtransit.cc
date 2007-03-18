/*
 * File:       mtransit.cc
 * Summary:    Tracks monsters that are in suspended animation between levels.
 * Written by: Darshan Shaligram
 *
 *  Modified for Crawl Reference by $Author: dshaligram $ on $Date: 2007-03-15T20:10:20.648083Z $
 */

#include "AppHdr.h"

#include "mtransit.h"
#include "items.h"
#include "mon-util.h"
#include "stuff.h"

#define MAX_LOST 100

monsters_in_transit the_lost_ones;

static void place_lost_monsters(m_transit_list &m);

static void cull_lost(m_transit_list &mlist, int how_many)
{
    // First pass, drop non-uniques.
    for (m_transit_list::iterator i = mlist.begin(); i != mlist.end(); )
    {
        m_transit_list::iterator finger = i++;
        if (!mons_is_unique(finger->mons.type))
        {
            mlist.erase(finger);

            if (--how_many <= MAX_LOST)
                return;
        }
    }

    // If we're still over the limit (unlikely), just lose
    // the old ones.
    while (how_many-- > MAX_LOST && !mlist.empty())
        mlist.erase( mlist.begin() );
}

void add_monster_to_transit(level_id lid, const monsters &m)
{
    m_transit_list &mlist = the_lost_ones[lid];
    mlist.push_back(m);

#ifdef DEBUG_DIAGNOSTICS
    mprf(MSGCH_DIAGNOSTICS, "Monster in transit: %s",
         m.name(DESC_PLAIN).c_str());
#endif

    const int how_many = mlist.size();
    if (how_many > MAX_LOST)
        cull_lost(mlist, how_many);
}

void place_transiting_monsters()
{
    level_id c = level_id::current();

    monsters_in_transit::iterator i = the_lost_ones.find(c);
    if (i == the_lost_ones.end())
        return;

    place_lost_monsters(i->second);
    if (i->second.empty())
        the_lost_ones.erase(i);
}

static bool place_lost_monster(follower &f)
{
    return (f.place(false));
}

static void place_lost_monsters(m_transit_list &m)
{
    for (m_transit_list::iterator i = m.begin();
         i != m.end(); )
    {
        m_transit_list::iterator mon = i++;

        // Transiting monsters have a 50% chance of being placed.
        if (coinflip())
            continue;

        if (place_lost_monster(*mon))
            m.erase(mon);
    }
}

//////////////////////////////////////////////////////////////////////////
// follower

follower::follower(const monsters &m) : mons(m), items()
{
    load_mons_items();
}

void follower::load_mons_items()
{
    for (int i = 0; i < NUM_MONSTER_SLOTS; ++i)
        if (mons.inv[i] != NON_ITEM)
            items[i] = mitm[ mons.inv[i] ];
        else
            items[i].clear();
}

bool follower::place(bool near_player)
{
    for (int i = 0; i < MAX_MONSTERS - 5; ++i)
    {
        monsters &m = menv[i];
        if (m.alive())
            continue;

        m = mons;
        if (m.find_place_to_live(near_player))
        {
#ifdef DEBUG_DIAGNOSTICS
            mprf(MSGCH_DIAGNOSTICS, "Placed follower: %s",
                 m.name(DESC_PLAIN).c_str());
#endif
            m.target_x = m.target_y = 0;
            m.flags |= MF_JUST_SUMMONED;
            
            restore_mons_items(m);
            return (true);
        }

        m.reset();
        break;
    }
    
    return (false);
}

void follower::restore_mons_items(monsters &m)
{
    for (int i = 0; i < NUM_MONSTER_SLOTS; ++i)
    {
        if (items[i].base_type == OBJ_UNASSIGNED)
            m.inv[i] = NON_ITEM;
        else
        {
            const int islot = get_item_slot(0);
            m.inv[i] = islot;
            if (islot == NON_ITEM)
                continue;

            item_def &it = mitm[islot];
            it = items[i];
            it.x = it.y = 0;
            it.link = NON_ITEM;
        }
    }
}
