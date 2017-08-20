/**
 * @file
 * @brief Self-enchantment spells.
**/

#include "AppHdr.h"

#include "spl-selfench.h"

#include <cmath>

#include "areas.h"
#include "art-enum.h"
#include "butcher.h" // butcher_corpse
#include "coordit.h" // radius_iterator
#include "god-conduct.h"
#include "god-passive.h"
#include "hints.h"
#include "items.h" // stack_iterator
#include "libutil.h"
#include "macro.h"
#include "message.h"
#include "output.h"
#include "religion.h"
#include "showsymb.h"
#include "spl-transloc.h"
#include "spl-util.h"
#include "transform.h"
#include "tilepick.h"
#include "view.h"
#include "viewchar.h"

int allowed_deaths_door_hp()
{
    int hp = calc_spell_power(SPELL_DEATHS_DOOR, true) / 10;

    return max(hp, 1);
}

spret_type cast_deaths_door(int pow, bool fail)
{
    fail_check();
    mpr("당신은 죽음의 문턱에 들어섰다!");
    mprf(MSGCH_SOUND, "모래시계에서 모래가 떨어지는 소리를 들은 것 같다...");

    set_hp(allowed_deaths_door_hp());
    deflate_hp(you.hp_max, false);

    you.set_duration(DUR_DEATHS_DOOR, 10 + random2avg(13, 3)
                                       + (random2(pow) / 10));

    if (you.duration[DUR_DEATHS_DOOR] > 25 * BASELINE_DELAY)
        you.duration[DUR_DEATHS_DOOR] = (23 + random2(5)) * BASELINE_DELAY;
    return SPRET_SUCCESS;
}

void remove_ice_armour()
{
    mprf(MSGCH_DURATION, "당신의 얼음 갑옷이 녹아 내린다.");
    you.redraw_armour_class = true;
    you.duration[DUR_ICY_ARMOUR] = 0;
}

spret_type ice_armour(int pow, bool fail)
{
    fail_check();

    if (you.duration[DUR_ICY_ARMOUR])
        mpr("당신의 얼음 갑옷은 두꺼워졌다.");
    else if (you.form == transformation::ice_beast)
        mpr("당신의 얼음 몸체가 더 탄력있게 느껴진다.");
    else
        mpr("얇은 얼음이 당신의 몸을 뒤덮는다!");

    if (you.attribute[ATTR_BONE_ARMOUR] > 0)
    {
        you.attribute[ATTR_BONE_ARMOUR] = 0;
        mpr("당신의 시체 갑옷이 떨어져 사라졌다.");
    }

    you.increase_duration(DUR_ICY_ARMOUR, random_range(40, 50), 50);
    you.props[ICY_ARMOUR_KEY] = pow;
    you.redraw_armour_class = true;

    return SPRET_SUCCESS;
}

/**
 * Iterate over all corpses in LOS and harvest them (unless it's just a test
 * run)
 *
 * @param harvester   The entity planning to do the harvesting.
 * @param dry_run     Whether this is a test run & no corpses should be
 *                    actually destroyed.
 * @param defy_god    Whether to ignore religious restrictions on defiling
 *                    corpses.
 * @return            The total number of corpses (available to be) destroyed.
 */
int harvest_corpses(const actor &harvester, bool dry_run, bool defy_god)
{
    int harvested = 0;

    for (radius_iterator ri(harvester.pos(), LOS_NO_TRANS); ri; ++ri)
    {
        for (stack_iterator si(*ri, true); si; ++si)
        {
            item_def &item = *si;
            if (item.base_type != OBJ_CORPSES)
                continue;

            // forbid harvesting orcs under Beogh
            if (you.religion == GOD_BEOGH && !defy_god)
            {
                const monster_type monnum
                    = static_cast<monster_type>(item.orig_monnum);
                if (mons_genus(monnum) == MONS_ORC)
                    continue;
            }

            ++harvested;

            if (dry_run)
                continue;

            // don't spam animations
            if (harvested <= 5)
            {
                bolt beam;
                beam.source = *ri;
                beam.target = harvester.pos();
                beam.glyph = get_item_glyph(item).ch;
                beam.colour = item.get_colour();
                beam.range = LOS_RADIUS;
                beam.aimed_at_spot = true;
                beam.item = &item;
                beam.flavour = BEAM_VISUAL;
                beam.draw_delay = 3;
                beam.fire();
                viewwindow();
            }

            destroy_item(item.index());
        }
    }

    return harvested;
}

