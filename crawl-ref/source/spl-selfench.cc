/**
 * @file
 * @brief Self-enchantment spells.
**/

#include "AppHdr.h"

#include "spl-selfench.h"
#include "externs.h"

#include "areas.h"
#include "delay.h"
#include "env.h"
#include "godconduct.h"
#include "hints.h"
#include "itemprop.h"
#include "libutil.h"
#include "message.h"
#include "misc.h"
#include "potion.h"
#include "religion.h"
#include "spl-cast.h"
#include "spl-other.h"
#include "spl-util.h"
#include "stuff.h"
#include "transform.h"
#include "view.h"

int allowed_deaths_door_hp(void)
{
    int hp = you.skill(SK_NECROMANCY) / 2;

    if (you.religion == GOD_KIKUBAAQUDGHA && !player_under_penance())
        hp += you.piety / 15;

    return std::max(hp, 1);
}

bool cast_deaths_door(int pow)
{
    if (you.is_undead)
        mpr("You're already dead!");
    else if (you.duration[DUR_EXHAUSTED])
        mpr("You are too exhausted to enter Death's door!");
    else if (you.duration[DUR_DEATHS_DOOR])
        mpr("Your appeal for an extension has been denied.");
    else
    {
        mpr("You feel invincible!");
        mpr("You seem to hear sand running through an hourglass...",
            MSGCH_SOUND);

        set_hp(allowed_deaths_door_hp());
        deflate_hp(you.hp_max, false);

        you.set_duration(DUR_DEATHS_DOOR, 10 + random2avg(13, 3)
                                           + (random2(pow) / 10));

        if (you.duration[DUR_DEATHS_DOOR] > 25 * BASELINE_DELAY)
            you.duration[DUR_DEATHS_DOOR] = (23 + random2(5)) * BASELINE_DELAY;
        return true;
    }

    return false;
}

void remove_ice_armour()
{
    mpr("Your icy armour melts away.", MSGCH_DURATION);
    you.redraw_armour_class = true;
    you.duration[DUR_ICY_ARMOUR] = 0;
}

bool ice_armour(int pow)
{
    if (!player_effectively_in_light_armour())
    {
        mpr("You are wearing too much armour.");
        return (false);
    }

    if (you.duration[DUR_STONESKIN])
    {
        mpr("The spell conflicts with another spell still in effect.");
        return (false);
    }

    if (you.duration[DUR_ICY_ARMOUR])
        mpr("Your icy armour thickens.");
    else
    {
        if (you.form == TRAN_ICE_BEAST)
            mpr("Your icy body feels more resilient.");
        else
            mpr("A film of ice covers your body!");

        you.redraw_armour_class = true;
    }

    you.increase_duration(DUR_ICY_ARMOUR, 20 + random2(pow) + random2(pow), 50,
                          NULL);

    return (true);
}

void missile_prot(int pow)
{
    you.increase_duration(DUR_REPEL_MISSILES, 8 + roll_dice(2, pow), 100,
                          "You feel protected from missiles.");
}

void deflection(int pow)
{
    you.increase_duration(DUR_DEFLECT_MISSILES, 15 + random2(pow), 100,
                          "You feel very safe from missiles.");
}

void remove_regen(bool divine_ability)
{
    mpr("Your skin stops crawling.", MSGCH_DURATION);
    you.duration[DUR_REGENERATION] = 0;
    if (divine_ability)
    {
        mpr("You feel less resistant to magic.", MSGCH_DURATION);
        you.attribute[ATTR_DIVINE_REGENERATION] = 0;
    }
}

void cast_regen(int pow, bool divine_ability)
{
    you.increase_duration(DUR_REGENERATION, 5 + roll_dice(2, pow / 3 + 1), 100,
                          "Your skin crawls.");

    if (divine_ability)
    {
        mpr("You feel resistant to magic.");
        you.attribute[ATTR_DIVINE_REGENERATION] = 1;
    }
}

