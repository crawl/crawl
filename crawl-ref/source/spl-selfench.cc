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
#include "item_use.h"
#include "libutil.h"
#include "message.h"
#include "misc.h"
#include "options.h"
#include "religion.h"
#include "shout.h"
#include "spl-cast.h"
#include "spl-transloc.h"
#include "spl-util.h"
#include "stuff.h"
#include "transform.h"
#include "view.h"

int allowed_deaths_door_hp(void)
{
    int hp = you.skill(SK_NECROMANCY) / 2;

    if (you_worship(GOD_KIKUBAAQUDGHA) && !player_under_penance())
        hp += you.piety / 15;

    return max(hp, 1);
}

spret_type cast_deaths_door(int pow, bool fail)
{
    if (you.is_undead)
        mpr("You're already dead!");
    else if (you.duration[DUR_EXHAUSTED])
        mpr("You are too exhausted to enter Death's door!");
    else if (you.duration[DUR_DEATHS_DOOR])
        mpr("Your appeal for an extension has been denied.");
    else
    {
        fail_check();
        mpr("You feel invincible!");
        mpr("You seem to hear sand running through an hourglass...",
            MSGCH_SOUND);

        set_hp(allowed_deaths_door_hp());
        deflate_hp(you.hp_max, false);

        you.set_duration(DUR_DEATHS_DOOR, 10 + random2avg(13, 3)
                                           + (random2(pow) / 10));

        if (you.duration[DUR_DEATHS_DOOR] > 25 * BASELINE_DELAY)
            you.duration[DUR_DEATHS_DOOR] = (23 + random2(5)) * BASELINE_DELAY;
        return SPRET_SUCCESS;
    }

    return SPRET_ABORT;
}

void remove_ice_armour()
{
    mpr("Your icy armour melts away.", MSGCH_DURATION);
    you.redraw_armour_class = true;
    you.duration[DUR_ICY_ARMOUR] = 0;
}

spret_type ice_armour(int pow, bool fail)
{
    if (!player_effectively_in_light_armour())
    {
        mpr("You are wearing too much armour.");
        return SPRET_ABORT;
    }

    if (player_stoneskin() || you.form == TRAN_STATUE)
    {
        mpr("The film of ice won't work on stone.");
        return SPRET_ABORT;
    }

    if (you.duration[DUR_FIRE_SHIELD])
    {
        mpr("Your ring of flames would instantly melt the ice.");
        return SPRET_ABORT;
    }

    fail_check();

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

    return SPRET_SUCCESS;
}

spret_type missile_prot(int pow, bool fail)
{
    fail_check();
    you.increase_duration(DUR_REPEL_MISSILES, 8 + roll_dice(2, pow), 100,
                          "You feel protected from missiles.");
    return SPRET_SUCCESS;
}

spret_type deflection(int pow, bool fail)
{
    fail_check();
    you.increase_duration(DUR_DEFLECT_MISSILES, 15 + random2(pow), 100,
                          "You feel very safe from missiles.");
    return SPRET_SUCCESS;
}

void remove_regen(bool divine_ability)
{
    mpr("Your skin stops crawling.", MSGCH_DURATION);
    you.duration[DUR_REGENERATION] = 0;
    if (divine_ability)
    {
        mpr("You feel less resistant to hostile enchantments.", MSGCH_DURATION);
        you.attribute[ATTR_DIVINE_REGENERATION] = 0;
    }
}

spret_type cast_regen(int pow, bool divine_ability, bool fail)
{
    fail_check();
    you.increase_duration(DUR_REGENERATION, 5 + roll_dice(2, pow / 3 + 1), 100,
                          "Your skin crawls.");

    if (divine_ability)
    {
        mpr("You feel resistant to hostile enchantments.");
        you.attribute[ATTR_DIVINE_REGENERATION] = 1;
    }
    return SPRET_SUCCESS;
}

