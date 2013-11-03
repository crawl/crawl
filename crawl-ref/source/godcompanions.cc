/**
 * @file
 * @brief Tracking permallies granted by Yred and Beogh.
**/

#include "AppHdr.h"

#include <algorithm>

#include "godcompanions.h"

#include "actor.h"
#include "mon-stuff.h"
#include "mon-util.h"
#include "religion.h"
#include "stuff.h"
#include "spl-other.h"
#include "branch.h"

map<mid_t, companion> companion_list;

companion::companion(const monster& m)
{
    mons = follower(m);
    level = level_id::current();
    timestamp = you.elapsed_time;
}

void init_companions(void)
{
    companion_list.clear();
}

void add_companion(monster* mons)
{
    ASSERT(mons->alive());
    companion_list[mons->mid] = companion(*mons);
}

void remove_companion(monster* mons)
{
    companion_list.erase(mons->mid);
}

void remove_enslaved_soul_companion()
{
    for (map<mid_t, companion>::iterator i = companion_list.begin();
         i != companion_list.end(); ++i)
    {
        monster* mons = monster_by_mid(i->first);
        if (!mons)
            mons = &i->second.mons.mons;
        if (mons_enslaved_soul(mons))
        {
            remove_companion(mons);
            return;
        }
    }
}

void remove_all_companions(god_type god)
{
    for (map<mid_t, companion>::iterator i = companion_list.begin();
         i != companion_list.end();)
    {
        monster* mons = monster_by_mid(i->first);
        if (!mons)
            mons = &i->second.mons.mons;
        if (mons_is_god_gift(mons, god))
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
    for (map<mid_t, companion>::iterator i = companion_list.begin();
         i != companion_list.end(); ++i)
    {
        monster* mons = monster_by_mid(i->first);
        if (mons)
        {
            if (mons->is_divine_companion())
            {
                ASSERT(mons->alive());
                i->second.mons = follower(*mons);
                i->second.timestamp = you.elapsed_time;
            }
        }
    }
}

void populate_offlevel_recall_list(vector<pair<mid_t, int> > &recall_list)
{
    for (map<mid_t, companion>::iterator i = companion_list.begin();
         i != companion_list.end(); ++i)
    {
        int mid = i->first;
        companion* comp = &i->second;
        if (companion_is_elsewhere(mid, true))
        {
            // Recall can't pull monsters out of the Abyss
            if (comp->level.branch == BRANCH_ABYSS)
                continue;

            pair<mid_t, int> p = make_pair(mid, comp->mons.mons.hit_dice);
            recall_list.push_back(p);
        }
    }
}

bool recall_offlevel_ally(mid_t mid)
{
    if (!companion_is_elsewhere(mid, true))
        return false;

    companion* comp = &companion_list[mid];
    if (comp->mons.place(true))
    {
        monster* mons = monster_by_mid(mid);

        // The monster is now on this level
        remove_monster_from_transit(comp->level, mid);
        comp->level = level_id::current();
        simple_monster_message(mons, " is recalled.");

        // Catch up time for off-level monsters
        // (We move the player away so that we don't get expiry
        // messages for things that supposed wore off ages ago)
        const coord_def old_pos = you.pos();
        you.moveto(coord_def(0, 0));

        int turns = you.elapsed_time - comp->timestamp;
        // Note: these are auts, not turns, thus healing is 10 times as fast as
        // for other monsters, confusion goes away after a single turn, etc.

        mons->heal(div_rand_round(turns * mons_off_level_regen_rate(mons), 100));

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

    return false;
}

bool companion_is_elsewhere(mid_t mid, bool must_exist)
{
    if (companion_list.find(mid) != companion_list.end())
    {
        return (companion_list[mid].level != level_id::current()
                || (player_in_branch(BRANCH_PANDEMONIUM)
                    && companion_list[mid].level.branch == BRANCH_PANDEMONIUM
                    && !monster_by_mid(mid)));
    }

    return (!must_exist);
}

void wizard_list_companions()
{
    if (companion_list.size() == 0)
    {
        mpr("You have no companions.");
        return;
    }

    for (map<mid_t, companion>::iterator i = companion_list.begin();
        i != companion_list.end(); ++i)
    {
        companion* comp = &i->second;
        monster* mon = &comp->mons.mons;
        mprf("%s (%d)(%s:%d)", mon->name(DESC_PLAIN, true).c_str(),
                mon->mid, branches[comp->level.branch].abbrevname, comp->level.depth);
    }
}

#if TAG_MAJOR_VERSION == 34
// A temporary routine to clean up some references to invalid companions and
// prevent crashes on load. Should be unnecessary once the cloning bugs that
// allow the creation of these invalid companions are fully mopped up
void fixup_bad_companions()
{
    for (map<mid_t, companion>::iterator i = companion_list.begin();
         i != companion_list.end();)
    {
        if (invalid_monster_type(i->second.mons.mons.type))
            companion_list.erase(i++);
        else
            ++i;
    }
}
#endif
