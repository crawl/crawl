#include "AppHdr.h"

#include "god-prayer.h"

#include <cmath>

#include "artefact.h"
#include "butcher.h"
#include "coordit.h"
#include "database.h"
#include "describe-god.h"
#include "english.h"
#include "env.h"
#include "food.h"
#include "fprop.h"
#include "god-abil.h"
#include "god-item.h"
#include "god-passive.h"
#include "hiscores.h"
#include "invent.h"
#include "item-prop.h"
#include "items.h"
#include "item-use.h"
#include "makeitem.h"
#include "message.h"
#include "notes.h"
#include "prompt.h"
#include "religion.h"
#include "shopping.h"
#include "spl-goditem.h"
#include "state.h"
#include "stepdown.h"
#include "stringutil.h"
#include "terrain.h"
#include "unwind.h"
#include "view.h"

string god_prayer_reaction()
{
    string result = uppercase_first(god_name(you.religion));
    const int rank = god_favour_rank(you.religion);
    if (crawl_state.player_is_dead())
        result += " was ";
    else
        result += " is ";
    result +=
        (rank == 7) ? "exalted by your worship" :
        (rank == 6) ? "extremely pleased with you" :
        (rank == 5) ? "greatly pleased with you" :
        (rank == 4) ? "most pleased with you" :
        (rank == 3) ? "pleased with you" :
        (rank == 2) ? "aware of your devotion"
                    : "noncommittal";
    result += ".";

    return result;
}

/**
 * Determine the god this game's ecumenical altar is for.
 * Replaces the ecumenical altar with the God's real altar.
 * Assumes you can worship at least one god (ie are not a
 * demigod), and that you're standing on the altar.
 *
 * @return The god this altar is for.
 */
static god_type _altar_identify_ecumenical_altar()
{
    god_type god;
    do
    {
        god = random_god();
    }
    while (!player_can_join_god(god));
    dungeon_terrain_changed(you.pos(), altar_for_god(god));
    return god;
}

static bool _pray_ecumenical_altar()
{
    if (yesno("You cannot tell which god this altar belongs to. Convert to "
              "them anyway?", false, 'n'))
    {
        {
            // Don't check for or charge a Gozag service fee.
            unwind_var<int> fakepoor(you.attribute[ATTR_GOLD_GENERATED], 0);

            god_type altar_god = _altar_identify_ecumenical_altar();
            mprf(MSGCH_GOD, "%s accepts your prayer!",
                            god_name(altar_god).c_str());
            you.turn_is_over = true;
            if (!you_worship(altar_god))
                join_religion(altar_god);
            else
                return true;
        }

        if (you_worship(GOD_RU))
            you.props[RU_SACRIFICE_PROGRESS_KEY] = 9999;
        else
            gain_piety(20, 1, false);

        mark_milestone("god.ecumenical", "prayed at an ecumenical altar.");
        return true;
    }
    else
    {
        canned_msg(MSG_OK);
        return false;
    }
}

/**
 * Attempt to convert to the given god.
 *
 * @return True if the conversion happened, false otherwise.
 */
void try_god_conversion(god_type god)
{
    ASSERT(god != GOD_NO_GOD);

    if (you.species == SP_DEMIGOD)
    {
        mpr("A being of your status worships no god.");
        return;
    }

    if (god == GOD_ECUMENICAL)
    {
        _pray_ecumenical_altar();
        return;
    }

    if (you_worship(GOD_NO_GOD) || god != you.religion)
    {
        // consider conversion
        you.turn_is_over = true;
        // But if we don't convert then god_pitch
        // makes it not take a turn after all.
        god_pitch(god);
    }
    else
    {
        // Already worshipping this god - just print a message.
        mprf(MSGCH_GOD, "You offer a %sprayer to %s.",
             you.cannot_speak() ? "silent " : "",
             god_name(god).c_str());
    }
}

