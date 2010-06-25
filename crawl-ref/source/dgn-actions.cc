/*
 *  File:       dgn-actions.cc
 *  Summary:    Delayed level actions.
 *  Written by: Adam Borowski
 */

#include "AppHdr.h"

#include "dgn-actions.h"

#include "coordit.h"
#include "debug.h"
#include "decks.h"
#include "env.h"
#include "player.h"
#include "view.h"

static const char *daction_names[] =
{
    "shuffle decks",
    "reapply passive mapping",
    "remove Jiyva altars",
};

void add_daction(daction_type act)
{
    COMPILE_CHECK(ARRAYSZ(daction_names) == NUM_DACTIONS, dactions_names_size);

    dprf("scheduling delayed action: %s", daction_names[act]);
    you.dactions.push_back(act);

    // Immediately apply it to the current level.
    catchup_dactions();
}

void _apply_daction(daction_type act)
{
    ASSERT(act >= 0 && act < NUM_DACTIONS);
    dprf("applying delayed action: %s", daction_names[act]);

    switch(act)
    {
    case DACT_SHUFFLE_DECKS:
        shuffle_all_decks_on_level();
        break;
    case DACT_REAUTOMAP:
        reautomap_level();
        break;
    case DACT_REMOVE_JIYVA_ALTARS:
        for (rectangle_iterator ri(1); ri; ++ri)
        {
            if (grd(*ri) == DNGN_ALTAR_JIYVA)
                grd(*ri) = DNGN_FLOOR;
        }
        break;
    case NUM_DACTIONS:
        ;
    }
}

void catchup_dactions()
{
    while(env.dactions_done < you.dactions.size())
        _apply_daction(you.dactions[env.dactions_done++]);
}
