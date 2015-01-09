/**
 * @file
 * @brief Weapon enchantment spells.
**/

#include "AppHdr.h"

#include "spl-wpnench.h"

#include "areas.h"
#include "itemprop.h"
#include "makeitem.h"
#include "message.h"
#include "prompt.h"
#include "religion.h"
#include "shout.h"
#include "spl-miscast.h"

// We need to know what brands equate with what missile brands to know if
// we should disallow temporary branding or not.
static special_missile_type _convert_to_missile(brand_type which_brand)
{
    switch (which_brand)
    {
    case SPWPN_NORMAL: return SPMSL_NORMAL;
    case SPWPN_FLAMING: return SPMSL_FLAME;
    case SPWPN_FREEZING: return SPMSL_FROST;
    case SPWPN_VENOM: return SPMSL_POISONED;
    case SPWPN_CHAOS: return SPMSL_CHAOS;
    default: return SPMSL_NORMAL; // there are no equivalents for the rest
                                  // of the ammo brands.
    }
}

static bool _ok_for_launchers(brand_type which_brand)
{
    switch (which_brand)
    {
    case SPWPN_NORMAL:
    case SPWPN_FREEZING:
    case SPWPN_FLAMING:
    case SPWPN_VENOM:
    //case SPWPN_PAIN: -- no pain missile type yet
    case SPWPN_CHAOS:
    case SPWPN_VORPAL:
        return true;
    default:
        return false;
    }
}

/**
 * Gets the message for when a weapon is branded.
 *
 * @param which_brand       The type of brand.
 * @param is_range_weapon   Whether the weapon is ranged.
 * @return                  The message for the brand being applied.
 */
static string _get_brand_msg(brand_type which_brand, bool is_range_weapon)
{
    string msg; // for distortion
    switch (which_brand)
    {
    case SPWPN_FLAMING:
        return " bursts into flame!";
    case SPWPN_FREEZING:
        return is_range_weapon ? " frosts over!" : " glows blue.";
    case SPWPN_VENOM:
        return " starts dripping with poison.";
    case SPWPN_DRAINING:
        return " crackles with unholy energy.";
    case SPWPN_VORPAL:
        return " glows silver and looks extremely dangerous.";
    case SPWPN_DISTORTION:
        msg = " seems to ";
        msg += random_choose("twist", "bend", "vibrate",
                             "flex", "wobble", "twang");
        msg += (coinflip() ? " oddly." : " strangely.");
        return msg;
    case SPWPN_PAIN:
        return silenced(you.pos()) ? " writhes in agony." :
                                        " shrieks in agony.";
    case SPWPN_HOLY_WRATH:
        return " shines with holy light.";
    case SPWPN_ELECTROCUTION:
        return " starts to spark.";
    case SPWPN_ANTIMAGIC:
        return " depletes magic around it.";
    case SPWPN_CHAOS:
        return " glistens with random hues.";
    default:
        return " wibbles buggily.";
    }
}

/** End your weapon branding spell.
 *
 * Returns the weapon to the previous brand, and ends the
 * DUR_WEAPON_BRAND.
 * @param weapon The item in question (which may have just been unwielded).
 * @param verbose whether to print a message about expiration.
 */
void end_weapon_brand(item_def &weapon, bool verbose)
{
    ASSERT(you.duration[DUR_WEAPON_BRAND]);

    const brand_type temp_effect = get_weapon_brand(weapon);
    set_item_ego_type(weapon, OBJ_WEAPONS, you.props[ORIGINAL_BRAND_KEY]);
    you.props.erase(ORIGINAL_BRAND_KEY);
    you.duration[DUR_WEAPON_BRAND] = 0;

    if (verbose)
    {
        const char *msg = nullptr;

        switch (temp_effect)
        {
        case SPWPN_VORPAL:
            if (get_vorpal_type(weapon) == DVORP_SLICING)
                msg = " seems blunter.";
            else
                msg = " feels lighter.";
            break;
        case SPWPN_ANTIMAGIC:
            msg = " stops repelling magic.";
            calc_mp();
            break;
        case SPWPN_FLAMING:       msg = " goes out."; break;
        case SPWPN_FREEZING:      msg = " stops glowing."; break;
        case SPWPN_VENOM:         msg = " stops dripping with poison."; break;
        case SPWPN_DRAINING:      msg = " stops crackling."; break;
        case SPWPN_DISTORTION:    msg = " seems straighter."; break;
        case SPWPN_PAIN:          msg = " seems less pained."; break;
        case SPWPN_CHAOS:         msg = " seems more stable."; break;
        case SPWPN_ELECTROCUTION: msg = " stops emitting sparks."; break;
        case SPWPN_HOLY_WRATH:    msg = "'s light goes out."; break;
        default: msg = " seems inexplicably less special."; break;
        }

        mprf(MSGCH_DURATION, "%s%s", weapon.name(DESC_YOUR).c_str(), msg);
    }

    you.wield_change = true;
    const brand_type real_brand = get_weapon_brand(weapon);
    if (real_brand == SPWPN_PROTECTION || temp_effect == SPWPN_PROTECTION)
        you.redraw_armour_class = true;
    else if (real_brand == SPWPN_EVASION || temp_effect == SPWPN_EVASION)
        you.redraw_evasion = true;
}

