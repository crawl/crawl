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
#include "god-abil.h"
#include "god-companions.h"
#include "god-passive.h" // passive_t::convert_orcs
#include "items.h"
#include "libutil.h" // map_find
#include "mon-behv.h"
#include "mon-place.h"
#include "mpr.h"
#include "religion.h"
#include "tag-version.h"
#include "terrain.h"
#include "timed-effects.h"

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
    mlist.back().transit_start_time = you.elapsed_time;

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
    if (c.branch == BRANCH_ABYSS)
    {
        c.depth = 1; // All monsters transiting to abyss are placed in Abyss:1.
                     // Look, don't ask me.
    }

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

static void _place_oka_duel_target(monster* mons)
{
    // It's possible for kobolds to have shorter LoS than the default min
    // placement distance, so place enemies closer to them.
    const int min_dist = min((int)you.current_vision, 3);
    int seen = 0;
    coord_def targ;
    for (radius_iterator ri(you.pos(), LOS_NO_TRANS); ri; ++ri)
    {
        if (cell_is_solid(*ri) || (env.pgrid(*ri) & FPROP_NO_TELE_INTO))
            continue;

        const int dist = grid_distance(you.pos(), *ri);
        if (dist > 5 || dist < min_dist)
            continue;

        if (one_chance_in(++seen))
            targ = *ri;
    }

    if (!targ.origin())
        mons->move_to_pos(targ);
}

static monster* _place_lost_monster(follower &f)
{
    dprf("Placing lost one: %s", f.mons.name(DESC_PLAIN, true).c_str());

    // Duel targets that survive a duel (due to player excommunication) should
    // be placed next to the player when they exit. Duel targets *entering* a
    // duel will be moved again later, but near_player prevents a tiny chance
    // of them failing to place at all.
    bool near_player = f.mons.props.exists(OKAWARU_DUEL_ABANDONED_KEY)
                       || f.mons.props.exists(OKAWARU_DUEL_CURRENT_KEY);

    if (monster* mons = f.place(near_player))
    {
        // Try to place current duel targets 3-5 spaces away from the player
        if (f.mons.props.exists(OKAWARU_DUEL_CURRENT_KEY))
            _place_oka_duel_target(mons);

        // Figure out how many turns we need to update the monster
        int turns = (you.elapsed_time - f.transit_start_time)/10;

        //Unflag as summoned or else monster will be ignored in update_monster
        mons->flags &= ~MF_JUST_SUMMONED;
        // Don't keep chasing forever.
        mons->props.erase(OKAWARU_DUEL_ABANDONED_KEY);
        // The status should already have been removed from the player, but
        // this prevents an erroneous status indicator sticking on the monster
        mons->del_ench(ENCH_BULLSEYE_TARGET);
        return update_monster(*mons, turns);
    }
    else
        return nullptr;
}

static void _level_place_lost_monsters(m_transit_list &m)
{
    for (auto i = m.begin(); i != m.end(); )
    {
        auto mon = i++;

        // Monsters transiting to the Abyss have a 50% chance of being
        // placed, otherwise a 100% chance.
        // Always place monsters that are chasing the player after abandoning
        // a duel, even in the Abyss.
        if (player_in_branch(BRANCH_ABYSS)
            && !mon->mons.props.exists(OKAWARU_DUEL_ABANDONED_KEY)
            // The Abyss can try to place monsters as 'lost' before it places
            // followers normally, and this can result in companion list desyncs.
            // Try to prevent that.
            && ((mon->mons.flags & MF_TAKING_STAIRS)
                || coinflip()))
        {
            continue;
        }

        if (monster* new_mon =_place_lost_monster(*mon))
        {
            // Now that the monster is on the level, we can safely apply traps
            // to it.
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
            items[i] = env.item[ mons.inv[i] ];
        else
            items[i].clear();
}

monster* follower::place(bool near_player)
{
    ASSERT(mons.alive());

    monster *m = get_free_monster();
    if (!m)
        return nullptr;

    // Copy the saved data.
    *m = mons;

    // Shafts no longer retain the position, if anything else would
    // want to request a specific one, it should do so here if !near_player

    if (m->find_place_to_live(near_player))
    {
#if TAG_MAJOR_VERSION == 34
        // fix up some potential cloned monsters for beogh chars.
        // see comments on maybe_bad_priest monster and in tags.cc for details
        monster *dup_m = monster_by_mid(m->mid);
        if (dup_m && maybe_bad_priest_monster(*dup_m))
            fixup_bad_priest_monster(*dup_m);
        // any clones not covered under maybe_bad_priest_monster will result
        // in duplicate mid errors.
#endif
        dprf("Placed follower: %s", m->name(DESC_PLAIN, true).c_str());
        m->target.reset();

        m->flags &= ~MF_TAKING_STAIRS & ~MF_BANISHED;
        m->flags |= MF_JUST_SUMMONED;
        restore_mons_items(*m);
        env.mid_cache[m->mid] = m->mindex();
        return m;
    }

    m->reset();
    return nullptr;
}

monster* follower::peek()
{
    ASSERT(mons.alive());

    monster *m = get_free_monster();
    if (!m)
        return nullptr;

    // Copy the saved data.
    *m = mons;
    restore_mons_items(*m);

    return m;
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

            item_def &it = env.item[islot];
            it = items[i];
            it.set_holding_monster(m);
        }
    }
}

