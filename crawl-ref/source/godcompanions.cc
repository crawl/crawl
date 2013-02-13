/**
 * @file
 * @brief Tracking peramallies granted by Yred and Beogh.
**/

#include "AppHdr.h"

#include <algorithm>

#include "godcompanions.h"

#include "coord.h"
#include "coordit.h"
#include "actor.h"
#include "mon-stuff.h"
#include "mon-util.h"
#include "religion.h"
#include "stuff.h"

map<mid_t, companion> companion_list;

companion::companion(const monster& m)
{
    mons = follower(m);
    level = level_id::current();
    timestamp = you.elapsed_time;
}

void add_companion(monster* mons)
{
    companion_list[mons->mid] = companion(*mons);
}

void remove_companion(monster* mons)
{
    companion_list.erase(mons->mid);
}

void remove_all_companions(god_type god)
{
    for(map<mid_t, companion>::iterator i = companion_list.begin();
        i != companion_list.end(); ++i )
    {
        monster* mons = monster_by_mid(i->first);
        if (!mons)
            mons = &i->second.mons.mons;
        if ((god == GOD_YREDELEMNUL && is_yred_undead_slave(mons))
            || (god == GOD_BEOGH && mons->wont_attack() && mons_is_god_gift(mons, GOD_BEOGH)))
        {
            companion_list.erase(i);
        }
    }
}

void move_companion_to(const monster* mons, const level_id lid)
{
    companion_list[mons->mid].level = lid;
    companion_list[mons->mid].mons = follower(*mons);
    companion_list[mons->mid].timestamp = you.elapsed_time;
}

void update_companions()
{
    for(map<mid_t, companion>::iterator i = companion_list.begin();
        i != companion_list.end(); ++i )
    {
        monster* mons = monster_by_mid(i->first);
        if (mons)
        {
            if (mons->is_divine_companion())
            {
                i->second.mons = follower(*mons);
                i->second.timestamp = you.elapsed_time;
            }
            else // We must have angered this creature or lost our religion
            {
                companion_list.erase(i);
            }
        }
    }
}

bool recall_offlevel_companions()
{
    bool recalled = false;

    for(map<mid_t, companion>::iterator i = companion_list.begin();
        i != companion_list.end(); ++i )
    {
        int mid = i->first;
        companion* comp = &i->second;
        if (comp->level != level_id::current())
        {
            if (comp->mons.place(true))
            {
                // The monster is now on this level
                remove_monster_from_transit(comp->level, mid);
                comp->level = level_id::current();
                simple_monster_message(monster_by_mid(mid), " is recalled.");
                recalled = true;
            }
        }
    }

    return recalled;
}

bool companion_is_elsewhere(const monster* mons)
{
    if (companion_list.find(mons->mid) != companion_list.end())
    {
        return (companion_list[mons->mid].level != level_id::current());
    }
    return false;
}