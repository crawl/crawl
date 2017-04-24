/**
 * @file
 * @brief Tracks monsters that are in suspended animation between levels.
**/

#include "AppHdr.h"

#include "mon-transit.h"

#include <algorithm>

#include "artefact.h"
#include "coordit.h"
#include "dactions.h"
#include "dungeon.h"
#include "god-companions.h"
#include "god-passive.h" // passive_t::convert_orcs
#include "items.h"
#include "libutil.h" // map_find
#include "mon-place.h"
#include "religion.h"

#define MAX_LOST 100

monsters_in_transit the_lost_ones;

static void _cull_lost_mons(m_transit_list &mlist, int how_many)
{
    // First pass, drop non-uniques.
    for (auto i = mlist.begin(); i != mlist.end();)
    {
        auto finger = i++;
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

/**
 * Get the monster transit list for the given level.
 * @param lid    The level.
 * @returns      The monster transit list.
 **/
m_transit_list *get_transit_list(const level_id &lid)
{
    return map_find(the_lost_ones, lid);
}

/**
 * Add a monster to a level's transit list.
 * @param lid    The level.
 * @param m      The monster to add.
 **/
void add_monster_to_transit(const level_id &lid, const monster& m)
{
    ASSERT(m.alive());

    m_transit_list &mlist = the_lost_ones[lid];
    mlist.emplace_back(m);

    dprf("Monster in transit to %s: %s", lid.describe().c_str(),
         m.name(DESC_PLAIN, true).c_str());

    if (m.is_divine_companion())
        move_companion_to(&m, lid);

    const int how_many = mlist.size();
    if (how_many > MAX_LOST)
        _cull_lost_mons(mlist, how_many);
}

/**
 * Remove a monster from a level's transit list.
 * @param lid    The level.
 * @param mid    The mid_t of the monster to remove.
 **/
void remove_monster_from_transit(const level_id &lid, mid_t mid)
{
    m_transit_list &mlist = the_lost_ones[lid];

    for (auto i = mlist.begin(); i != mlist.end(); ++i)
    {
        if (i->mons.mid == mid)
        {
            mlist.erase(i);
            return;
        }
    }
}

static void _level_place_followers(m_transit_list &m)
{
    for (auto i = m.begin(); i != m.end();)
    {
        auto mon = i++;
        if ((mon->mons.flags & MF_TAKING_STAIRS) && mon->place(true))
        {
            if (mon->mons.is_divine_companion())
            {
                move_companion_to(monster_by_mid(mon->mons.mid),
                                                 level_id::current());
            }

            // Now that the monster is onlevel, we can safely apply traps to it.
            if (monster* new_mon = monster_by_mid(mon->mons.mid))
                // old loc isn't really meaningful
                new_mon->apply_location_effects(new_mon->pos());
            m.erase(mon);
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

/**
 * Place any followers transiting to this level.
 **/
void place_followers()
{
    _place_lost_ones(_level_place_followers);
}

static bool _place_lost_monster(follower &f)
{
    dprf("Placing lost one: %s", f.mons.name(DESC_PLAIN, true).c_str());
    return f.place(false);
}

static void _level_place_lost_monsters(m_transit_list &m)
{
    for (auto i = m.begin(); i != m.end(); )
    {
        auto mon = i++;

        // Monsters transiting to the Abyss have a 50% chance of being
        // placed, otherwise a 100% chance.
        if (player_in_branch(BRANCH_ABYSS) && coinflip())
            continue;

        if (_place_lost_monster(*mon))
        {
            // Now that the monster is on the level, we can safely apply traps
            // to it.
            if (monster* new_mon = monster_by_mid(mon->mons.mid))
                // old loc isn't really meaningful
                new_mon->apply_location_effects(new_mon->pos());
            m.erase(mon);
        }
    }
}

/**
 * Place any monsters in transit to this level.
 **/
void place_transiting_monsters()
{
    _place_lost_ones(_level_place_lost_monsters);
}

void apply_daction_to_transit(daction_type act)
{
    for (auto &entry : the_lost_ones)
    {
        m_transit_list* m = &entry.second;
        for (auto j = m->begin(); j != m->end(); ++j)
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
    for (const auto &entry : the_lost_ones)
    {
        for (const auto &follower : entry.second)
            if (mons_matches_daction(&follower.mons, act))
                count++;
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
            it.set_holding_monster(m);
        }
    }
}

static bool _is_religious_follower(const monster &mon)
{
    return (you_worship(GOD_YREDELEMNUL)
            || will_have_passive(passive_t::convert_orcs)
            || you_worship(GOD_FEDHAS))
                && is_follower(mon);
}

static bool _mons_can_follow_player_from(const monster &mons,
                                         const coord_def from,
                                         bool within_level = false)
{
    if (!mons.alive()
        || mons.speed_increment < 50
        || mons.incapacitated()
        || mons.is_stationary())
    {
        return false;
    }

    if (!monster_habitable_grid(&mons, DNGN_FLOOR))
        return false;

    // Only non-wandering friendly monsters or those actively
    // seeking the player will follow up/down stairs.
    if (!mons.friendly()
          && (!mons_is_seeking(mons) || mons.foe != MHITYOU)
        || mons.foe == MHITNOT)
    {
        return false;
    }

    // Unfriendly monsters must be directly adjacent to follow.
    if (!mons.friendly() && (mons.pos() - from).rdist() > 1)
        return false;

    // Monsters that can't use stairs can still be marked as followers
    // (though they'll be ignored for transit), so any adjacent real
    // follower can follow through. (jpeg)
    if (within_level && !mons_class_can_use_transporter(mons.type)
        || !within_level && !mons_can_use_stairs(mons, grd(from)))
    {
        if (_is_religious_follower(mons))
            return true;

        return false;
    }
    return true;
}

// Tag any monster following the player
static bool _tag_follower_at(const coord_def &pos, const coord_def &from,
                             bool &real_follower)
{
    if (!in_bounds(pos) || pos == from)
        return false;

    monster* fol = monster_at(pos);
    if (fol == nullptr)
        return false;

    if (!_mons_can_follow_player_from(*fol, from))
        return false;

    real_follower = true;
    fol->flags |= MF_TAKING_STAIRS;

    // Clear patrolling/travel markers.
    fol->patrol_point.reset();
    fol->travel_path.clear();
    fol->travel_target = MTRAV_NONE;

    fol->clear_clinging();

    dprf("%s is marked for following.", fol->name(DESC_THE, true).c_str());
    return true;
}

static int _follower_tag_radius(const coord_def &from)
{
    // If only friendlies are adjacent, we set a max radius of 5, otherwise
    // only adjacent friendlies may follow.
    for (adjacent_iterator ai(from); ai; ++ai)
    {
        if (const monster* mon = monster_at(*ai))
            if (!mon->friendly())
                return 1;
    }

    return 5;
}

/**
 * Handle movement of adjacent player followers from a given location. This is
 * used when traveling through stairs or a transporter.
 *
 * @param from       The location from which the player moved.
 * @param handler    A handler function that does movement of the actor to the
 *                   destination, returning true if the actor was friendly. The
 *                   `real` argument tracks whether the actor was an actual
 *                   follower that counts towards the follower limit.
 **/
void handle_followers(const coord_def &from,
                      bool (*handler)(const coord_def &pos,
                                      const coord_def &from, bool &real))
{
    const int radius = _follower_tag_radius(from);
    int n_followers = 18;

    vector<coord_def> places[2];
    int place_set = 0;

    places[place_set].push_back(from);
    memset(travel_point_distance, 0, sizeof(travel_distance_grid_t));
    while (!places[place_set].empty())
    {
        for (const coord_def p : places[place_set])
        {
            for (adjacent_iterator ai(p); ai; ++ai)
            {
                if ((*ai - from).rdist() > radius
                    || travel_point_distance[ai->x][ai->y])
                {
                    continue;
                }
                travel_point_distance[ai->x][ai->y] = 1;

                bool real_follower = false;
                if (handler(*ai, from, real_follower))
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

/**
 * Tag all followers at your position for level transit through stairs.
 **/
void tag_followers()
{
    handle_followers(you.pos(), _tag_follower_at);
}

/**
 * Untag all followers at your position for level transit through stairs.
 **/
void untag_followers()
{
    for (auto &mons : menv_real)
        mons.flags &= ~MF_TAKING_STAIRS;
}

static bool _transport_follower_at(const coord_def &pos, const coord_def &from,
                                   bool &real_follower)
{
    if (!in_bounds(pos) || pos == from)
        return false;

    monster* fol = monster_at(pos);
    if (fol == nullptr)
        return false;

    if (!_mons_can_follow_player_from(*fol, from, true))
        return false;

    if (fol->find_place_to_live(true))
    {
        real_follower = true;
        dprf("%s is transported.", fol->name(DESC_THE, true).c_str());
    }

    return true;
}

/**
 * Transport all followers from a position to a position near your current one.
 *
 * @param from    The position from where you transported.
 **/
void transport_followers_from(const coord_def &from)
{
    handle_followers(from, _transport_follower_at);
}