void follower::write_to_prop(CrawlVector& vec)
{
    vec.clear();
    vec.push_back(mons);
    for (int i = 0; i < NUM_MONSTER_SLOTS; ++i)
        vec.push_back(items[i]);
    vec.push_back(transit_start_time);
}

void follower::read_from_prop(CrawlVector& vec)
{
    mons = vec[0].get_monster();
    for (int i = 0; i < NUM_MONSTER_SLOTS; ++i)
        items[i] = vec[i + 1].get_item();
    transit_start_time = vec[NUM_MONSTER_SLOTS + 1].get_int();
}

static bool _mons_can_follow_player_from(const monster &mons,
                                         const coord_def from,
                                         bool within_level = false)
{
    if (!mons.alive()
        || mons.speed_increment < 50
        || mons.incapacitated()
        || mons.is_stationary()
        || mons.is_constricted()
        || mons.has_ench(ENCH_BOUND))
    {
        return false;
    }

    if (!monster_habitable_grid(&mons, DNGN_FLOOR))
        return false;

    // Only non-wandering friendly monsters or those actively
    // seeking the player will follow up/down stairs.
    if (!mons.friendly()
          && (!mons_is_seeking(mons) || mons.foe != MHITYOU)
        || mons.foe == MHITNOT && mons.behaviour != BEH_WITHDRAW)
    {
        return false;
    }

    // Unfriendly monsters must be directly adjacent to follow.
    if (!mons.friendly() && (mons.pos() - from).rdist() > 1)
        return false;

    // Finally, check whether it is possible for the monster to actually use
    // the same method of transit the player is.
    if (within_level && !mons_class_can_use_transporter(mons.type)
        || !within_level && !mons_can_use_stairs(mons, env.grid(from)))
    {
        return false;
    }
    return true;
}

// Tag any monster following the player
static bool _tag_follower_at(const coord_def &pos, const coord_def &from)
{
    if (!in_bounds(pos) || pos == from)
        return false;

    monster* fol = monster_at(pos);
    if (fol == nullptr)
        return false;

    if (!_mons_can_follow_player_from(*fol, from))
        return false;

    fol->flags |= MF_TAKING_STAIRS;

    // Clear patrolling/travel markers.
    fol->patrol_point.reset();
    fol->travel_path.clear();
    fol->travel_target = MTRAV_NONE;
    mons_end_withdraw_order(*fol);

    dprf("%s is marked for following.", fol->name(DESC_THE, true).c_str());
    return true;
}

/**
 * Handle movement of monsters following the player across stairs or transporters
 *
 * @param from       The location from which the player moved.
 * @param handler    A handler function that does movement of the actor to the
 *                   destination, returning true if the actor was (or will be)
 *                   moved.
 **/
void handle_followers(const coord_def &from,
                      bool (*handler)(const coord_def &pos,
                                      const coord_def &from))
{
    int n_followers = 18;
    for (radius_iterator ri(from, 3, C_SQUARE, LOS_NO_TRANS, true); ri; ++ri)
    {
        if (handler(*ri, from))
        {
            // If we've run out of our follower allowance, bail.
            if (--n_followers <= 0)
                return;
        }
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

static bool _transport_follower_at(const coord_def &pos, const coord_def &from)
{
    if (!in_bounds(pos) || pos == from)
        return false;

    monster* fol = monster_at(pos);
    if (fol == nullptr)
        return false;

    if (!_mons_can_follow_player_from(*fol, from, true))
        return false;

    // Specifically leave friendly monsters behind if there is nowhere to place
    // then on the other side of a transporter (instead of teleporting them to
    // random other places on the floor). Doing so can interact poorly with
    // Guantlet and Beogh in particular.
    if (fol->find_place_to_live(true, fol->friendly()))
    {
        env.map_knowledge(pos).clear_monster();
        dprf("%s is transported.", fol->name(DESC_THE, true).c_str());

        return true;
    }

    return false;
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
