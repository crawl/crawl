/*
 * File:       mtransit.cc
 * Summary:    Tracks monsters that are in suspended animation between levels.
 * Written by: Darshan Shaligram
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 */

#include "AppHdr.h"
REVISION("$Rev$");

#include <algorithm>

#include "mtransit.h"

#include "dungeon.h"
#include "items.h"
#include "monplace.h"
#include "mon-util.h"
#include "monstuff.h"
#include "randart.h"
#include "stuff.h"

#define MAX_LOST 100

monsters_in_transit the_lost_ones;
items_in_transit    transiting_items;

static void level_place_lost_monsters(m_transit_list &m);
static void level_place_followers(m_transit_list &m);

static void cull_lost_mons(m_transit_list &mlist, int how_many)
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

static void cull_lost_items(i_transit_list &ilist, int how_many)
{
    // First pass, drop non-artifacts.
    for (i_transit_list::iterator i = ilist.begin(); i != ilist.end(); )
    {
        i_transit_list::iterator finger = i++;
        if (!is_artefact(*finger))
        {
            ilist.erase(finger);

            if (--how_many <= MAX_LOST)
                return;
        }
    }

    // Second pass, drop randarts.
    for (i_transit_list::iterator i = ilist.begin(); i != ilist.end(); )
    {
        i_transit_list::iterator finger = i++;
        if (is_random_artefact(*finger))
        {
            ilist.erase(finger);

            if (--how_many <= MAX_LOST)
                return;
        }
    }

    // Third pass, drop unrandarts.
    for (i_transit_list::iterator i = ilist.begin(); i != ilist.end(); )
    {
        i_transit_list::iterator finger = i++;
        if (is_unrandom_artefact(*finger))
        {
            ilist.erase(finger);

            if (--how_many <= MAX_LOST)
                return;
        }
    }

    // If we're still over the limit (unlikely), just lose
    // the old ones.
    while (how_many-- > MAX_LOST && !ilist.empty())
        ilist.erase( ilist.begin() );
}

m_transit_list *get_transit_list(const level_id &lid)
{
    monsters_in_transit::iterator i = the_lost_ones.find(lid);
    return (i != the_lost_ones.end()? &i->second : NULL);
}

void add_monster_to_transit(const level_id &lid, const monsters &m)
{
    m_transit_list &mlist = the_lost_ones[lid];
    mlist.push_back(m);

#ifdef DEBUG_DIAGNOSTICS
    mprf(MSGCH_DIAGNOSTICS, "Monster in transit: %s",
         m.name(DESC_PLAIN).c_str());
#endif

    const int how_many = mlist.size();
    if (how_many > MAX_LOST)
        cull_lost_mons(mlist, how_many);
}

void place_lost_ones(void (*placefn)(m_transit_list &ml))
{
    level_id c = level_id::current();

    monsters_in_transit::iterator i = the_lost_ones.find(c);
    if (i == the_lost_ones.end())
        return;
    placefn(i->second);
    if (i->second.empty())
        the_lost_ones.erase(i);
}

void place_transiting_monsters()
{
    place_lost_ones(level_place_lost_monsters);
}

void place_followers()
{
    place_lost_ones(level_place_followers);
}

static bool place_lost_monster(follower &f)
{
#ifdef DEBUG_DIAGNOSTICS
    mprf(MSGCH_DIAGNOSTICS, "Placing lost one: %s",
         f.mons.name(DESC_PLAIN).c_str());
#endif
    return (f.place(false));
}

static void level_place_lost_monsters(m_transit_list &m)
{
    for (m_transit_list::iterator i = m.begin();
         i != m.end(); )
    {
        m_transit_list::iterator mon = i++;

        // Monsters transiting to the Abyss have a 50% chance of being
        // placed, otherwise a 100% chance.
        if (you.level_type == LEVEL_ABYSS && coinflip())
            continue;

        if (place_lost_monster(*mon))
            m.erase(mon);
    }
}

static void level_place_followers(m_transit_list &m)
{
    for (m_transit_list::iterator i = m.begin(); i != m.end(); )
    {
        m_transit_list::iterator mon = i++;
        if ((mon->mons.flags & MF_TAKING_STAIRS) && mon->place(true))
            m.erase(mon);
    }
}

void add_item_to_transit(const level_id &lid, const item_def &i)
{
    i_transit_list &ilist = transiting_items[lid];
    ilist.push_back(i);

#ifdef DEBUG_DIAGNOSTICS
    mprf(MSGCH_DIAGNOSTICS, "Item in transit: %s",
         i.name(DESC_PLAIN).c_str());
#endif

    const int how_many = ilist.size();
    if (how_many > MAX_LOST)
        cull_lost_items(ilist, how_many);
}

void place_transiting_items()
{
    level_id c = level_id::current();

    items_in_transit::iterator i = transiting_items.find(c);
    if (i == transiting_items.end())
        return;

    i_transit_list &ilist = i->second;
    i_transit_list keep;
    i_transit_list::iterator item;

    for (item = ilist.begin(); item != ilist.end(); item++)
    {
        coord_def pos = item->pos;

        if (!in_bounds(pos))
            pos = random_in_bounds();

        const coord_def where_to_go =
            dgn_find_nearby_stair(DNGN_ESCAPE_HATCH_DOWN,
                                  pos, true);

        // List of items we couldn't place.
        if (!copy_item_to_grid(*item, where_to_go))
            keep.push_back(*item);
    }

    // Only unplaceable items are kept in list.
    ilist = keep;
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
        // Find first empty slot in menv and copy monster into it.
        monsters &m = menv[i];
        if (m.alive())
            continue;
        m = mons;

        bool placed = false;

        // In certain instances (currently, falling through a shaft)
        // try to place monster a close as possible to its previous
        // <x,y> coordinates.
        if (!near_player && you.level_type == LEVEL_DUNGEON
            && in_bounds(m.pos()))
        {
            const coord_def where_to_go =
                dgn_find_nearby_stair(DNGN_ESCAPE_HATCH_DOWN,
                                      m.pos(), true);

            if (where_to_go == you.pos())
                near_player = true;
            else if (m.find_home_near_place(where_to_go))
            {
                mgrd(m.pos()) = m.mindex();
                placed = true;
            }
        }

        if (!placed)
            placed = m.find_place_to_live(near_player);

        if (placed)
        {
#ifdef DEBUG_DIAGNOSTICS
            mprf(MSGCH_DIAGNOSTICS, "Placed follower: %s",
                 m.name(DESC_PLAIN).c_str());
#endif
            m.target.reset();

            m.flags &= ~MF_TAKING_STAIRS;
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
            it.pos.set(-2, -2);
            it.link = NON_ITEM + 1 + m.mindex();
        }
    }
}
