/**
 * @file
 * @brief Tracks monsters that are in suspended animation between levels.
**/

#include "AppHdr.h"

#include <algorithm>

#include "mon-transit.h"

#include "artefact.h"
#include "coord.h"
#include "coordit.h"
#include "dactions.h"
#include "dungeon.h"
#include "env.h"
#include "godcompanions.h"
#include "items.h"
#include "mon-place.h"
#include "mon-util.h"
#include "random.h"
#include "religion.h"
#include "travel.h"

#define MAX_LOST 100

monsters_in_transit the_lost_ones;
items_in_transit    transiting_items;

void transit_lists_clear()
{
    the_lost_ones.clear();
    transiting_items.clear();
}

static void level_place_lost_monsters(m_transit_list &m);
static void level_place_followers(m_transit_list &m);

static void cull_lost_mons(m_transit_list &mlist, int how_many)
{
    // First pass, drop non-uniques.
    for (m_transit_list::iterator i = mlist.begin(); i != mlist.end();)
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
        mlist.erase(mlist.begin());
}

static void cull_lost_items(i_transit_list &ilist, int how_many)
{
    // First pass, drop non-artefacts.
    for (i_transit_list::iterator i = ilist.begin(); i != ilist.end();)
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
    for (i_transit_list::iterator i = ilist.begin(); i != ilist.end();)
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
    for (i_transit_list::iterator i = ilist.begin(); i != ilist.end();)
    {
        i_transit_list::iterator finger = i++;
        if (is_unrandom_artefact(*finger)
            && !is_special_unrandom_artefact(*finger))
        {
            ilist.erase(finger);

            if (--how_many <= MAX_LOST)
                return;
        }
    }

    // If we're still over the limit (unlikely), just lose
    // the old ones.
    while (how_many-- > MAX_LOST && !ilist.empty())
        ilist.erase(ilist.begin());
}

m_transit_list *get_transit_list(const level_id &lid)
{
    monsters_in_transit::iterator i = the_lost_ones.find(lid);
    return i != the_lost_ones.end()? &i->second : NULL;
}

void add_monster_to_transit(const level_id &lid, const monster& m)
{
    ASSERT(m.alive());

    m_transit_list &mlist = the_lost_ones[lid];
    mlist.push_back(follower(m));

    dprf("Monster in transit to %s: %s", lid.describe().c_str(),
         m.name(DESC_PLAIN, true).c_str());

    if (m.is_divine_companion())
        move_companion_to(&m, lid);

    const int how_many = mlist.size();
    if (how_many > MAX_LOST)
        cull_lost_mons(mlist, how_many);
}

void remove_monster_from_transit(const level_id &lid, mid_t mid)
{
    m_transit_list &mlist = the_lost_ones[lid];

    for (m_transit_list::iterator i = mlist.begin(); i != mlist.end(); ++i)
    {
        if (i->mons.mid == mid)
        {
            mlist.erase(i);
            return;
        }
    }
}

static void _place_lost_ones(void (*placefn)(m_transit_list &ml))
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
    _place_lost_ones(level_place_lost_monsters);
}

void place_followers()
{
    _place_lost_ones(level_place_followers);
}

static bool place_lost_monster(follower &f)
{
    dprf("Placing lost one: %s", f.mons.name(DESC_PLAIN, true).c_str());
    return f.place(false);
}

static void level_place_lost_monsters(m_transit_list &m)
{
    for (m_transit_list::iterator i = m.begin();
         i != m.end();)
    {
        m_transit_list::iterator mon = i++;

        // Monsters transiting to the Abyss have a 50% chance of being
        // placed, otherwise a 100% chance.
        if (player_in_branch(BRANCH_ABYSS) && coinflip())
            continue;

        if (place_lost_monster(*mon))
            m.erase(mon);
    }
}

static void level_place_followers(m_transit_list &m)
{
    for (m_transit_list::iterator i = m.begin(); i != m.end();)
    {
        m_transit_list::iterator mon = i++;
        if ((mon->mons.flags & MF_TAKING_STAIRS) && mon->place(true))
        {
            if (mon->mons.is_divine_companion())
                move_companion_to(monster_by_mid(mon->mons.mid), level_id::current());
            m.erase(mon);
        }
    }
}

void add_item_to_transit(const level_id &lid, const item_def &i)
{
    i_transit_list &ilist = transiting_items[lid];
    ilist.push_back(i);

    dprf("Item in transit: %s", i.name(DESC_PLAIN).c_str());

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

    for (item = ilist.begin(); item != ilist.end(); ++item)
    {
        coord_def pos = item->pos;

        if (!in_bounds(pos))
            pos = random_in_bounds();

        const coord_def where_to_go =
            dgn_find_nearby_stair(DNGN_ESCAPE_HATCH_DOWN,
                                  pos, true);

        // List of items we couldn't place.
        if (!copy_item_to_grid(*item, where_to_go, -1, false, true))
            keep.push_back(*item);
    }

    // Only unplaceable items are kept in list.
    ilist = keep;
}

void apply_daction_to_transit(daction_type act)
{
    for (monsters_in_transit::iterator i = the_lost_ones.begin();
            i != the_lost_ones.end(); ++i)
    {
        m_transit_list* m = &i->second;
        for (m_transit_list::iterator j = m->begin(); j != m->end(); ++j)
        {
            monster* mon = &j->mons;
            if (mons_matches_daction(mon, act))
                apply_daction_to_mons(mon, act, false, true);

            // If that killed the monster, remove it from transit.
            // Removing this monster invalidates the iterator that
            // points to it, so decrement the iterator first.
            if (!mon->alive())
                m->erase(j--);
        }
    }
}

