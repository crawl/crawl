/*
 *  File:       spells2.cc
 *  Summary:    Implementations of some additional enchantment spells.
 *  Written by: Linley Henzell
 */

#include "AppHdr.h"

#include "spells2.h"
#include "externs.h"

#include "areas.h"
#include "artefact.h"
#include "delay.h"
#include "hints.h"
#include "itemprop.h"
#include "libutil.h"
#include "makeitem.h"
#include "message.h"
#include "shout.h"
#include "spl-cast.h"
#include "spl-util.h"
#include "stuff.h"


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
    case SPWPN_RETURNING: return SPMSL_RETURNING;
    default: return SPMSL_NORMAL; // there are no equivalents for the rest
                                  // of the ammo brands.
    }
}

// Some launchers need to convert different brands.
static brand_type _convert_to_launcher(brand_type which_brand)
{
    switch (which_brand)
    {
    case SPWPN_FREEZING: return SPWPN_FROST; case SPWPN_FLAMING: return SPWPN_FLAME;
    default: return (which_brand);
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
    case SPWPN_RETURNING:
    case SPWPN_CHAOS:
        return (true);
    default:
        return (false);
    }
}

bool brand_weapon(brand_type which_brand, int power)
{
    if (!you.weapon())
        return (false);

    bool temp_brand = you.duration[DUR_WEAPON_BRAND];
    item_def& weapon = *you.weapon();

    // Can't brand non-weapons, but can brand some launchers (see later).
    if (weapon.base_type != OBJ_WEAPONS)
        return (false);

    // But not blowguns.
    if (weapon.sub_type == WPN_BLOWGUN)
        return (false);

    // Can't brand artefacts.
    if (is_artefact(weapon))
        return (false);

    // Can't brand already-branded items.
    if (!temp_brand && get_weapon_brand(weapon) != SPWPN_NORMAL)
        return (false);

    // Some brandings are restricted to certain damage types.
    const int wpn_type = get_vorpal_type(weapon);
    if (which_brand == SPWPN_VENOM && wpn_type == DVORP_CRUSHING
        || which_brand == SPWPN_VORPAL && wpn_type != DVORP_SLICING
                                       && wpn_type != DVORP_STABBING
        || which_brand == SPWPN_DUMMY_CRUSHING && wpn_type != DVORP_CRUSHING)
    {
        return (false);
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
            return (false);

        // If the brand isn't appropriate for that launcher, also say no.
        if (!_ok_for_launchers(which_brand))
            return (false);

        // Otherwise, convert to the correct brand type, most specifically (but
        // not necessarily only) flaming -> flame, freezing -> frost.
        which_brand = _convert_to_launcher(which_brand);
    }

    // Allow rebranding a temporarily-branded item to a different brand.
    if (temp_brand && (get_weapon_brand(weapon) != which_brand))
    {
        you.duration[DUR_WEAPON_BRAND] = 0;
        set_item_ego_type(weapon, OBJ_WEAPONS, SPWPN_NORMAL);
        temp_brand = false;
    }

    std::string msg = weapon.name(DESC_CAP_YOUR);

    bool emit_special_message = !temp_brand;
    int duration_affected = 0;
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

    case SPWPN_DISTORTION:      //jmf: Added for Warp Weapon.
        msg += " seems to ";
        msg += random_choose_string("twist", "bend", "vibrate",
                                    "flex", "wobble", "twang", NULL);
        msg += (coinflip() ? " oddly." : " strangely.");
        duration_affected = 5;

        // [dshaligram] Clamping power to 2.
        power = 2;
        break;

    case SPWPN_PAIN:
        // Well, in theory, we could be silenced, but then how are
        // we casting the brand spell?
        msg += " shrieks in agony.";
        noisy(15, you.pos());
        duration_affected = 8;
        // We must repeat the special message here (as there's a side effect.)
        emit_special_message = true;
        break;

    case SPWPN_DUMMY_CRUSHING:  //jmf: Added for Maxwell's Silver Hammer.
        which_brand = SPWPN_VORPAL;
        msg += " glows silver and feels heavier.";
        duration_affected = 7;
        break;

    case SPWPN_RETURNING:
        msg += " wiggles in your hand.";
        duration_affected = 5;
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
        mprf("%s flashes.", weapon.name(DESC_CAP_YOUR).c_str());

    you.increase_duration(DUR_WEAPON_BRAND,
                          duration_affected + roll_dice(2, power), 50);

    return (true);
}

bool cast_selective_amnesia(bool force)
{
    if (you.spell_no == 0)
    {
        canned_msg(MSG_NO_SPELLS);
        return (false);
    }

    int keyin = 0;

    // Pick a spell to forget.
    while (true)
    {
        mpr("Forget which spell ([?*] list [ESC] exit)? ", MSGCH_PROMPT);
        keyin = get_ch();

        if (key_is_escape(keyin))
            return (false);

        if (keyin == '?' || keyin == '*')
        {
            keyin = list_spells(false);
            redraw_screen();
        }

        if (!isaalpha(keyin))
            mesclr();
        else
            break;
    }

    const spell_type spell = get_spell_by_letter(keyin);
    const int slot = get_spell_slot_by_letter(keyin);

    if (spell == SPELL_NO_SPELL)
    {
        mpr("You don't know that spell.");
        return (false);
    }

    if (!force
        && random2(you.skills[SK_SPELLCASTING])
           < random2(spell_difficulty(spell)))
    {
        mpr("Oops! This spell sure is a blunt instrument.");
        forget_map(20 + random2(50));
    }
    else
    {
        const int ep_gain = spell_mana(spell);
        del_spell_from_memory_by_slot(slot);

        if (ep_gain > 0)
        {
            inc_mp(ep_gain, false);
            mpr("The spell releases its latent energy back to you as "
                "it unravels.");
        }
    }

    return (true);
}

void cast_see_invisible(int pow)
{
    if (you.can_see_invisible())
        mpr("You feel as though your vision will be sharpened longer.");
    else
    {
        mpr("Your vision seems to sharpen.");

        // We might have to turn autopickup back on again.
        // TODO: Once the spell times out we might want to check all monsters
        //       in LOS for invisibility and turn autopickup off again, if
        //       needed.
        autotoggle_autopickup(false);
    }

    // No message if you already are under the spell.
    you.increase_duration(DUR_SEE_INVISIBLE, 10 + random2(2 + pow/2), 100);
}

void cast_silence(int pow)
{
    if (!you.attribute[ATTR_WAS_SILENCED])
        mpr("A profound silence engulfs you.");

    you.attribute[ATTR_WAS_SILENCED] = 1;

    you.increase_duration(DUR_SILENCE, 10 + random2avg(pow, 2), 100);
    invalidate_agrid(true);

    if (you.beheld())
        you.update_beholders();

    learned_something_new(HINT_YOU_SILENCE);
}
