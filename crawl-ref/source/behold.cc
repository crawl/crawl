/**
 * @file
 * @brief player methods dealing with mesmerisation.
**/

#include "AppHdr.h"

#include "player.h"

#include "art-enum.h"
#include "coord.h"
#include "env.h"
#include "fprop.h"
#include "mon-util.h"
#include "monster.h"
#include "random.h"
#include "state.h"
#include "areas.h"

static bool _mermaid_beholder(const monster* mons)
{
    return mons_genus(mons->type) == MONS_MERMAID;
}

// Add a monster to the list of beholders.
void player::add_beholder(const monster* mon, bool axe)
{
    if (is_sanctuary(pos()) && !axe)
    {
        if (_mermaid_beholder(mon))
        {
            if (can_see(mon))
            {
                mprf("%s's singing sounds muted, and has no effect on you.",
                     mon->name(DESC_THE).c_str());
            }
            else
                mpr("The melody is strangely muted, and has no effect on you.");
        }
        else
        {
            if (can_see(mon))
                mprf("%s's is no longer quite as mesmerising!", mon->name(DESC_THE).c_str());
            else
                mpr("Your mesmeriser suddenly seems less interesting!");
        }

        return;
    }

    if (!duration[DUR_MESMERISED])
    {
        set_duration(DUR_MESMERISED, 7, 12);
        beholders.push_back(mon->mindex());
        if (!axe)
        {
            mprf(MSGCH_WARN, "You are mesmerised by %s!",
                             mon->name(DESC_THE).c_str());
        }
    }
    else
    {
        increase_duration(DUR_MESMERISED, 5, 12);
        if (!beheld_by(mon))
            beholders.push_back(mon->mindex());
    }
}

// Whether player is mesmerised.
bool player::beheld() const
{
    ASSERT(duration[DUR_MESMERISED] > 0 == !beholders.empty());
    return duration[DUR_MESMERISED] > 0;
}

// Whether player is mesmerised by the given monster.
bool player::beheld_by(const monster* mon) const
{
    for (unsigned int i = 0; i < beholders.size(); i++)
        if (beholders[i] == mon->mindex())
            return true;
    return false;
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
            return mon;
    }
    return NULL;
}

monster* player::get_any_beholder() const
{
    if (!beholders.empty())
        return &menv[beholders[0]];
    else
        return NULL;
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
void player::beholders_check_noise(int loudness, bool axe)
{
    if (axe)
        return;

    if (loudness >= 20 && beheld())
    {
        mprf("For a moment, your mind becomes perfectly clear!");
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
        if (_mermaid_beholder(mon))
        {
            if (you.can_see(mon))
            {
                mprf("%s's singing becomes strangely muted.",
                     mon->name(DESC_THE).c_str());
            }
            else
                mpr("Something's singing becomes strangely muted.");
        }
        else
        {
            if (you.can_see(mon))
                mprf("%s's is no longer quite as mesmerising!", mon->name(DESC_THE).c_str());
            else
                mpr("Your mesmeriser suddenly seems less interesting!");
        }

        return;
    }

    if (you.can_see(mon))
    {
        if (silenced(you.pos()) || silenced(mon->pos()))
        {
            if (_mermaid_beholder(mon))
            {
                mprf("You can no longer hear %s's singing!",
                     mon->name(DESC_THE).c_str());
            }
            else
                mpr("The silence clears your mind.");
            return;
        }

        if (_mermaid_beholder(mon))
            mprf("%s stops singing.", mon->name(DESC_THE).c_str());
        else
            mprf("%s is no longer quite as mesmerising!", mon->name(DESC_THE).c_str());

        return;
    }

    if (_mermaid_beholder(mon))
        mpr("Something stops singing.");
    else
        mpr("Your mesmeriser is now quite boring!");
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
        if (!possible_beholder(mon))
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
    if (possible_beholder(mon))
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
bool player::possible_beholder(const monster* mon) const
{
    if (crawl_state.game_is_arena())
        return false;

    return (mon->alive() && !mon->submerged()
         && see_cell_no_trans(mon->pos()) && mon->see_cell_no_trans(pos())
         && !mon->wont_attack() && !mon->pacified()
         && ((mons_genus(mon->type) == MONS_MERMAID
              || mon->has_spell(SPELL_MESMERISE))
             && !silenced(pos()) && !silenced(mon->pos())
             && !mon->has_ench(ENCH_MUTE)
             && !mon->confused()
             && !mon->asleep() && !mon->cannot_move()
             && !mon->berserk_or_insane()
             && !mons_is_fleeing(mon)
             && !is_sanctuary(pos())
           || player_equip_unrand(UNRAND_DEMON_AXE)));
}