spret_type cast_revivification(int pow, bool fail)
{
    if (you.hp == you.hp_max)
        canned_msg(MSG_NOTHING_HAPPENS);
    else if (you.hp_max < 21)
        mpr("You lack the resilience to cast this spell.");
    else
    {
        fail_check();
        mpr("Your body is healed in an amazingly painful way.");

        int loss = 2;
        for (int i = 0; i < 9; ++i)
            if (x_chance_in_y(8, pow))
                loss++;

        dec_max_hp(loss * you.hp_max / 100);
        set_hp(you.hp_max);

        if (you.duration[DUR_DEATHS_DOOR])
        {
            mpr("Your life is in your own hands once again.", MSGCH_DURATION);
            // XXX: better cause name?
            paralyse_player("Death's Door abortion", 5 + random2(5));
            confuse_player(10 + random2(10));
            you.duration[DUR_DEATHS_DOOR] = 0;
        }
        return SPRET_SUCCESS;
    }

    return SPRET_ABORT;
}

spret_type cast_swiftness(int power, bool fail)
{
    if (you.form == TRAN_TREE)
    {
        canned_msg(MSG_CANNOT_MOVE);
        return SPRET_ABORT;
    }

    if (stasis_blocks_effect(true, true, "%s emits a piercing whistle.", 20,
                             "%s makes your neck tingle."))
    {
        return SPRET_ABORT;
    }

    if (!you.duration[DUR_SWIFTNESS] && player_movement_speed() <= 6)
    {
        mpr("You can't move any more quickly.");
        return SPRET_ABORT;
    }

    fail_check();

    if (you.in_liquid())
    {
        // Hint that the player won't be faster until they leave the liquid.
        mprf("The %s foams!", you.in_water() ? "water"
                            : you.in_lava()  ? "lava"
                                             : "liquid ground");
    }

    // [dshaligram] Removed the on-your-feet bit.  Sounds odd when
    // you're flying, for instance.
    you.increase_duration(DUR_SWIFTNESS, 20 + random2(power), 100,
                          you.in_liquid()
                              ? "You feel like you could be quicker."
                              : "You feel quick.");
    did_god_conduct(DID_HASTY, 8, true);

    return SPRET_SUCCESS;
}

spret_type cast_fly(int power, bool fail)
{
    if (!flight_allowed())
        return SPRET_ABORT;

    fail_check();
    const int dur_change = 25 + random2(power) + random2(power);
    const bool was_flying = you.airborne();

    you.increase_duration(DUR_FLIGHT, dur_change, 100);
    you.attribute[ATTR_FLIGHT_UNCANCELLABLE] = 1;

    if (!was_flying)
        float_player();
    else
        mpr("You feel more buoyant.");
    return SPRET_SUCCESS;
}

spret_type cast_teleport_control(int power, bool fail)
{
    fail_check();
    if (allow_control_teleport(true))
        mpr("You feel in control.");
    else
        mpr("You feel your control is inadequate.");

    if (you.duration[DUR_TELEPORT] && !player_control_teleport())
    {
        mpr("You feel your translocation being delayed.");
        you.increase_duration(DUR_TELEPORT, 1 + random2(3));
    }

    you.increase_duration(DUR_CONTROL_TELEPORT, 10 + random2(power), 50);

    return SPRET_SUCCESS;
}

