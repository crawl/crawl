/**
 * @file
 * @brief player methods dealing with mesmerisation.
**/

#include "AppHdr.h"

#include "mpr.h"
#include "player.h"

#include "areas.h"
#include "art-enum.h"
#include "coord.h"
#include "env.h"
#include "fprop.h"
#include "losglobal.h"
#include "religion.h"
#include "state.h"
#include "stringutil.h"

// Add a monster to the list of beholders.
void player::add_beholder(monster& mon, bool forced, int dur)
{
    if (!duration[DUR_MESMERISED])
    {
        if (!dur)
            dur = random_range(7, 15);
        set_duration(DUR_MESMERISED, dur, 15);
        beholders.push_back(mon.mid);
        if (!forced)
        {
            mprf(MSGCH_WARN, "You are mesmerised by %s!",
                             mon.name(DESC_THE).c_str());
        }
    }
    else
    {
        if (!dur)
            dur = random_range(5, 8);
        increase_duration(DUR_MESMERISED, dur, 15);
        if (!beheld_by(mon))
            beholders.push_back(mon.mid);
    }

    // If forced externally onto a creature, make sure it doesn't break if they
    // become disabled.
    if (forced)
        mon.props[FORCED_MESMERISE_KEY] = true;
}

// Whether player is mesmerised.
bool player::beheld() const
{
    ASSERT((duration[DUR_MESMERISED] > 0) == !beholders.empty());
    return duration[DUR_MESMERISED] > 0;
}

// Whether player is mesmerised by the given monster.
bool player::beheld_by(const monster &mon) const
{
    return find(begin(beholders), end(beholders), mon.mid) != end(beholders);
}

// Checks whether a beholder keeps you from moving to
// target, and returns one if it exists.
monster* player::get_beholder(const coord_def &target) const
{
    for (mid_t beh : beholders)
    {
        monster* mon = monster_by_mid(beh);
        // The monster may have died.
        if (!mon)
            continue;
        const int olddist = grid_distance(pos(), mon->pos());
        const int newdist = grid_distance(target, mon->pos());

        if (olddist < newdist)
            return mon;
    }
    return nullptr;
}

monster* player::get_any_beholder() const
{
    if (!beholders.empty())
        return monster_by_mid(beholders[0]);
    else
        return nullptr;
}

// Removes a monster from the list of beholders if present.
void player::remove_beholder(monster& mon)
{
    for (unsigned int i = 0; i < beholders.size(); i++)
        if (beholders[i] == mon.mid)
        {
            beholders.erase(beholders.begin() + i);
            mon.props.erase(FORCED_MESMERISE_KEY);
            _removed_beholder();
            return;
        }
}

// Clear the list of beholders. Doesn't message.
void player::clear_beholders()
{
    beholders.clear();
    duration[DUR_MESMERISED] = 0;
    you.duration[DUR_MESMERISE_IMMUNE] = random_range(21, 40);
}

static void _removed_beholder_msg(const monster *mons)
{
    if (!mons)
        return;

    const monster &mon = *mons;
    if (!mon.alive() || !you.see_cell(mon.pos()))
        return;

    if (is_sanctuary(you.pos()) && !mons_is_fleeing(mon))
    {
        string msg = make_stringf("The Sanctuary denies %s grip on your mind.",
                                  mons->name(DESC_ITS).c_str());
        god_speaks(GOD_ZIN, msg.c_str());
        return;
    }

    if (mons_is_siren_beholder(mon))
    {
        if (silenced(you.pos()) || silenced(mon.pos()))
        {
            if (you.can_see(mon))
                mprf("You can no longer hear %s singing!", mon.name(DESC_ITS).c_str());
            else
                mpr("The silence clears your mind.");
        }
        else
            mprf("%s stops singing.", mon.name(DESC_THE).c_str());
    }
    else
    {
        mprf("%s loses %s grip on your mind.", mon.name(DESC_THE).c_str(),
                                               mon.pronoun(PRONOUN_POSSESSIVE).c_str());
    }
}

// Update all beholders' status after changes.
void player::update_beholders()
{
    if (!beheld())
        return;
    bool removed = false;
    for (int i = beholders.size() - 1; i >= 0; i--)
    {
        monster* mon = monster_by_mid(beholders[i]);
        if (!possible_beholder(mon))
        {
            beholders.erase(beholders.begin() + i);
            removed = true;
            mon->props.erase(FORCED_MESMERISE_KEY);

            // If that was the last one, clear the duration before
            // printing any subsequent messages, or a --more-- can
            // crash (#6547).
            _removed_beholder(true);
            _removed_beholder_msg(mon);
        }
    }
    if (removed)
        _removed_beholder();
}

// Update a single beholder.
void player::update_beholder(monster* mon)
{
    if (possible_beholder(mon))
        return;
    for (unsigned int i = 0; i < beholders.size(); i++)
        if (beholders[i] == mon->mid)
        {
            beholders.erase(beholders.begin() + i);
            mon->props.erase(FORCED_MESMERISE_KEY);
            // Do this dance to clear the duration before printing messages
            // (#8844), but still print all messages in the right order.
            _removed_beholder(true);
            _removed_beholder_msg(mon);
            _removed_beholder();
            return;
        }
}

// Helper function that resets the duration and messages if the player
// is no longer mesmerised.
void player::_removed_beholder(bool quiet)
{
    if (beholders.empty())
    {
        duration[DUR_MESMERISED] = 0;
        you.duration[DUR_MESMERISE_IMMUNE] = random_range(21, 40);
        if (!quiet)
            mprf(MSGCH_DURATION, "You are no longer mesmerised.");
    }
}

// Helper function that checks whether the given monster is a possible
// beholder.
bool player::possible_beholder(const monster* mon) const
{
    if (crawl_state.game_is_arena())
        return false;

    return mon && mon->alive() && cell_see_cell(pos(), mon->pos(), LOS_SOLID_SEE)
        && !mon->wont_attack() && !mon->pacified() && !is_sanctuary(pos())
        && (((!mons_is_siren_beholder(mon->type)
                || (!silenced(pos()) && !mon->is_silenced()))
             && !mon->confused()
             && !mon->asleep() && !mon->cannot_act()
             && !mon->berserk_or_frenzied()
             && !mons_is_fleeing(*mon))
            || mon->props.exists(FORCED_MESMERISE_KEY));
}
