/**
 * @file
 * @brief Self-enchantment spells.
**/

#include "AppHdr.h"

#include "spl-selfench.h"

#include <cmath>

#include "act-iter.h"
#include "areas.h"
#include "art-enum.h"
#include "coordit.h" // radius_iterator
#include "env.h"
#include "god-passive.h"
#include "hints.h"
#include "items.h" // stack_iterator
#include "libutil.h"
#include "message.h"
#include "output.h"
#include "player.h"
#include "prompt.h"
#include "religion.h"
#include "spl-other.h"
#include "spl-util.h"
#include "stringutil.h"
#include "terrain.h"
#include "transform.h"
#include "tilepick.h"
#include "view.h"

spret cast_deaths_door(int pow, bool fail)
{
    fail_check();
    mpr("You stand defiantly in death's doorway!");
    mprf(MSGCH_SOUND, "You seem to hear sand running through an hourglass...");

    you.set_duration(DUR_DEATHS_DOOR, 10 + random2avg(13, 3)
                                       + div_rand_round(random2(pow), 10));

    const int hp = max(div_rand_round(pow, 10), 1);
    you.attribute[ATTR_DEATHS_DOOR_HP] = hp;
    set_hp(hp);

    if (you.duration[DUR_DEATHS_DOOR] > 25 * BASELINE_DELAY)
        you.duration[DUR_DEATHS_DOOR] = (23 + random2(5)) * BASELINE_DELAY;
    return spret::success;
}

void remove_ice_armour()
{
    mprf(MSGCH_DURATION, "Your icy armour melts away.");
    you.redraw_armour_class = true;
    you.duration[DUR_ICY_ARMOUR] = 0;
}

spret ice_armour(int pow, bool fail)
{
    fail_check();

    if (you.duration[DUR_ICY_ARMOUR])
        mpr("Your icy armour thickens.");
    else
        mpr("A film of ice covers your body!");

    you.increase_duration(DUR_ICY_ARMOUR, random_range(40, 50), 50);
    you.props[ICY_ARMOUR_KEY] = pow;
    you.redraw_armour_class = true;

    return spret::success;
}

void fiery_armour()
{
    if (you.duration[DUR_FIERY_ARMOUR])
        mpr("Your cloak of flame flares fiercely!");
    else if (you.duration[DUR_ICY_ARMOUR] || player_icemail_armour_class())
    {
        mpr("A sizzling cloak of flame settles atop your icy armour.");
        // TODO: add corresponding inverse message for casting ozo's etc
        // while DUR_FIERY_ARMOUR is active (maybe..?)
    }
    else
        mpr("A protective cloak of flame settles atop you.");

    you.increase_duration(DUR_FIERY_ARMOUR, random_range(110, 140), 1500);
    you.redraw_armour_class = true;
}

spret cast_revivification(int pow, bool fail)
{
    fail_check();
    mpr("Your body is healed in an amazingly painful way.");

    const int loss = 6 + binomial(9, 8, pow);
    dec_max_hp(loss * you.hp_max / 100);
    set_hp(you.hp_max);

    if (you.duration[DUR_DEATHS_DOOR])
    {
        mprf(MSGCH_DURATION, "Your life is in your own hands once again.");
        // XXX: better cause name?
        paralyse_player("Death's Door abortion");
        you.duration[DUR_DEATHS_DOOR] = 0;
    }
    return spret::success;
}

spret cast_swiftness(int power, bool fail)
{
    fail_check();

    you.set_duration(DUR_SWIFTNESS, 12 + random2(power)/2, 30,
                     "You feel quick.");
    you.attribute[ATTR_SWIFTNESS] = you.duration[DUR_SWIFTNESS];

    return spret::success;
}

int cast_selective_amnesia(const string &pre_msg)
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
    keyin = list_spells(false, false, false, "forget");
    redraw_screen();
    update_screen();

    if (isaalpha(keyin))
    {
        spell = get_spell_by_letter(keyin);
        slot = get_spell_slot_by_letter(keyin);

        const bool in_library = you.spell_library[spell];
        if (spell != SPELL_NO_SPELL)
        {
            const string prompt = make_stringf(
                    "Forget %s, freeing %d spell level%s for a total of %d?%s",
                    spell_title(spell), spell_levels_required(spell),
                    spell_levels_required(spell) != 1 ? "s" : "",
                    player_spell_levels(false) + spell_levels_required(spell),
                    in_library ? "" : " This spell is not in your library!");

            if (yesno(prompt.c_str(), in_library, 'n', false))
            {
                if (!pre_msg.empty())
                    mpr(pre_msg);
                del_spell_from_memory_by_slot(slot);
                return 1;
            }
        }
    }

    return -1;
}

