/**
 * @file
 * @brief User interactions that need to be restarted if the game is forcibly
 *        saved (via SIGHUP/window close).
**/

#include "AppHdr.h"
#include "acquire.h"
#include "decks.h"
#include "libutil.h"
#include "player.h"
#include "state.h"
#include "uncancel.h"

void add_uncancel(uncancellable_type kind, int arg)
{
    you.uncancel.push_back(pair<uncancellable_type, int>(kind, arg));
}

void run_uncancels()
{
    while (!you.uncancel.empty() && !crawl_state.seen_hups)
    {
        // changed to -1 if we pop prematurely
        int act = you.uncancel.size() - 1;

        // Beware of race conditions: it is not enough to check seen_hups,
        // the action must actually fail as well!  And if it fails but there
        // was no HUP, it's due to some other reason.

        int arg = you.uncancel[act].second;

        switch (you.uncancel[act].first)
        {
        case UNC_ACQUIREMENT:
            if (!acquirement(OBJ_RANDOM, arg) && crawl_state.seen_hups)
                return;
            break;

        case UNC_DRAW_THREE:
            if (!draw_three(arg) && crawl_state.seen_hups)
                return;
            break;

        case UNC_STACK_FIVE:
            if (!stack_five(arg) && crawl_state.seen_hups)
                return;
            break;

        case UNC_MERCENARY:
            if (!recruit_mercenary(arg) && crawl_state.seen_hups)
                return;
            break;

        }

        if (act != -1)
            erase_any(you.uncancel, act);
    }
}