/**
 * Casts the player spell "Cigotuvi's Embrace", pulling all corpses into LOS
 * around the caster to serve as armour.
 *
 * @param pow   The spellpower at which the spell is being cast.
 * @param fail  Whether the casting failed.
 * @return      SPRET_ABORT if you already have an incompatible buff running,
 *              SPRET_FAIL if fail is true, and SPRET_SUCCESS otherwise.
 */
spret_type corpse_armour(int pow, bool fail)
{
    // Could check carefully to see if it's even possible that there are any
    // valid corpses/skeletons in LOS (any piles with stuff under them, etc)
    // before failing, but it's better to be simple + predictable from the
    // player's perspective.
    fail_check();

    const int harvested = harvest_corpses(you);
    dprf("Harvested: %d", harvested);

    if (!harvested)
    {
        if (harvest_corpses(you, true, true))
            mpr("이 시체들을 훼손하는 것은 죄가 될 것이다!");
        else
            canned_msg(MSG_NOTHING_HAPPENS);
        return SPRET_SUCCESS; // still takes a turn, etc
    }

    if (you.attribute[ATTR_BONE_ARMOUR] <= 0)
        mpr("죽은 자의 육체가 당신을 감싸기 위해 다가왔다!");
    else
        mpr("당신을 둘러싸고 있는 살점과 뼈가 두꺼워졌다.");

    // value of ATTR_BONE_ARMOUR will be sqrt(9*harvested), rounded randomly
    int squared = sqr(you.attribute[ATTR_BONE_ARMOUR]) + 9 * harvested;
    you.attribute[ATTR_BONE_ARMOUR] = sqrt(squared) + random_real();
    you.redraw_armour_class = true;

    return SPRET_SUCCESS;
}

spret_type deflection(int pow, bool fail)
{
    fail_check();
    you.attribute[ATTR_DEFLECT_MISSILES] = 1;
    mpr("당신은 투사체로부터 매우 안전하다고 느낀다.");

    return SPRET_SUCCESS;
}

spret_type cast_regen(int pow, bool fail)
{
    fail_check();
    you.increase_duration(DUR_REGENERATION, 5 + roll_dice(2, pow / 3 + 1), 100,
                          "Your skin crawls.");

    return SPRET_SUCCESS;
}

spret_type cast_revivification(int pow, bool fail)
{
    fail_check();
    mpr("당신의 몸이 놀랍도록 고통스러운 방식으로 치유된다.");

    const int loss = 6 + binomial(9, 8, pow);
    dec_max_hp(loss * you.hp_max / 100);
    set_hp(you.hp_max);

    if (you.duration[DUR_DEATHS_DOOR])
    {
        mprf(MSGCH_DURATION, "삶이 당신의 손으로 다시 한번 되돌아왔다.");
        // XXX: better cause name?
        paralyse_player("Death's Door abortion");
        you.duration[DUR_DEATHS_DOOR] = 0;
    }
    return SPRET_SUCCESS;
}