spret cast_fugue_of_the_fallen(int pow, bool fail)
{
    fail_check();

    if (you.duration[DUR_FUGUE])
        mpr("You release your grip on the fallen and begin the cycle anew!");
    else
        mpr("You call out to the remnants of the fallen!");

    you.set_duration(DUR_FUGUE, 25 + random2avg(pow, 2));

    you.props[FUGUE_KEY] = 0;
    return spret::success;
}

void do_fugue_wail(const coord_def pos)
{
    // Do burst of negative energy damage around the spot that was hit by an
    // attack with max fugue stacks. Hit anything which isn't friendly and
    // immune to negative energy.
    vector <monster*> affected;
    for (adjacent_iterator ai(pos); ai; ++ai)
    {
        if (monster_at(*ai) && !mons_is_firewood(*monster_at(*ai))
            && !monster_at(*ai)->wont_attack()
            && monster_at(*ai)->res_negative_energy() < 3)
        {
            affected.push_back(monster_at(*ai));
        }
    }

    int pow = calc_spell_power(SPELL_FUGUE_OF_THE_FALLEN);

    if (!affected.empty())
        mpr("The fallen lash out in pain!");
    for (monster *m : affected)
    {
        if (m->alive())
        {
            m->hurt(&you, roll_dice(2, 3 + div_rand_round(pow, 25)),
                    BEAM_NEG, KILLED_BY_BEAM);
        }
    }
}

int silence_min_range(int pow)
{
    return shrinking_aoe_range((20 + pow/5) * BASELINE_DELAY);
}

int silence_max_range(int pow)
{
    return shrinking_aoe_range((19 + pow/5 + pow/2) * BASELINE_DELAY);
}

spret cast_silence(int pow, bool fail)
{
    fail_check();
    mpr("A profound silence engulfs you.");

    you.increase_duration(DUR_SILENCE, 20 + div_rand_round(pow,5)
                            + random2avg(div_rand_round(pow,2), 2), 100);
    invalidate_agrid(true);

    if (you.beheld())
        you.update_beholders();

    learned_something_new(HINT_YOU_SILENCE);
    return spret::success;
}

int liquefaction_max_range(int pow)
{
    return shrinking_aoe_range((9 + pow) * BASELINE_DELAY);
}

spret cast_liquefaction(int pow, bool fail)
{
    fail_check();
    flash_view_delay(UA_PLAYER, BROWN, 80);
    flash_view_delay(UA_PLAYER, YELLOW, 80);
    flash_view_delay(UA_PLAYER, BROWN, 140);

    mpr("The ground around you becomes liquefied!");

    you.increase_duration(DUR_LIQUEFYING, 15 + random2avg(pow, 2), 100);
    invalidate_agrid(true);
    return spret::success;
}

// Is there at least one valid hostile thing in sight?
bool jinxbite_targets_available()
{
    for (monster_near_iterator mi(&you, LOS_NO_TRANS); mi; ++mi)
    {
        if (mons_is_threatening(**mi) && !mi->wont_attack()
            && mi->willpower() != WILL_INVULN)
        {
            return true;
        }
    }

    return false;
}

spret cast_jinxbite(int pow, bool fail)
{
    if (!jinxbite_targets_available())
    {
        mpr("There is nobody nearby that the sprites are interested in.");
        return spret::abort;
    }

    fail_check();

    mprf("You beckon %s vexing sprites to accompany your attacks.",
         you.duration[DUR_JINXBITE] ? "more" : "some");

    const int base_dur = random_range(9, 15);
    const int will_dur = random_range(base_dur, 15) +
                         div_rand_round(spell_power_cap(SPELL_JINXBITE), 4);

    you.increase_duration(DUR_JINXBITE, base_dur + div_rand_round(pow, 4), 28);
    you.increase_duration(DUR_LOWERED_WL, will_dur, 28,
                          "You feel your willpower being sapped.");

    return spret::success;
}

spret cast_confusing_touch(int power, bool fail)
{
    fail_check();
    msg::stream << you.hands_act("begin", "to glow ")
                << (you.duration[DUR_CONFUSING_TOUCH] ? "brighter" : "red")
                << "." << endl;

    you.set_duration(DUR_CONFUSING_TOUCH,
                     max(10 + div_rand_round(random2(1 + power), 5),
                         you.duration[DUR_CONFUSING_TOUCH]),
                     20, nullptr);
    you.props[CONFUSING_TOUCH_KEY] = power;

    return spret::success;
}
