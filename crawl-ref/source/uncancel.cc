/**
 * @file
 * @brief User interactions that need to be restarted if the game is forcibly
 *        saved (via SIGHUP/window close).
**/

#include "AppHdr.h"

#include "mpr.h"
#include "uncancel.h"

#include "ability.h"
#include "acquire.h"
#include "decks.h"
#include "env.h"
#include "god-abil.h"
#include "item-use.h"
#include "libutil.h"
#include "nearby-danger.h"
#include "state.h"
#include "tag-version.h"
#include "unwind.h"

bool has_uncancel()
{
    return !you.uncancel.empty();
}

static bool _resume_uncancel(bool run_success_effect)
{
    if (!has_uncancel())
        return false;

    static bool running = false;
    // Run uncancels iteratively rather than recursively.
    if (running)
        return false;
    unwind_bool run(running, true);

    const size_t index = you.uncancel.size() - 1;

    // Beware of race conditions: it is not enough to check seen_hups as it
    // might have been set after the action succeeded or failed for unrelated
    // reasons.

    uncancellable copy = you.uncancel[index];
    dprf("Running uncancel type=%d", copy.kind);

    const coord_def original_pos = you.pos();
    bool nearby_mons = there_are_monsters_nearby(true, true, false);
    const bool dangerous = player_in_a_dangerous_place();
    ability_type ability = ABIL_NON_ABILITY;

    bool succeeded = true;
    switch (copy.kind)
    {
    case UNC_DRAW_THREE:
        succeeded = draw_three();
        ability = ABIL_NEMELEX_TRIPLE_DRAW;
        break;
    case UNC_STACK_FIVE:
        succeeded = stack_five();
        ability = ABIL_NEMELEX_STACK_FIVE;
        break;
    case UNC_POTION_PETITION:
        succeeded = gozag_potion_petition();
        ability = ABIL_GOZAG_POTION_PETITION;
        break;
    case UNC_CALL_MERCHANT:
        succeeded = gozag_call_merchant();
        ability = ABIL_GOZAG_CALL_MERCHANT;
        break;
    case UNC_ENCHANT_WEAPON:
        succeeded = uncancel_enchant_weapon();
        break;
    case UNC_ENCHANT_ARMOUR:
        succeeded = uncancel_enchant_armour();
        break;
    case UNC_BRAND_WEAPON:
        succeeded = uncancel_brand_weapon();
        break;
    case UNC_AMNESIA:
        succeeded = uncancel_amnesia();
        break;
    case UNC_BLINKING:
        succeeded = uncancel_blinking();
        break;
    case UNC_IDENTIFY:
        succeeded = uncancel_identify();
        break;
    }

    if (!succeeded)
        return false;

    // We used to have a bug where we would sometimes run the success effects
    // before running the uncanel. If this has happened, don't run them after
    // as well
#if TAG_MAJOR_VERSION == 34
    if (copy.piety_cost_or_in_inventory < 0)
        run_success_effect = false;
#endif

    if (run_success_effect)
    {
        if (ability != ABIL_NON_ABILITY)
        {
            int piety_cost = copy.piety_cost_or_in_inventory;
            int mp_cost = copy.mp_cost_or_item_index;
            handle_post_ability_effects(ability, spret::success, piety_cost,
                                        mp_cost, copy.hp_cost, true);
        }
        else
        {
            bool in_inv = (bool)copy.piety_cost_or_in_inventory;
            int scroll_index = copy.mp_cost_or_item_index;
            item_def& scroll = in_inv ? you.inv[scroll_index]
                                      : env.item[scroll_index];
            handle_post_scroll_effects(&scroll, spret::success, original_pos,
                                       nearby_mons, false, dangerous, false);
        }
    }

    you.uncancel.erase(you.uncancel.begin() + index);
    return true;
}

void resume_uncancel()
{
    _resume_uncancel(true);
}

bool run_uncancel(uncancellable uc)
{
    you.uncancel.push_back(uc);
    return _resume_uncancel(false);
}
