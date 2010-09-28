/*
 * File:     behold.cc
 * Summary:  player methods dealing with mesmerisation.
 */

#include "AppHdr.h"

#include "player.h"

#include "coord.h"
#include "debug.h"
#include "env.h"
#include "fprop.h"
#include "mon-util.h"
#include "monster.h"
#include "random.h"
#include "state.h"
#include "stuff.h"
#include "areas.h"

// Add a monster to the list of beholders.
void player::add_beholder(const monster* mon)
{
    if (is_sanctuary(you.pos()))
    {
        if (you.can_see(mon))
        {
            mprf("%s's singing sounds muted, and has no effect on you.",
                 mon->name(DESC_CAP_THE).c_str());
        }
        else
        {
            mpr("The melody is strangely muted, and has no effect on you.");
        }
        return;
    }

    if (!duration[DUR_MESMERISED])
    {
        you.set_duration(DUR_MESMERISED, 7, 12);
        beholders.push_back(mon->mindex());
        mprf(MSGCH_WARN, "You are mesmerised by %s!",
                         mon->name(DESC_NOCAP_THE).c_str());
    }
    else
    {
        you.increase_duration(DUR_MESMERISED, 5, 12);
        if (!beheld_by(mon))
            beholders.push_back(mon->mindex());
    }
}

// Whether player is mesmerised.
bool player::beheld() const
{
    ASSERT(duration[DUR_MESMERISED] > 0 == !beholders.empty());
    return (duration[DUR_MESMERISED] > 0);
}

// Whether player is mesmerised by the given monster.
bool player::beheld_by(const monster* mon) const
{
    for (unsigned int i = 0; i < beholders.size(); i++)
        if (beholders[i] == mon->mindex())
            return (true);
    return (false);
}

// Checks whether a beholder keeps you from moving to
// target, and returns one if it exists.
monster* player::get_beholder(const coord_def &target) const
{
    for (unsigned int i = 0; i < beholders.size(); i++)
    {
        monster* mon = &menv[beholders[i]];
        const int olddist = grid_distance(pos(), mon->pos());
        const int newdist = grid_distance(target, mon->pos());

        if (olddist < newdist)
            return (mon);
    }
    return (NULL);
}

monster* player::get_any_beholder() const
{
    if (beholders.size() > 0)
        return (&menv[beholders[0]]);
    else
        return (NULL);
}

// Removes a monster from the list of beholders if present.
void player::remove_beholder(const monster* mon)
{
    for (unsigned int i = 0; i < beholders.size(); i++)
        if (beholders[i] == mon->mindex())
        {
            beholders.erase(beholders.begin() + i);
            _removed_beholder();
            return;
        }
}

// Clear the list of beholders. Doesn't message.
void player::clear_beholders()
{
    beholders.clear();
    duration[DUR_MESMERISED] = 0;
}

// Possibly end mesmerisation if a loud noise happened.
void player::beholders_check_noise(int loudness)
{
    if (loudness >= 20 && beheld())
    {
        mprf("For a moment, you cannot hear the mermaid%s!",
             beholders.size() > 1 ? "s" : "");
        clear_beholders();
        _removed_beholder();
    }
}

static void _removed_beholder_msg(const monster* mon)
{
    if (!mon->alive() || mons_genus(mon->type) != MONS_MERMAID
        || mon->submerged() || !you.see_cell(mon->pos()))
    {
        return;
    }

    if (is_sanctuary(you.pos()) && !mons_is_fleeing(mon))
    {
        if (you.can_see(mon))
        {
            mprf("%s's singing becomes strangely muted.",
                 mon->name(DESC_CAP_THE).c_str());
        }
        else
            mpr("Something's singing becomes strangely muted.");

        return;
    }

    if (you.can_see(mon))
    {
        if (silenced(you.pos()) || silenced(mon->pos()))
        {
            mprf("You can no longer hear %s's singing!",
                 mon->name(DESC_NOCAP_THE).c_str());
            return;
        }
        mprf("%s stops singing.", mon->name(DESC_CAP_THE).c_str());
        return;
    }

    mpr("Something stops singing.");
}

// Update all beholders' status after changes.
void player::update_beholders()
{
    if (!beheld())
        return;
    bool removed = false;
    for (int i = beholders.size() - 1; i >= 0; i--)
    {
        const monster* mon = &menv[beholders[i]];
        if (!_possible_beholder(mon))
        {
            beholders.erase(beholders.begin() + i);
            removed = true;
            _removed_beholder_msg(mon);
        }
    }
    if (removed)
        _removed_beholder();
}

// Update a single beholder.
void player::update_beholder(const monster* mon)
{
    if (_possible_beholder(mon))
        return;
    for (unsigned int i = 0; i < beholders.size(); i++)
        if (beholders[i] == mon->mindex())
        {
            beholders.erase(beholders.begin() + i);
            _removed_beholder_msg(mon);
            _removed_beholder();
            return;
        }
}

// Helper function that resets the duration and messages if the player
// is no longer mesmerised.
void player::_removed_beholder()
{
    if (beholders.empty())
    {
        duration[DUR_MESMERISED] = 0;
        mpr(coinflip() ? "You break out of your daze!"
                       : "You are no longer entranced.",
            MSGCH_DURATION);
    }
}

// Helper function that checks whether the given monster is a possible
// beholder.
bool player::_possible_beholder(const monster* mon) const
{
    if (crawl_state.game_is_arena())
        return (false);

    return (mon->alive() && mons_genus(mon->type) == MONS_MERMAID
         && !silenced(pos()) && !silenced(mon->pos())
         && see_cell(mon->pos()) && mon->see_cell(pos())
         && !mon->submerged() && !mon->confused()
         && !mon->asleep() && !mon->cannot_move()
         && !mon->wont_attack() && !mon->pacified()
         && !mon->berserk() && !mons_is_fleeing(mon)
         && !is_sanctuary(you.pos()));
}
