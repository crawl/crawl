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

/**
 * Temporarily brand a weapon with pain.
 *
 * @param[in] power         Spellpower.
 * @param[in] fail          Whether you've already failed to cast.
 * @return                  Success, fail, or abort.
 */
spret cast_excruciating_wounds(int power, bool fail)
{
    item_def& weapon = *you.weapon();
    const brand_type which_brand = SPWPN_PAIN;
    const brand_type orig_brand = get_weapon_brand(weapon);

    // Can only brand melee weapons.
    if (is_range_weapon(weapon))
    {
        mpr("You cannot brand ranged weapons with this spell.");
        return spret::abort;
    }

    bool has_temp_brand = you.duration[DUR_EXCRUCIATING_WOUNDS];
    if (!has_temp_brand && get_weapon_brand(weapon) == which_brand)
    {
        mpr("This weapon is already branded with pain.");
        return spret::abort;
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
            return spret::abort;
        }
    }

    fail_check();

    if (dangerous_disto)
        unwield_distortion(true);

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
        monster * spectral = find_spectral_weapon(&you);
        if (orig_brand == SPWPN_SPECTRAL && spectral)
            end_spectral_weapon(spectral, false);
    }

    you.increase_duration(DUR_EXCRUCIATING_WOUNDS, 8 + roll_dice(2, power), 50);

    return spret::success;
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
    you.props["confusing touch power"] = power;

    return spret::success;
}