int count_daction_in_transit(daction_type act)
{
    int count = 0;
    for (monsters_in_transit::iterator i = the_lost_ones.begin();
            i != the_lost_ones.end(); ++i)
    {
        m_transit_list* m = &i->second;
        for (m_transit_list::iterator j = m->begin(); j != m->end(); ++j)
        {
            monster* mon = &j->mons;
            if (mons_matches_daction(mon, act))
                count++;
        }
    }

    return count;
}

//////////////////////////////////////////////////////////////////////////
// follower

follower::follower(const monster& m) : mons(m), items()
{
    ASSERT(m.alive());
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
    ASSERT(mons.alive());

    monster *m = get_free_monster();
    if (!m)
        return false;

    // Copy the saved data.
    *m = mons;

    // Shafts no longer retain the position, if anything else would
    // want to request a specific one, it should do so here if !near_player

    if (m->find_place_to_live(near_player))
    {
        dprf("Placed follower: %s", m->name(DESC_PLAIN, true).c_str());
        m->target.reset();

        m->flags &= ~MF_TAKING_STAIRS & ~MF_BANISHED;
        m->flags |= MF_JUST_SUMMONED;
        restore_mons_items(*m);
        env.mid_cache[m->mid] = m->mindex();
        return true;
    }

    m->reset();
    return false;
}

void follower::restore_mons_items(monster& m)
{
    for (int i = 0; i < NUM_MONSTER_SLOTS; ++i)
    {
        if (items[i].base_type == OBJ_UNASSIGNED)
            m.inv[i] = NON_ITEM;
        else
        {
            const int islot = get_mitm_slot(0);
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

static bool _is_religious_follower(const monster* mon)
{
    return (you_worship(GOD_YREDELEMNUL)
            || you_worship(GOD_BEOGH)
            || you_worship(GOD_FEDHAS))
                && is_follower(mon);
}

static bool _tag_follower_at(const coord_def &pos, bool &real_follower)
{
    if (!in_bounds(pos) || pos == you.pos())
        return false;

    monster* fol = monster_at(pos);
    if (fol == NULL)
        return false;

    if (!fol->alive()
        || fol->speed_increment < 50
        || fol->incapacitated()
        || mons_is_boulder(fol)
        || fol->is_stationary())
    {
        return false;
    }

    if (!monster_habitable_grid(fol, DNGN_FLOOR))
        return false;

    // Only non-wandering friendly monsters or those actively
    // seeking the player will follow up/down stairs.
    if (!fol->friendly()
          && (!mons_is_seeking(fol) || fol->foe != MHITYOU)
        || fol->foe == MHITNOT)
    {
        return false;
    }

    // Unfriendly monsters must be directly adjacent to follow.
    if (!fol->friendly() && (pos - you.pos()).abs() > 2)
        return false;

    // Monsters that can't use stairs can still be marked as followers
    // (though they'll be ignored for transit), so any adjacent real
    // follower can follow through. (jpeg)
    if (!mons_can_use_stairs(fol))
    {
        if (_is_religious_follower(fol))
        {
            fol->flags |= MF_TAKING_STAIRS;
            return true;
        }
        return false;
    }

    real_follower = true;

    // Monster is chasing player through stairs.
    fol->flags |= MF_TAKING_STAIRS;

    // Clear patrolling/travel markers.
    fol->patrol_point.reset();
    fol->travel_path.clear();
    fol->travel_target = MTRAV_NONE;

    fol->clear_clinging();

    dprf("%s is marked for following.",
         fol->name(DESC_THE, true).c_str());

    return true;
}

static int follower_tag_radius2()
{
    // If only friendlies are adjacent, we set a max radius of 6, otherwise
    // only adjacent friendlies may follow.
    for (adjacent_iterator ai(you.pos()); ai; ++ai)
    {
        if (const monster* mon = monster_at(*ai))
            if (!mon->friendly())
                return 2;
    }

    return 6 * 6;
}

void tag_followers()
{
    const int radius2 = follower_tag_radius2();
    int n_followers = 18;

    vector<coord_def> places[2];
    int place_set = 0;

    places[place_set].push_back(you.pos());
    memset(travel_point_distance, 0, sizeof(travel_distance_grid_t));
    while (!places[place_set].empty())
    {
        for (int i = 0, size = places[place_set].size(); i < size; ++i)
        {
            const coord_def &p = places[place_set][i];
            for (adjacent_iterator ai(p); ai; ++ai)
            {
                if ((*ai - you.pos()).abs() > radius2
                    || travel_point_distance[ai->x][ai->y])
                {
                    continue;
                }
                travel_point_distance[ai->x][ai->y] = 1;

                bool real_follower = false;
                if (_tag_follower_at(*ai, real_follower))
                {
                    // If we've run out of our follower allowance, bail.
                    if (real_follower && --n_followers <= 0)
                        return;
                    places[!place_set].push_back(*ai);
                }
            }
        }
        places[place_set].clear();
        place_set = !place_set;
    }
}

void untag_followers()
{
    for (int m = 0; m < MAX_MONSTERS; ++m)
        menv[m].flags &= (~MF_TAKING_STAIRS);
}