int zin_tithe(const item_def& item, int quant, bool quiet, bool converting)
{
    if (item.tithe_state == TS_NO_TITHE)
        return 0;

    int taken = 0;
    int due = quant += you.attribute[ATTR_TITHE_BASE];
    if (due > 0)
    {
        int tithe = due / 10;
        due -= tithe * 10;
        // Those high enough in the hierarchy get to reap the benefits.
        // You're never big enough to be paid, the top is not having to pay
        // (and even that at 200 piety, for a brief moment until it decays).
        tithe = min(tithe,
                    (you.penance[GOD_ZIN] + MAX_PIETY - you.piety) * 2 / 3);
        if (tithe <= 0)
        {
            // update the remainder anyway
            you.attribute[ATTR_TITHE_BASE] = due;
            return 0;
        }
        taken = tithe;
        you.attribute[ATTR_DONATIONS] += tithe;
        mprf("You pay a tithe of %d gold.", tithe);

        if (item.tithe_state == TS_NO_PIETY) // seen before worshipping Zin
        {
            tithe = 0;
            simple_god_message(" ignores your late donation.");
        }
        // A single scroll can give you more than D:1-18, Lair and Orc
        // together, limit the gains. You're still required to pay from
        // your sudden fortune, yet it's considered your duty to the Church
        // so piety is capped. If you want more piety, donate more!
        //
        // Note that the stepdown is not applied to other gains: it would
        // be simpler, yet when a monster combines a number of gold piles
        // you shouldn't be penalized.
        int denom = 2;
        if (item.props.exists(ACQUIRE_KEY)) // including "acquire any" in vaults
        {
            tithe = stepdown_value(tithe, 10, 10, 50, 50);
            dprf("Gold was acquired, reducing gains to %d.", tithe);
        }
        else
        {
            if (player_in_branch(BRANCH_ORC) && !converting)
            {
                // Another special case: Orc gives simply too much compared to
                // other branches.
                denom *= 2;
            }
            // Avg gold pile value: 10 + depth/2.
            tithe *= 47;
            denom *= 20 + env.absdepth0;
        }
        gain_piety(tithe * 3, denom);
    }
    you.attribute[ATTR_TITHE_BASE] = due;
    return taken;
}

enum class jiyva_slurp_result
{
    none = 0,
    food = 1 << 0,
    hp   = 1 << 1,
    mp   = 1 << 2,
};
DEF_BITFIELD(jiyva_slurp_results, jiyva_slurp_result);

struct slurp_gain
{
    jiyva_slurp_results jiyva_bonus;
    piety_gain_t piety_gain;
};

// God effects of sacrificing one item from a stack (e.g., a weapon, one
// out of 20 arrows, etc.). Does not modify the actual item in any way.
static slurp_gain _sacrifice_one_item_noncount(const item_def& item)
{
    // item_value() multiplies by quantity.
    const int shop_value = item_value(item, true) / item.quantity;
    // Since the god is taking the items as a sacrifice, they must have at
    // least minimal value, otherwise they wouldn't be taken.
    const int value = (item.base_type == OBJ_CORPSES ?
                          50 * stepdown_value(max(1,
                          max_corpse_chunks(item.mon_type)), 4, 4, 12, 12) :
                      (is_worthless_consumable(item) ? 1 : shop_value));

#if defined(DEBUG_DIAGNOSTICS) || defined(DEBUG_SACRIFICE)
        mprf(MSGCH_DIAGNOSTICS, "Sacrifice item value: %d", value);
#endif

    slurp_gain gain { jiyva_slurp_result::none, PIETY_NONE };

    // compress into range 0..250
    const int stepped = stepdown_value(value, 50, 50, 200, 250);
    gain_piety(stepped, 50);
    gain.piety_gain = (piety_gain_t)min(2, div_rand_round(stepped, 50));

    if (player_under_penance(GOD_JIYVA))
        return gain;

    int item_value = div_rand_round(stepped, 50);
    if (have_passive(passive_t::slime_feed)
        && x_chance_in_y(you.piety, MAX_PIETY)
        && !you_foodless())
    {
        //same as a sultana
        lessen_hunger(70, true);
        gain.jiyva_bonus |= jiyva_slurp_result::food;
    }

    if (have_passive(passive_t::slime_mp)
        && x_chance_in_y(you.piety, MAX_PIETY)
        && you.magic_points < you.max_magic_points)
    {
        inc_mp(max(random2(item_value), 1));
        gain.jiyva_bonus |= jiyva_slurp_result::mp;
    }

    if (have_passive(passive_t::slime_hp)
        && x_chance_in_y(you.piety, MAX_PIETY)
        && you.hp < you.hp_max
        && !you.duration[DUR_DEATHS_DOOR])
    {
        inc_hp(max(random2(item_value), 1));
        gain.jiyva_bonus |= jiyva_slurp_result::hp;
    }

    return gain;
}

void jiyva_slurp_item_stack(const item_def& item, int quantity)
{
    ASSERT(you_worship(GOD_JIYVA));
    if (quantity <= 0)
        quantity = item.quantity;
    slurp_gain gain { jiyva_slurp_result::none, PIETY_NONE };
    for (int j = 0; j < quantity; ++j)
    {
        const slurp_gain new_gain = _sacrifice_one_item_noncount(item);

        gain.piety_gain = max(gain.piety_gain, new_gain.piety_gain);
        gain.jiyva_bonus |= new_gain.jiyva_bonus;
    }

    if (gain.piety_gain > PIETY_NONE)
        simple_god_message(" appreciates your sacrifice.");
    if (gain.jiyva_bonus & jiyva_slurp_result::food)
        mpr("You feel a little less hungry.");
    if (gain.jiyva_bonus & jiyva_slurp_result::mp)
        canned_msg(MSG_GAIN_MAGIC);
    if (gain.jiyva_bonus & jiyva_slurp_result::hp)
        canned_msg(MSG_GAIN_HEALTH);
}
