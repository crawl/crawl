/*
 * File:      exclude.cc
 * Summary:   Code related to travel exclusions.
 */

#include "AppHdr.h"

#include "exclude.h"

#include "mon-util.h"
#include "stuff.h"
#include "travel.h"
#include "tutorial.h"

// TODO: move other exclusion code here.

static bool _mon_needs_auto_exclude(const monsters *mon, bool sleepy = false)
{
    if (mons_is_stationary(mon))
    {
        if (sleepy)
            return (false);

        // Don't give away mimics unless already known.
        return (!mons_is_mimic(mon->type)
                || testbits(mon->flags, MF_KNOWN_MIMIC));
    }
    // Auto exclusion only makes sense if the monster is still asleep.
    return (mons_is_sleeping(mon));
}

// Check whether a given monster is listed in the auto_exclude option.
bool need_auto_exclude(const monsters *mon, bool sleepy)
{
    // This only works if the name is lowercased.
    std::string name = mon->name(DESC_BASENAME);
    lowercase(name);

    for (unsigned i = 0; i < Options.auto_exclude.size(); ++i)
        if (Options.auto_exclude[i].matches(name)
            && _mon_needs_auto_exclude(mon, sleepy)
            && mon->attitude == ATT_HOSTILE)
        {
            return (true);
        }

    return (false);
}

// If the monster is in the auto_exclude list, automatically set an
// exclusion.
void set_auto_exclude(const monsters *mon)
{
    if (need_auto_exclude(mon) && !is_exclude_root(mon->pos()))
    {
        set_exclude(mon->pos(), LOS_RADIUS, true);
#ifdef USE_TILE
        viewwindow(true, false);
#endif
        learned_something_new(TUT_AUTO_EXCLUSION, mon->pos());
    }
}
        
// Clear auto exclusion if the monster is killed or wakes up with the
// player in sight. If sleepy is true, stationary monsters are ignored.
void remove_auto_exclude(const monsters *mon, bool sleepy)
{
    if (need_auto_exclude(mon, sleepy))
    {
        del_exclude(mon->pos());
#ifdef USE_TILE
        viewwindow(true, false);
#endif
    }
}


