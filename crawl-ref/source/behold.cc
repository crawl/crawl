#include "AppHdr.h"

#include "player.h"

#include "coord.h"
#include "debug.h"
#include "env.h"
#include "los.h"
#include "mon-util.h"
#include "monster.h"
#include "random.h"
#include "stuff.h"
#include "view.h"

// Add a monster to the list of beholders.
void player::add_beholder(const monsters* mon)
{
    if (!duration[DUR_MESMERISED])
    {
        duration[DUR_MESMERISED] = 7;
        beholders.push_back(mon->mindex());
        mprf(MSGCH_WARN, "You are mesmerised by %s!",
                         mon->name(DESC_NOCAP_THE).c_str());
    }
    else
    {
        duration[DUR_MESMERISED] += 5;
        if (!beheld_by(mon))
            beholders.push_back(mon->mindex());
    }

    if (duration[DUR_MESMERISED] > 12)
        duration[DUR_MESMERISED] = 12;
}

// Whether player is mesmerised.
bool player::beheld() const
{
    ASSERT(duration[DUR_MESMERISED] > 0 == !beholders.empty());
    return (duration[DUR_MESMERISED] > 0);
}

// Whether player is mesmerised by the given monster.
bool player::beheld_by(const monsters* mon) const
{
    for (unsigned int i = 0; i < beholders.size(); i++)
        if (beholders[i] == mon->mindex())
            return (true);
    return (false);
}

// Checks whether a beholder keeps you from moving to
// target, and returns one if it exists.
monsters* player::get_beholder(const coord_def &target) const
{
    for (unsigned int i = 0; i < beholders.size(); i++)
    {
        monsters *mon = &menv[beholders[i]];
        const int olddist = grid_distance(you.pos(), mon->pos());
        const int newdist = grid_distance(target, mon->pos());

        if (olddist < newdist)
            return (mon);
    }
    return (NULL);
}

monsters* player::get_any_beholder() const
{
    if (beholders.size() > 0)
        return (&menv[beholders[0]]);
    else
        return (NULL);
}

// Removes a monster from the list of beholders if present.
void player::remove_beholder(const monsters *mon)
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
    if (loudness >= 20 && you.beheld())
    {
        mprf("For a moment, you cannot hear the mermaid%s!",
             beholders.size() > 1 ? "s" : "");
        clear_beholders();
        _removed_beholder();
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
        const monsters* mon = &menv[beholders[i]];
        if (!_possible_beholder(mon))
        {
            beholders.erase(beholders.begin() + i);
            removed = true;
        }
    }
    if (removed)
        _removed_beholder();
}

// Update a single beholder.
void player::update_beholder(const monsters *mon)
{
    if (_possible_beholder(mon))
        return;
    for (unsigned int i = 0; i < beholders.size(); i++)
        if (beholders[i] = mon->mindex())
        {
            beholders.erase(beholders.begin() + i);
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
bool player::_possible_beholder(const monsters *mon) const
{
    if (silenced(you.pos()))
        return (false);
    if (!mon->alive() || !mons_near(mon) || mons_friendly(mon)
        || mon->submerged() || mon->confused() || mons_cannot_move(mon)
        || mon->asleep() || silenced(mon->pos()))
    {
        return (false);
    }

    // TODO: replace this by see/see_no_trans.
    int walls = num_feats_between(you.pos(), mon->pos(),
                                  DNGN_UNSEEN, DNGN_MAXOPAQUE);
    if (walls > 0)
        return (false);

    return (true);
}
