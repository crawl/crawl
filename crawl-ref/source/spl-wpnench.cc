/**
 * @file
 * @brief Weapon enchantment spells.
**/

#include "AppHdr.h"

#include "spl-wpnench.h"

#include "areas.h"
#include "godpassive.h"
#include "itemprop.h"
#include "makeitem.h"
#include "message.h"
#include "prompt.h"
#include "religion.h"
#include "shout.h"
#include "spl-miscast.h"

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
}

/**
 * Temporarily brand a weapon with pain.
 *
 * @param[in] power         Spellpower.
 * @param[in] fail          Whether you've already failed to cast.
 * @return                  Success, fail, or abort.
 */
spret_type cast_excruciating_wounds(int power, bool fail)
{
    item_def& weapon = *you.weapon();
    const brand_type which_brand = SPWPN_PAIN;
    const brand_type orig_brand = get_weapon_brand(weapon);

    // Can only brand melee weapons.
    if (is_range_weapon(weapon))
    {
        mpr("You cannot brand ranged weapons with this spell.");
        return SPRET_ABORT;
    }

    bool has_temp_brand = you.duration[DUR_EXCRUCIATING_WOUNDS];
    if (!has_temp_brand && get_weapon_brand(weapon) == which_brand)
    {
        mpr("This weapon is already branded with pain.");
        return SPRET_ABORT;
    }

    const bool dangerous_disto = orig_brand == SPWPN_DISTORTION
                                 && !have_passive(passive_t::safe_distortion);
    if (dangerous_disto)
    {
        const string prompt =
              "Really brand " + weapon.name(DESC_INVENTORY) + "?";
        if (!yesno(prompt.c_str(), false, 'n'))
        {
            canned_msg(MSG_OK);
            return SPRET_ABORT;
        }
    }

    fail_check();

    if (dangerous_disto)
    {
        // Can't get out of it that easily...
        MiscastEffect(&you, nullptr, WIELD_MISCAST, SPTYP_TRANSLOCATION,
                      9, 90, "rebranding a weapon of distortion");
    }

    noisy(spell_effect_noise(SPELL_EXCRUCIATING_WOUNDS), you.pos());
    mprf("%s %s in agony.", weapon.name(DESC_YOUR).c_str(),
                            silenced(you.pos()) ? "writhes" : "shrieks");

    if (!has_temp_brand)
    {
        you.props[ORIGINAL_BRAND_KEY] = get_weapon_brand(weapon);
        set_item_ego_type(weapon, OBJ_WEAPONS, which_brand);
        you.wield_change = true;
        if (you.duration[DUR_SPWPN_PROTECTION])
        {
            you.duration[DUR_SPWPN_PROTECTION] = 0;
            you.redraw_armour_class = true;
        }
        if (orig_brand == SPWPN_ANTIMAGIC)
            calc_mp();
    }

    you.increase_duration(DUR_EXCRUCIATING_WOUNDS, 8 + roll_dice(2, power), 50);

    return SPRET_SUCCESS;
}

spret_type cast_confusing_touch(int power, bool fail)
{
    fail_check();
    msg::stream << you.hands_act("begin", "to glow ")
                << (you.duration[DUR_CONFUSING_TOUCH] ? "brighter" : "red")
                << "." << endl;

    you.set_duration(DUR_CONFUSING_TOUCH,
                     max(10 + random2(power) / 5,
                         you.duration[DUR_CONFUSING_TOUCH]),
                     20, nullptr);

    return SPRET_SUCCESS;
}
