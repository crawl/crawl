/**
 * @file
 * @brief Tracking permallies granted by Yred and Beogh.
**/

#include "AppHdr.h"

#include "god-companions.h"

#include <algorithm>

#include "branch.h"
#include "dgn-overview.h"
#include "message.h"
#include "mon-util.h"
#include "religion.h"
#include "spl-other.h"

map<mid_t, companion> companion_list;

companion::companion(const monster& m)
    : mons(follower(m)), level(level_id::current()), timestamp(you.elapsed_time)
{
}

void init_companions()
{
    companion_list.clear();
}

void add_companion(monster* mons)
{
    ASSERT(mons->alive());
    // Right now this is a special case for Saint Roka, but
    // future orcish uniques should behave in the same way.
    mons->props["no_annotate"] = true;
    remove_unique_annotation(mons);
    companion_list[mons->mid] = companion(*mons);
}

void remove_companion(monster* mons)
{
    mons->props["no_annotate"] = false;
    set_unique_annotation(mons);
    companion_list.erase(mons->mid);
}

void remove_enslaved_soul_companion()
{
    for (auto &entry : companion_list)
    {
        monster* mons = monster_by_mid(entry.first);
        if (!mons)
            mons = &entry.second.mons.mons;
        if (mons_enslaved_soul(*mons))
        {
            remove_companion(mons);
            return;
        }
    }
}

void remove_all_companions(god_type god)
{
    for (auto i = companion_list.begin(); i != companion_list.end();)
    {
        monster* mons = monster_by_mid(i->first);
        if (!mons)
            mons = &i->second.mons.mons;
        if (mons_is_god_gift(*mons, god))
            companion_list.erase(i++);
        else
            ++i;
    }
}

void move_companion_to(const monster* mons, const level_id lid)
{
    // If it's taking stairs, that means the player is heading ahead of it,
    // so we shouldn't relocate the monster until it actually arrives
    // (or we can clone things on the other end)
    if (!(mons->flags & MF_TAKING_STAIRS))
    {
        companion_list[mons->mid].level = lid;
        companion_list[mons->mid].mons = follower(*mons);
        companion_list[mons->mid].timestamp = you.elapsed_time;
    }
}

void update_companions()
{
    for (auto &entry : companion_list)
    {
        monster* mons = monster_by_mid(entry.first);
        if (mons)
        {
            if (mons->is_divine_companion())
            {
                ASSERT(mons->alive());
                entry.second.mons = follower(*mons);
                entry.second.timestamp = you.elapsed_time;
            }
        }
    }
}

void populate_offlevel_recall_list(vector<pair<mid_t, int> > &recall_list)
{
    for (auto &entry : companion_list)
    {
        int mid = entry.first;
        companion &comp = entry.second;
        if (companion_is_elsewhere(mid, true))
        {
            // Recall can't pull monsters out of the Abyss
            if (comp.level.branch == BRANCH_ABYSS)
                continue;

            recall_list.emplace_back(mid, comp.mons.mons.get_experience_level());
        }
    }
}

/**
 * Attempt to recall an ally from offlevel.
 *
 * @param mid   The ID of the monster to be recalled.
 * @return      Whether the monster was successfully recalled onto the level.
 * Note that the monster may not still be alive or onlevel, due to shafts, etc,
 * but they were here at least briefly!
 */
bool recall_offlevel_ally(mid_t mid)
{
    if (!companion_is_elsewhere(mid, true))
        return false;

    companion* comp = &companion_list[mid];
    if (!comp->mons.place(true))
        return false;

    monster* mons = monster_by_mid(mid);

    // The monster is now on this level
    remove_monster_from_transit(comp->level, mid);
    comp->level = level_id::current();
    simple_monster_message(*mons, "은(는) 소집되었다.");

    // Now that the monster is onlevel, we can safely apply traps to it.
    // old location isn't very meaningful, so use current loc
    mons->apply_location_effects(mons->pos());
    // check if it was killed/shafted by a trap...
    if (!mons->alive())
        return true; // still successfully recalled!

    // Catch up time for off-level monsters
    // (We move the player away so that we don't get expiry
    // messages for things that supposed wore off ages ago)
    const coord_def old_pos = you.pos();
    you.moveto(coord_def(0, 0));

    int turns = you.elapsed_time - comp->timestamp;
    // Note: these are auts, not turns, thus healing is 10 times as fast as
    // for other monsters, confusion goes away after a single turn, etc.

    mons->heal(div_rand_round(turns * mons->off_level_regen_rate(), 100));

    if (turns >= 10 && mons->alive())
    {
        // Remove confusion manually (so that the monster
        // doesn't blink after being recalled)
        mons->del_ench(ENCH_CONFUSION, true);
        mons->timeout_enchantments(turns / 10);
    }
    you.moveto(old_pos);
    // Do this after returning the player to the proper position
    // because it uses player position
    recall_orders(mons);

    return true;
}

bool companion_is_elsewhere(mid_t mid, bool must_exist)
{
    if (companion_list.count(mid))
    {
        return companion_list[mid].level != level_id::current()
               || (player_in_branch(BRANCH_PANDEMONIUM)
                   && companion_list[mid].level.branch == BRANCH_PANDEMONIUM
                   && !monster_by_mid(mid));
    }

    return !must_exist;
}

void wizard_list_companions()
{
    if (companion_list.size() == 0)
    {
        mpr("You have no companions.");
        return;
    }

    for (auto &entry : companion_list)
    {
        companion &comp = entry.second;
        monster &mon = comp.mons.mons;
        mprf("%s (%d)(%s:%d)", mon.name(DESC_PLAIN, true).c_str(), mon.mid,
             branches[comp.level.branch].abbrevname, comp.level.depth);
    }
}

/**
 * Returns the mid of the current ancestor granted by Hepliaklqana, if any. If none
 * exists, returns MID_NOBODY.
 *
 * The ancestor is *not* guaranteed to be on-level, even if it exists; check
 * the companion_list before doing anything rash!
 *
 * @return  The mid_t of the player's ancestor, or MID_NOBODY if none exists.
 */
mid_t hepliaklqana_ancestor()
{
    for (auto &entry : companion_list)
        if (mons_is_hepliaklqana_ancestor(entry.second.mons.mons.type))
            return entry.first;
    return MID_NOBODY;
}

/**
 * Returns the a pointer to the current ancestor granted by Hepliaklqana, if
 * any. If none exists, returns null.
 *
 * The ancestor is *not* guaranteed to be on-level, even if it exists; check
 * the companion_list before doing anything rash!
 *
 * @return  The player's ancestor, or nullptr if none exists.
 */
monster* hepliaklqana_ancestor_mon()
{
    const mid_t ancestor_mid = hepliaklqana_ancestor();
    if (ancestor_mid == MID_NOBODY)
        return nullptr;

    monster* ancestor = monster_by_mid(ancestor_mid);
    if (ancestor)
        return ancestor;

    for (auto &entry : companion_list)
        if (mons_is_hepliaklqana_ancestor(entry.second.mons.mons.type))
            return &entry.second.mons.mons;
    // should never reach this...
    return nullptr;
}

#if TAG_MAJOR_VERSION == 34
// A temporary routine to clean up some references to invalid companions and
// prevent crashes on load. Should be unnecessary once the cloning bugs that
// allow the creation of these invalid companions are fully mopped up
void fixup_bad_companions()
{
    for (auto i = companion_list.begin(); i != companion_list.end();)
    {
        if (invalid_monster_type(i->second.mons.mons.type))
            companion_list.erase(i++);
        else
            ++i;
    }
}
#endif