bool cast_swiftness(int power)
{
    if (you.in_water())
    {
        mpr("The water foams!");
        return (false);
    }

    if (!you.duration[DUR_SWIFTNESS] && player_movement_speed() <= 6)
    {
        mpr("You can't move any more quickly.");
        return (false);
    }

    // [dshaligram] Removed the on-your-feet bit.  Sounds odd when
    // you're levitating, for instance.
    you.increase_duration(DUR_SWIFTNESS, 20 + random2(power), 100,
                          "You feel quick.");
    did_god_conduct(DID_HASTY, 8, true);

    return (true);
}

void cast_fly(int power)
{
    const int dur_change = 25 + random2(power) + random2(power);
    const bool was_levitating = you.airborne();

    you.increase_duration(DUR_LEVITATION, dur_change, 100);
    you.increase_duration(DUR_CONTROLLED_FLIGHT, dur_change, 100);

    burden_change();

    if (!was_levitating)
        float_player(true);
    else
        mpr("You feel more buoyant.");
}

void cast_insulation(int power)
{
    you.increase_duration(DUR_INSULATION, 10 + random2(power), 100,
                          "You feel insulated.");
}

void cast_teleport_control(int power)
{
    you.increase_duration(DUR_CONTROL_TELEPORT, 10 + random2(power), 50,
                          "You feel in control.");
}

int cast_selective_amnesia(std::string *pre_msg)
{
    if (you.spell_no == 0)
    {
        canned_msg(MSG_NO_SPELLS);
        return (0);
    }

    int keyin = 0;
    spell_type spell;
    int slot;

    // Pick a spell to forget.
    while (true)
    {
        mpr("Forget which spell ([?*] list [ESC] exit)? ", MSGCH_PROMPT);
        keyin = get_ch();

        if (key_is_escape(keyin))
        {
            canned_msg(MSG_OK);
            return (-1);
        }

        if (keyin == '?' || keyin == '*')
        {
            keyin = list_spells(false, false, false);
            redraw_screen();
        }

        if (!isaalpha(keyin))
        {
            mesclr();
            continue;
        }

        spell = get_spell_by_letter(keyin);
        slot = get_spell_slot_by_letter(keyin);

        if (spell == SPELL_NO_SPELL)
            mpr("You don't know that spell.");
        else
            break;
    }

    if (pre_msg)
        mpr(pre_msg->c_str());

    const int ep_gain = spell_mana(spell);
    del_spell_from_memory_by_slot(slot);

    if (ep_gain > 0)
    {
        inc_mp(ep_gain);
        mpr("The spell releases its latent energy back to you as "
            "it unravels.");
    }

    return (1);
}

void cast_see_invisible(int pow)
{
    if (you.can_see_invisible())
        mpr("You feel as though your vision will be sharpened longer.");
    else
    {
        mpr("Your vision seems to sharpen.");

        // We might have to turn autopickup back on again.
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

    you.increase_duration(DUR_SILENCE, 10 + pow/4 + random2avg(pow/2, 2), 100);
    invalidate_agrid(true);

    if (you.beheld())
        you.update_beholders();

    learned_something_new(HINT_YOU_SILENCE);
}

void cast_liquefaction(int pow)
{
    flash_view_delay(BROWN, 80);
    flash_view_delay(YELLOW, 80);
    flash_view_delay(BROWN, 140);

    mpr("The ground around you becomes liquefied!");

    you.increase_duration(DUR_LIQUEFYING, 10 + random2avg(pow, 2), 100);
    invalidate_agrid(true);
}

void cast_shroud_of_golubria(int pow)
{
    if (you.duration[DUR_SHROUD_OF_GOLUBRIA])
        mpr("Your shroud grows in power.");
    else
        mpr("Space distorts slightly along a thin shroud covering your body.");

    you.increase_duration(DUR_SHROUD_OF_GOLUBRIA, 7 + roll_dice(2, pow), 50);
}
