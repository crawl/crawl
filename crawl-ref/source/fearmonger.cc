/**
 * @file
 * @brief player methods dealing with mesmerisation.
**/

#include "AppHdr.h"

#include "player.h"

#include "areas.h"
#include "coord.h"
#include "env.h"
#include "fprop.h"
#include "monster.h"
#include "mon-util.h"
#include "state.h"

// Add a monster to the list of fearmongers.
bool player::add_fearmonger(const monster* mon)
{
    ASSERT(mon); // XXX: change to const monster &mon
    if (is_sanctuary(pos()))
    {
        if (can_see(*mon))
        {
            mprf("%s's aura of fear is muted, and has no effect on you.",
                 mon->name(DESC_THE).c_str());
        }
        else
            mpr("The fearful aura is strangely muted, and has no effect on you.");

        return false;
    }

    if (!duration[DUR_AFRAID])
    {
        set_duration(DUR_AFRAID, 7, 12);
        fearmongers.push_back(mon->mid);
        mprf(MSGCH_WARN, "You are terrified of %s!",
                         mon->name(DESC_THE).c_str());
    }
    else
    {
        increase_duration(DUR_AFRAID, 5, 12);
        if (!afraid_of(mon))
            fearmongers.push_back(mon->mid);
    }

    return true;
}

// Whether player is afraid.
bool player::afraid() const
{
    ASSERT((duration[DUR_AFRAID] > 0) == !fearmongers.empty());
    return duration[DUR_AFRAID] > 0;
}

// Whether player is afraid of the given monster.
bool player::afraid_of(const monster* mon) const
{
    return find(begin(fearmongers), end(fearmongers), mon->mid)
               != end(fearmongers);
}

// Checks whether a fearmonger keeps you from moving to
// target, and returns one if it exists.
monster* player::get_fearmonger(const coord_def &target) const
{
    for (mid_t monger : fearmongers)
    {
        monster* mon = monster_by_mid(monger);
        // The monster may have died.
        if (!mon)
            continue;
        const int olddist = grid_distance(pos(), mon->pos());
        const int newdist = grid_distance(target, mon->pos());

        if (olddist > newdist)
            return mon;
    }
    return nullptr;
}

monster* player::get_any_fearmonger() const
{
    if (!fearmongers.empty())
        return monster_by_mid(fearmongers[0]);
    else
        return nullptr;
}

// Removes a monster from the list of fearmongers if present.
void player::remove_fearmonger(const monster* mon)
{
    for (unsigned int i = 0; i < fearmongers.size(); i++)
        if (fearmongers[i] == mon->mid)
        {
            fearmongers.erase(fearmongers.begin() + i);
            _removed_fearmonger();
            return;
        }
}

// Clear the list of fearmongers. Doesn't message.
void player::clear_fearmongers()
{
    fearmongers.clear();
    duration[DUR_AFRAID] = 0;
}

// Update all fearmongers' status after changes.
void player::update_fearmongers()
{
    if (!afraid())
        return;
    bool removed = false;
    for (int i = fearmongers.size() - 1; i >= 0; i--)
    {
        const monster* mon = monster_by_mid(fearmongers[i]);
        if (!_possible_fearmonger(mon))
        {
            fearmongers.erase(fearmongers.begin() + i);
            removed = true;

            // If that was the last one, clear the duration before
            // printing any subsequent messages, or a --more-- can
            // crash (#6547).
            _removed_fearmonger(true);
        }
    }
    if (removed)
        _removed_fearmonger();
}

// Update a single fearmonger.
void player::update_fearmonger(const monster* mon)
{
    if (_possible_fearmonger(mon))
        return;
    for (unsigned int i = 0; i < fearmongers.size(); i++)
        if (fearmongers[i] == mon->mid)
        {
            fearmongers.erase(fearmongers.begin() + i);
            // Do this dance to clear the duration before printing messages
            // (#8844), but still print all messages in the right order.
            _removed_fearmonger(true);
            _removed_fearmonger();
            return;
        }
}

// Helper function that resets the duration and messages if the player
// is no longer afraid.
void player::_removed_fearmonger(bool quiet)
{
    if (fearmongers.empty())
    {
        duration[DUR_AFRAID] = 0;
        if (!quiet)
            mprf(MSGCH_DURATION, "You are no longer terrified.");
    }
}

// Helper function that checks whether the given monster is a possible
// fearmonger.
bool player::_possible_fearmonger(const monster* mon) const
{
    if (crawl_state.game_is_arena())
        return false;

    return mon && mon->alive()
        && !silenced(pos()) && !silenced(mon->pos())
        && see_cell(mon->pos()) && mon->see_cell(pos())
        && !mon->submerged() && !mon->confused()
        && !mon->asleep() && !mon->cannot_move()
        && !mon->wont_attack() && !mon->pacified()
        && !mon->berserk_or_insane()
        && !mons_is_fleeing(*mon)
        && !is_sanctuary(pos());
}
