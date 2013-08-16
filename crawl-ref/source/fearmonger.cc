/**
 * @file
 * @brief player methods dealing with mesmerisation.
**/

#include "AppHdr.h"

#include "player.h"

#include "coord.h"
#include "env.h"
#include "fprop.h"
#include "mon-util.h"
#include "monster.h"
#include "state.h"
#include "areas.h"

// Add a monster to the list of fearmongers.
bool player::add_fearmonger(const monster* mon)
{
    if (is_sanctuary(pos()))
    {
        if (can_see(mon))
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
        fearmongers.push_back(mon->mindex());
        mprf(MSGCH_WARN, "You are terrified of %s!",
                         mon->name(DESC_THE).c_str());
    }
    else
    {
        increase_duration(DUR_AFRAID, 5, 12);
        if (!afraid_of(mon))
            fearmongers.push_back(mon->mindex());
    }

    return true;
}

// Whether player is afraid.
bool player::afraid() const
{
    ASSERT(duration[DUR_AFRAID] > 0 == !fearmongers.empty());
    return (duration[DUR_AFRAID] > 0);
}

// Whether player is afraid of the given monster.
bool player::afraid_of(const monster* mon) const
{
    for (unsigned int i = 0; i < fearmongers.size(); i++)
        if (fearmongers[i] == mon->mindex())
            return true;
    return false;
}

// Checks whether a fearmonger keeps you from moving to
// target, and returns one if it exists.
monster* player::get_fearmonger(const coord_def &target) const
{
    for (unsigned int i = 0; i < fearmongers.size(); i++)
    {
        monster* mon = &menv[fearmongers[i]];
        const int olddist = grid_distance(pos(), mon->pos());
        const int newdist = grid_distance(target, mon->pos());

        if (olddist > newdist)
            return mon;
    }
    return NULL;
}

monster* player::get_any_fearmonger() const
{
    if (!fearmongers.empty())
        return &menv[fearmongers[0]];
    else
        return NULL;
}

// Removes a monster from the list of fearmongers if present.
void player::remove_fearmonger(const monster* mon)
{
    for (unsigned int i = 0; i < fearmongers.size(); i++)
        if (fearmongers[i] == mon->mindex())
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

// Possibly end fear if a loud noise happened.
void player::fearmongers_check_noise(int loudness, bool axe)
{
    if (axe)
        return;

    if (loudness >= 20 && beheld())
    {
        mprf("For a moment, your terror fades away!");
        clear_fearmongers();
        _removed_fearmonger();
    }
}

static void _removed_fearmonger_msg(const monster* mon)
{
    return;
}

// Update all fearmongers' status after changes.
void player::update_fearmongers()
{
    if (!afraid())
        return;
    bool removed = false;
    for (int i = fearmongers.size() - 1; i >= 0; i--)
    {
        const monster* mon = &menv[fearmongers[i]];
        if (!_possible_fearmonger(mon))
        {
            fearmongers.erase(fearmongers.begin() + i);
            removed = true;
            _removed_fearmonger_msg(mon);
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
        if (fearmongers[i] == mon->mindex())
        {
            fearmongers.erase(fearmongers.begin() + i);
            _removed_fearmonger_msg(mon);
            _removed_fearmonger();
            return;
        }
}

// Helper function that resets the duration and messages if the player
// is no longer afraid.
void player::_removed_fearmonger()
{
    if (fearmongers.empty())
    {
        duration[DUR_AFRAID] = 0;
        mpr("You are no longer terrified.",
            MSGCH_DURATION);
    }
}

// Helper function that checks whether the given monster is a possible
// fearmonger.
bool player::_possible_fearmonger(const monster* mon) const
{
    if (crawl_state.game_is_arena())
        return false;

    return (mon->alive()
         && !silenced(pos()) && !silenced(mon->pos())
         && see_cell(mon->pos()) && mon->see_cell(pos())
         && !mon->submerged() && !mon->confused()
         && !mon->asleep() && !mon->cannot_move()
         && !mon->wont_attack() && !mon->pacified()
         && !mon->berserk_or_insane()
         && !mons_is_fleeing(mon)
         && !is_sanctuary(pos()));
}
