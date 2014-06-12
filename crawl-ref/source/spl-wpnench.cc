/**
 * @file
 * @brief Weapon enchantment spells.
**/

#include "AppHdr.h"

#include "spl-wpnench.h"
#include "externs.h"

#include "areas.h"
#include "artefact.h"
#include "itemprop.h"
#include "makeitem.h"
#include "message.h"
#include "religion.h"
#include "shout.h"
#include "skills2.h"
#include "spl-miscast.h"
#include "stuff.h"

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

/** End your weapon branding spell.
 *
 * Returns the weapon to the previous brand, and ends the
 * DUR_WEAPON_BRAND.
 * @param verbose whether to print a message about expiration.
 */
void end_weapon_brand(bool verbose)
{
    ASSERT(you.duration[DUR_WEAPON_BRAND]);
    ASSERT(you.weapon());

    item_def& weapon = *you.weapon();
    const int temp_effect = get_weapon_brand(weapon);
    set_item_ego_type(weapon, OBJ_WEAPONS, you.props["orig brand"]);
    you.wield_change = true;
    you.props.erase("orig brand");
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
}

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
        if (weapon.sub_type == WPN_SLING)
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

    // Can't get out of it that easily...
    if (get_weapon_brand(weapon) == SPWPN_DISTORTION
        && !has_temp_brand
        && !you_worship(GOD_LUGONU))
    {
        const string prompt =
              "Really brand " + weapon.name(DESC_INVENTORY) + "?";
        if (!yesno(prompt.c_str(), false, 'n'))
        {
            canned_msg(MSG_OK);
            return SPRET_ABORT;
        }
        MiscastEffect(&you, WIELD_MISCAST, SPTYP_TRANSLOCATION,
                      9, 90, "a distortion unwield");
    }

    fail_check();

    string msg = weapon.name(DESC_YOUR);

    bool extending = has_temp_brand && get_weapon_brand(weapon) == which_brand;
    bool emit_special_message = !extending;
    int duration_affected = 10;
    switch (which_brand)
    {
    case SPWPN_FLAMING:
        msg += " bursts into flame!";
        duration_affected = 7;
        break;

    case SPWPN_FREEZING:
        msg += is_range_weapon(weapon) ? " frosts over!" : " glows blue.";
        duration_affected = 7;
        break;

    case SPWPN_VENOM:
        msg += " starts dripping with poison.";
        duration_affected = 15;
        break;

    case SPWPN_DRAINING:
        msg += " crackles with unholy energy.";
        duration_affected = 12;
        break;

    case SPWPN_VORPAL:
        msg += " glows silver and looks extremely sharp.";
        duration_affected = 10;
        break;

    case SPWPN_DISTORTION:
        msg += " seems to ";
        msg += random_choose("twist", "bend", "vibrate",
                                    "flex", "wobble", "twang", NULL);
        msg += (coinflip() ? " oddly." : " strangely.");
        duration_affected = 5;

        // Low duration, but power still helps.
        power /= 2;
        break;

    case SPWPN_PAIN:
        // Well, in theory, we could be silenced, but then how are
        // we casting the brand spell?
        // 1KB: Xom can cast it.  The Blade card currently can't.
        if (silenced(you.pos()))
            msg += " writhes in agony.";
        else
        {
            msg += " shrieks in agony.";
            noisy(15, you.pos());
        }
        duration_affected = 8;
        // We must repeat the special message here (as there's a side effect.)
        emit_special_message = true;
        break;

    case SPWPN_HOLY_WRATH:
        msg += " shines with holy light.";
        break;

    case SPWPN_ELECTROCUTION:
        msg += " starts to spark.";
        break;

    case SPWPN_ANTIMAGIC:
        msg += " depletes magic around it.";
        break;

    case SPWPN_CHAOS:
        msg += " glistens with random hues.";
        break;

    default:
        break;
    }

    if (!extending)
    {
        if (has_temp_brand)
            end_weapon_brand();
        you.props["orig brand"] = get_weapon_brand(weapon);
        set_item_ego_type(weapon, OBJ_WEAPONS, which_brand);
        you.wield_change = true;
    }

    if (emit_special_message)
        mpr(msg.c_str());
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
    msg::stream << "Your " << you.hand_name(true) << " begin to glow "
                << (you.duration[DUR_CONFUSING_TOUCH] ? "brighter" : "red")
                << "." << endl;

    you.set_duration(DUR_CONFUSING_TOUCH,
                     max(10 + random2(power) / 5,
                         you.duration[DUR_CONFUSING_TOUCH]),
                     20, NULL);

    return SPRET_SUCCESS;
}

spret_type cast_sure_blade(int power, bool fail)
{
    if (!you.weapon())
        mpr("You aren't wielding a weapon!");
    else if (weapon_skill(you.weapon()->base_type,
                          you.weapon()->sub_type) != SK_SHORT_BLADES)
    {
        mpr("You cannot bond with this weapon.");
    }
    else
    {
        fail_check();
        if (!you.duration[DUR_SURE_BLADE])
            mpr("You become one with your weapon.");
        else if (you.duration[DUR_SURE_BLADE] < 25 * BASELINE_DELAY)
            mpr("Your bond becomes stronger.");

        you.increase_duration(DUR_SURE_BLADE, 8 + (random2(power) / 10),
                              25, NULL);
        return SPRET_SUCCESS;
    }

    return SPRET_ABORT;
}
