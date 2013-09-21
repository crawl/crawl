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
#include "shout.h"
#include "skills2.h"


// We need to know what brands equate with what missile brands to know if
// we should disallow temporary branding or not.
static special_missile_type _convert_to_missile(brand_type which_brand)
{
    switch (which_brand)
    {
    case SPWPN_NORMAL: return SPMSL_NORMAL;
    case SPWPN_FLAME: // deliberate fall through
    case SPWPN_FLAMING: return SPMSL_FLAME;
    case SPWPN_FROST: // deliberate fall through
    case SPWPN_FREEZING: return SPMSL_FROST;
    case SPWPN_VENOM: return SPMSL_POISONED;
    case SPWPN_CHAOS: return SPMSL_CHAOS;
    default: return SPMSL_NORMAL; // there are no equivalents for the rest
                                  // of the ammo brands.
    }
}

// Some launchers need to convert different brands.
static brand_type _convert_to_launcher(brand_type which_brand)
{
    switch (which_brand)
    {
    case SPWPN_FREEZING: return SPWPN_FROST;
    case SPWPN_FLAMING: return SPWPN_FLAME;
    default: return which_brand;
    }
}

static bool _ok_for_launchers(brand_type which_brand)
{
    switch (which_brand)
    {
    case SPWPN_NORMAL:
    case SPWPN_FREEZING:
    case SPWPN_FROST:
    case SPWPN_FLAMING:
    case SPWPN_FLAME:
    case SPWPN_VENOM:
    //case SPWPN_PAIN: -- no pain missile type yet
    case SPWPN_CHAOS:
    case SPWPN_VORPAL:
        return true;
    default:
        return false;
    }
}

spret_type brand_weapon(brand_type which_brand, int power, bool fail)
{
    if (!you.weapon())
    {
        mpr("You aren't wielding a weapon.");
        return SPRET_ABORT;
    }

    bool temp_brand = you.duration[DUR_WEAPON_BRAND];
    item_def& weapon = *you.weapon();

    if (!is_brandable_weapon(weapon, true))
    {
        if (weapon.base_type != OBJ_WEAPONS)
            mpr("This isn't a weapon.");
        else
            mpr("You cannot enchant this weapon.");
        return SPRET_ABORT;
    }

    // Can't brand already-branded items.
    if (!temp_brand && get_weapon_brand(weapon) != SPWPN_NORMAL)
    {
        mpr("This weapon is already enchanted.");
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

        // Otherwise, convert to the correct brand type, most specifically (but
        // not necessarily only) flaming -> flame, freezing -> frost.
        which_brand = _convert_to_launcher(which_brand);
    }

    fail_check();

    // Allow rebranding a temporarily-branded item to a different brand.
    if (temp_brand && (get_weapon_brand(weapon) != which_brand))
    {
        you.duration[DUR_WEAPON_BRAND] = 0;
        set_item_ego_type(weapon, OBJ_WEAPONS, SPWPN_NORMAL);
        temp_brand = false;
    }

    string msg = weapon.name(DESC_YOUR);

    bool emit_special_message = !temp_brand;
    int duration_affected = 10;
    switch (which_brand)
    {
    case SPWPN_FLAME:
    case SPWPN_FLAMING:
        msg += " bursts into flame!";
        duration_affected = 7;
        break;

    case SPWPN_FROST:
        msg += " frosts over!";
        duration_affected = 7;
        break;

    case SPWPN_FREEZING:
        msg += " glows blue.";
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

    if (!temp_brand)
    {
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

    you.increase_duration(DUR_CONFUSING_TOUCH, 5 + (random2(power) / 5),
                          50, NULL);

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