int cast_selective_amnesia(string *pre_msg)
{
    if (you.spell_no == 0)
    {
        canned_msg(MSG_NO_SPELLS);
        return 0;
    }

    int keyin = 0;
    spell_type spell;
    int slot;

    // Pick a spell to forget.
    mpr("Forget which spell ([?*] list [ESC] exit)? ", MSGCH_PROMPT);
    keyin = Options.auto_list
            ? list_spells(false, false, false, "Forget which spell?")
            : get_ch();
    redraw_screen();

    while (true)
    {
        if (key_is_escape(keyin))
        {
            canned_msg(MSG_OK);
            return -1;
        }

        if (keyin == '?' || keyin == '*')
        {
            keyin = list_spells(false, false, false, "Forget which spell?");
            redraw_screen();
        }

        if (!isaalpha(keyin))
        {
            mesclr();
            keyin = get_ch();
            continue;
        }

        spell = get_spell_by_letter(keyin);
        slot = get_spell_slot_by_letter(keyin);

        if (spell == SPELL_NO_SPELL)
        {
            mpr("You don't know that spell.");
            mpr("Forget which spell ([?*] list [ESC] exit)? ", MSGCH_PROMPT);
            keyin = get_ch();
        }
        else
            break;
    }

    if (pre_msg)
        mpr(pre_msg->c_str());

    del_spell_from_memory_by_slot(slot);

    return 1;
}

spret_type cast_infusion(int pow, bool fail)
{
    fail_check();
    if (!you.duration[DUR_INFUSION])
        mpr("Your attacks are magically infused.");
    else
        mpr("Your attacks are magically infused for longer.");

    you.increase_duration(DUR_INFUSION,  8 + roll_dice(2, pow), 100);
    you.props["infusion_power"] = pow;

    return SPRET_SUCCESS;
}

spret_type cast_song_of_slaying(int pow, bool fail)
{
    fail_check();

    if (you.duration[DUR_SONG_OF_SLAYING])
        mpr("You start a new song!");
    else
        mpr("You start singing a song of slaying.");

    you.increase_duration(DUR_SONG_OF_SLAYING, 20 + pow / 3, 20 + pow / 3);

    noisy(10, you.pos());

    you.props["song_of_slaying_bonus"] = 0;
    return SPRET_SUCCESS;
}

spret_type cast_song_of_shielding(int pow, bool fail)
{
    fail_check();
    you.increase_duration(DUR_SONG_OF_SHIELDING, 10 + random2(pow) / 3, 40);
    mpr("You are being protected by your magic.");
    return SPRET_SUCCESS;
}

spret_type cast_silence(int pow, bool fail)
{
    fail_check();
    mpr("A profound silence engulfs you.");

    you.increase_duration(DUR_SILENCE, 10 + pow/4 + random2avg(pow/2, 2), 100);
    invalidate_agrid(true);

    if (you.beheld())
        you.update_beholders();

    learned_something_new(HINT_YOU_SILENCE);
    return SPRET_SUCCESS;
}

spret_type cast_liquefaction(int pow, bool fail)
{
    if (!you.stand_on_solid_ground())
    {
        if (!you.ground_level())
            mpr("You can't cast this spell without touching the ground.");
        else
            mpr("You need to be on clear, solid ground to cast this spell.");
        return SPRET_ABORT;
    }

    if (you.duration[DUR_LIQUEFYING] || liquefied(you.pos()))
    {
        mpr("The ground here is already liquefied! You'll have to wait.");
        return SPRET_ABORT;
    }

    fail_check();
    flash_view_delay(BROWN, 80);
    flash_view_delay(YELLOW, 80);
    flash_view_delay(BROWN, 140);

    mpr("The ground around you becomes liquefied!");

    you.increase_duration(DUR_LIQUEFYING, 10 + random2avg(pow, 2), 100);
    invalidate_agrid(true);
    return SPRET_SUCCESS;
}

spret_type cast_shroud_of_golubria(int pow, bool fail)
{
    fail_check();
    if (you.duration[DUR_SHROUD_OF_GOLUBRIA])
        mpr("You renew your shroud.");
    else
        mpr("Space distorts slightly along a thin shroud covering your body.");

    you.increase_duration(DUR_SHROUD_OF_GOLUBRIA, 7 + roll_dice(2, pow), 50);
    return SPRET_SUCCESS;
}

spret_type cast_transform(int pow, transformation_type which_trans, bool fail)
{
    if (!transform(pow, which_trans, false, true))
        return SPRET_ABORT;

    fail_check();
    transform(pow, which_trans);
    return SPRET_SUCCESS;
}