/**
 *  Gets the duration modifier for a given brand type.
 *
 * @param which_brand       The type of brand.
 * @return                  The duration modifier, default 10.
 */
static int _get_brand_duration(brand_type which_brand)
{
    switch (which_brand)
    {
    case SPWPN_FLAMING:
    case SPWPN_FREEZING:
        return 7;
    case SPWPN_VENOM:
        return 15;
    case SPWPN_DRAINING:
        return 12;
    case SPWPN_DISTORTION:
        return 5;
    case SPWPN_PAIN:
        return 8;
    default:
        return 10;
    }
 }

/**
 * Temporarily brand a weapon.
 *
 * @param[in] which_brand   The brand type to apply.
 * @param[in] power         Spellpower.
 * @param[in] fail          Whether you've already failed to cast.
 * @return                  Success, fail, or abort.
 */
spret_type brand_weapon(brand_type which_brand, int power, bool fail)
{
    if (!you.weapon())
    {
        mpr("You aren't wielding a weapon.");
        return SPRET_ABORT;
    }

    item_def& weapon = *you.weapon();

    if (!is_brandable_weapon(weapon, true))
    {
        if (weapon.base_type != OBJ_WEAPONS)
            mpr("This isn't a weapon.");
        else
            mpr("You cannot enchant this weapon.");
        return SPRET_ABORT;
    }

    bool has_temp_brand = you.duration[DUR_WEAPON_BRAND];
    // No need to brand with a brand it's already branded with.
    if (!has_temp_brand && get_weapon_brand(weapon) == which_brand)
    {
        mpr("This weapon is already enchanted with that brand.");
        return SPRET_ABORT;
    }

    // Can only brand launchers with sensible brands.
    if (is_range_weapon(weapon))
    {
        // If the new missile type wouldn't match the launcher, say no.
        missile_type missile = fires_ammo_type(weapon);

        // XXX: To deal with the fact that is_missile_brand_ok will be
        // unhappy if we attempt to brand stones, tell it we're using
        // sling bullets instead.
        if (item_attack_skill(weapon) == SK_SLINGS)
            missile = MI_SLING_BULLET;

        if (!is_missile_brand_ok(missile, _convert_to_missile(which_brand), true))
        {
            mpr("You cannot enchant this weapon with this spell.");
            return SPRET_ABORT;
        }

        // If the brand isn't appropriate for that launcher, also say no.
        if (!_ok_for_launchers(which_brand))
        {
            mpr("You cannot enchant this weapon with this spell.");
            return SPRET_ABORT;
        }
    }

    const brand_type orig_brand = get_weapon_brand(weapon);
    const bool dangerous_disto = orig_brand == SPWPN_DISTORTION
                                 && !has_temp_brand
                                 && !you_worship(GOD_LUGONU);

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

    string msg = weapon.name(DESC_YOUR);

    bool extending = has_temp_brand && orig_brand == which_brand;
    bool emit_special_message = !extending;
    int duration_affected = _get_brand_duration(which_brand);
    msg += _get_brand_msg(which_brand, is_range_weapon(weapon));

    if (which_brand == SPWPN_DISTORTION)
        power /= 2;
    else if (which_brand == SPWPN_PAIN && !silenced(you.pos()))
    {
        noisy(spell_effect_noise(SPELL_EXCRUCIATING_WOUNDS), you.pos());
        // We must repeat the special message here (as there's a side effect.)
        emit_special_message = true;
    }

    if (!extending)
    {
        if (has_temp_brand)
            end_weapon_brand(weapon);
        you.props[ORIGINAL_BRAND_KEY] = get_weapon_brand(weapon);

        set_item_ego_type(weapon, OBJ_WEAPONS, which_brand);
        you.wield_change = true;
        if (orig_brand == SPWPN_PROTECTION || which_brand == SPWPN_PROTECTION)
            you.redraw_armour_class = true;
        else if (orig_brand == SPWPN_EVASION || which_brand == SPWPN_EVASION)
            you.redraw_evasion = true;
    }

    if (emit_special_message)
        mpr(msg);
    else
        mprf("%s flashes.", weapon.name(DESC_YOUR).c_str());

    you.increase_duration(DUR_WEAPON_BRAND,
                          duration_affected + roll_dice(2, power), 50);
    if (which_brand == SPWPN_ANTIMAGIC)
        calc_mp();

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

spret_type cast_sure_blade(int power, bool fail)
{
    if (!you.weapon())
        mpr("You aren't wielding a weapon!");
    else if (item_attack_skill(*you.weapon()) != SK_SHORT_BLADES)
        mpr("You cannot bond with this weapon.");
    else
    {
        fail_check();
        if (!you.duration[DUR_SURE_BLADE])
            mpr("You become one with your weapon.");
        else if (you.duration[DUR_SURE_BLADE] < 25 * BASELINE_DELAY)
            mpr("Your bond becomes stronger.");

        you.increase_duration(DUR_SURE_BLADE, 8 + (random2(power) / 10),
                              25, nullptr);
        return SPRET_SUCCESS;
    }

    return SPRET_ABORT;
}
