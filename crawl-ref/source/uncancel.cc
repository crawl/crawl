/**
 * @file
 * @brief User interactions that need to be restarted if the game is forcibly
 *        saved (via SIGHUP/window close).
**/

#include "AppHdr.h"

#include "mpr.h"
#include "uncancel.h"

#include "acquire.h"
#include "decks.h"
#include "god-abil.h"
#include "item-use.h"
#include "libutil.h"
#include "state.h"
#include "tag-version.h"
#include "unwind.h"

void add_uncancel(uncancellable_type kind, int arg)
{
    you.uncancel.emplace_back(kind, arg);
}

static bool running = false;

void run_uncancels()
{
    // Run uncancels iteratively rather than recursively.
    if (running)
        return;
    unwind_bool run(running, true);

    while (!you.uncancel.empty() && !crawl_state.seen_hups)
    {
        // changed to -1 if we pop prematurely
        int act = you.uncancel.size() - 1;

        // Beware of race conditions: it is not enough to check seen_hups,
        // the action must actually fail as well!  And if it fails but there
        // was no HUP, it's due to some other reason.

        int arg = you.uncancel[act].second;
        dprf("Running uncancel type=%d arg=%d", you.uncancel[act].first, arg);

        switch (you.uncancel[act].first)
        {
        case UNC_DRAW_THREE:
            if (!draw_three() && crawl_state.seen_hups)
                return;
            break;

        case UNC_STACK_FIVE:
            if (!stack_five(arg) && crawl_state.seen_hups)
                return;
            break;

#if TAG_MAJOR_VERSION == 34
        case UNC_MERCENARY:
        case UNC_ACQUIREMENT:
            break;

#endif
        case UNC_POTION_PETITION:
            if (!gozag_potion_petition() && crawl_state.seen_hups)
                return;
            break;

        case UNC_CALL_MERCHANT:
            if (!gozag_call_merchant() && crawl_state.seen_hups)
                return;
            break;

        case UNC_ENCHANT_WEAPON:
            if (!uncancel_enchant_weapon() && crawl_state.seen_hups)
                return;
            break;
        case UNC_ENCHANT_ARMOUR:
            if (!uncancel_enchant_armour() && crawl_state.seen_hups)
                return;
            break;
        case UNC_BRAND_WEAPON:
            if (!uncancel_brand_weapon() && crawl_state.seen_hups)
                return;
            break;
        case UNC_AMNESIA:
            if (!uncancel_amnesia() && crawl_state.seen_hups)
                return;
            break;
        case UNC_BLINKING:
            if (!uncancel_blinking() && crawl_state.seen_hups)
                return;
            break;
        case UNC_IDENTIFY:
            if (!uncancel_identify() && crawl_state.seen_hups)
                return;
            break;
        }

        if (act != -1)
            erase_any(you.uncancel, act);
    }
}