spret_type cast_swiftness(int power, bool fail)
{
    fail_check();

    if (you.in_liquid())
    {
        // Hint that the player won't be faster until they leave the liquid.
        mprf("%s 거품!", you.in_water() ? "물"
                                             : "액화된");
    }

    you.set_duration(DUR_SWIFTNESS, 12 + random2(power)/2, 30,
                     "당신은 빨라진 것을 느낀다.");
    you.attribute[ATTR_SWIFTNESS] = you.duration[DUR_SWIFTNESS];

    return SPRET_SUCCESS;
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
    mprf(MSGCH_PROMPT, "어떤 주문을 잊겠습니까? ([?*] 목록 [ESC] 취소)");
    keyin = list_spells(false, false, false, "Forget which spell?");
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
            clear_messages();
            mprf(MSGCH_PROMPT, "어떤 주문을 잊겠습니까? ([?*] 목록 [ESC] 취소)");
            keyin = get_ch();
            continue;
        }

        spell = get_spell_by_letter(keyin);
        slot = get_spell_slot_by_letter(keyin);

        if (spell == SPELL_NO_SPELL)
        {
            mpr("당신은 그 주문을 모른다.");
            mprf(MSGCH_PROMPT, "어떤 주문을 잊겠습니까? ([?*] 목록 [ESC] 취소)");
            keyin = get_ch();
        }
        else
            break;
    }

    if (!pre_msg.empty())
        mpr(pre_msg);

    del_spell_from_memory_by_slot(slot);

    return 1;
}

spret_type cast_infusion(int pow, bool fail)
{
    fail_check();
    if (!you.duration[DUR_INFUSION])
        mpr("당신은 당신의 공격에 마력을 주입했다.");
    else
        mpr("당신이 시전한 마력 주입의 시간이 연장되었다.");

    you.increase_duration(DUR_INFUSION,  8 + roll_dice(2, pow), 100);
    you.props["infusion_power"] = pow;

    return SPRET_SUCCESS;
}

spret_type cast_song_of_slaying(int pow, bool fail)
{
    fail_check();

    if (you.duration[DUR_SONG_OF_SLAYING])
        mpr("당신은 새 노래를 시작한다!");
    else
        mpr("당신은 살육의 노래를 부르기 시작한다.");

    you.set_duration(DUR_SONG_OF_SLAYING, 20 + random2avg(pow, 2));

    you.props[SONG_OF_SLAYING_KEY] = 0;
    return SPRET_SUCCESS;
}

spret_type cast_silence(int pow, bool fail)
{
    fail_check();
    mpr("심오한 침묵이 당신을 에워싼다.");

    you.increase_duration(DUR_SILENCE, 10 + pow/4 + random2avg(pow/2, 2), 100);
    invalidate_agrid(true);

    if (you.beheld())
        you.update_beholders();

    learned_something_new(HINT_YOU_SILENCE);
    return SPRET_SUCCESS;
}

spret_type cast_liquefaction(int pow, bool fail)
{
    fail_check();
    flash_view_delay(UA_PLAYER, BROWN, 80);
    flash_view_delay(UA_PLAYER, YELLOW, 80);
    flash_view_delay(UA_PLAYER, BROWN, 140);

    mpr("당신 주변의 땅이 액화한다!");

    you.increase_duration(DUR_LIQUEFYING, 10 + random2avg(pow, 2), 100);
    invalidate_agrid(true);
    return SPRET_SUCCESS;
}

spret_type cast_shroud_of_golubria(int pow, bool fail)
{
    fail_check();
    if (you.duration[DUR_SHROUD_OF_GOLUBRIA])
        mpr("당신은 장막의 지속시간을 갱신했다.");
    else
        mpr("당신의 몸을 감싸는 얇은 장막을 따라 공간이 미세하게 왜곡된다.");

    you.increase_duration(DUR_SHROUD_OF_GOLUBRIA, 7 + roll_dice(2, pow), 50);
    return SPRET_SUCCESS;
}

spret_type cast_transform(int pow, transformation which_trans, bool fail)
{
    if (!transform(pow, which_trans, false, true)
        || !check_form_stat_safety(which_trans))
    {
        return SPRET_ABORT;
    }

    fail_check();
    transform(pow, which_trans);
    return SPRET_SUCCESS;
}
