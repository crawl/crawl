/**
 * @file
 * @brief Weapon enchantment spells.
**/

#include "AppHdr.h"

#include "spl-wpnench.h"

#include "areas.h"
#include "god-item.h"
#include "god-passive.h"
#include "item-prop.h"
#include "message.h"
#include "player-equip.h"
#include "prompt.h"
#include "religion.h"
#include "shout.h"
#include "spl-miscast.h"
#include "spl-summoning.h"

/** End your weapon branding spell.
 *
 * Returns the weapon to the previous brand, and ends DUR_EXCRUCIATING_WOUNDS.
 * @param weapon The item in question (which may have just been unwielded).
 * @param verbose whether to print a message about expiration.
 */
void end_weapon_brand(item_def &weapon, bool verbose)
{
    ASSERT(you.duration[DUR_EXCRUCIATING_WOUNDS]);

    set_item_ego_type(weapon, OBJ_WEAPONS, you.props[ORIGINAL_BRAND_KEY]);
    you.props.erase(ORIGINAL_BRAND_KEY);
    you.duration[DUR_EXCRUCIATING_WOUNDS] = 0;

    if (verbose)
    {
        mprf(MSGCH_DURATION, "%s seems less pained.",
             weapon.name(DESC_YOUR).c_str());
    }

    you.wield_change = true;
    const brand_type real_brand = get_weapon_brand(weapon);
    if (real_brand == SPWPN_ANTIMAGIC)
        calc_mp();
    if (you.weapon() && is_holy_item(weapon) && you.form == transformation::lich)
    {
        mprf(MSGCH_DURATION, "%s falls away!", weapon.name(DESC_YOUR).c_str());
        unequip_item(EQ_WEAPON);
    }
}

spret cast_confusing_touch(int power, bool fail)
{
    fail_check();
    msg::stream << you.hands_act("begin", "to glow ")
                << (you.duration[DUR_CONFUSING_TOUCH] ? "brighter" : "red")
                << "." << endl;

    you.set_duration(DUR_CONFUSING_TOUCH,
                     max(10 + random2(power) / 5,
                         you.duration[DUR_CONFUSING_TOUCH]),
                     20, nullptr);
    you.props[CONFUSING_TOUCH_KEY] = power;

    return spret::success;
}
