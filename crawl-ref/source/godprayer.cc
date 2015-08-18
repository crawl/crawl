#include "AppHdr.h"

#include "godprayer.h"

#include <cmath>

#include "artefact.h"
#include "butcher.h"
#include "coordit.h"
#include "database.h"
#include "english.h"
#include "env.h"
#include "fprop.h"
#include "godabil.h"
#include "goditem.h"
#include "godpassive.h"
#include "hiscores.h"
#include "invent.h"
#include "itemprop.h"
#include "items.h"
#include "item_use.h"
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

static bool _offer_items();

static bool _confirm_pray_sacrifice(god_type god)
{
    for (stack_iterator si(you.pos(), true); si; ++si)
    {
        bool penance = false;
        if (god_likes_item(god, *si)
            && needs_handle_warning(*si, OPER_PRAY, penance))
        {
            string prompt = "Really sacrifice stack with ";
            prompt += si->name(DESC_A);
            prompt += " in it?";
            if (penance)
                prompt += " This could place you under penance!";

            if (!yesno(prompt.c_str(), false, 'n'))
            {
                canned_msg(MSG_OK);
                return false;
            }
        }
    }

    return true;
}

string god_prayer_reaction()
{
    string result = uppercase_first(god_name(you.religion));
    if (crawl_state.player_is_dead())
        result += " was ";
    else
        result += " is ";
    result +=
        (you.piety >= piety_breakpoint(5)) ? "exalted by your worship" :
        (you.piety >= piety_breakpoint(4)) ? "extremely pleased with you" :
        (you.piety >= piety_breakpoint(3)) ? "greatly pleased with you" :
        (you.piety >= piety_breakpoint(2)) ? "most pleased with you" :
        (you.piety >= piety_breakpoint(1)) ? "pleased with you" :
        (you.piety >= piety_breakpoint(0)) ? "aware of your devotion"
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
    dungeon_terrain_changed(you.pos(), altar_for_god(god), false);
    return god;
}

static bool _pray_ecumenical_altar()
{
    if (yesno("This altar will convert you to a god. You cannot discern "
              "which. Do you pray?", false, 'n'))
    {
        {
            // Don't check for or charge a Gozag service fee.
            unwind_var<int> fakepoor(you.attribute[ATTR_GOLD_GENERATED], 0);

            god_type altar_god = _altar_identify_ecumenical_altar();
            mprf(MSGCH_GOD, "%s accepts your prayer!",
                            god_name(altar_god).c_str());
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
static bool _try_god_conversion(god_type god, bool beogh_priest)
{
    ASSERT(god != GOD_NO_GOD);

    if (you.species == SP_DEMIGOD)
    {
        mpr("A being of your status worships no god.");
        return false;
    }

    if (god == GOD_ECUMENICAL)
        return _pray_ecumenical_altar();

    if (you_worship(GOD_NO_GOD) || god != you.religion)
    {
        // consider conversion
        you.turn_is_over = true;
        // But if we don't convert then god_pitch
        // makes it not take a turn after all.
        god_pitch(god);
        if (you.turn_is_over && you_worship(GOD_BEOGH) && beogh_priest)
            spare_beogh_convert();
        return you.turn_is_over;
    }
    return false;
}

/**
 * Zazen.
 */
static void _zen_meditation()
{
    const mon_holy_type holi = you.holiness();
    mprf(MSGCH_PRAY,
         "You spend a moment contemplating the meaning of %s.",
         holi == MH_NONLIVING ? "existence" : holi == MH_UNDEAD ? "unlife" : "life");
}

/**
 * Pray. (To your god, or the god of the altar you're at, or to Beogh, if
 * you're an orc being preached at.)
 */
void pray(bool allow_conversion)
{
    const god_type altar_god = feat_altar_god(grd(you.pos()));
    const bool beogh_priest = env.level_state & LSTATE_BEOGH
        && can_convert_to_beogh();
    const god_type target_god = beogh_priest ? GOD_BEOGH : altar_god;

    // only successful prayer takes time
    you.turn_is_over = false;
    // Try to pray to an altar or beogh (if either is possible)
    if (allow_conversion
        && target_god != GOD_NO_GOD
        && you.religion != target_god)
    {
        if (_try_god_conversion(target_god, beogh_priest))
            return;
    }

    ASSERT(!you.turn_is_over);
    // didn't convert to anyone.
    if (you_worship(GOD_NO_GOD))
    {
        // wasn't considering following a god; just meditating.
        if (altar_god == GOD_NO_GOD)
            _zen_meditation();
        return;
    }

    mprf(MSGCH_PRAY, "You offer a %sprayer to %s.",
         you.cannot_speak() ? "silent " : "",
         god_name(you.religion).c_str());

    you.turn_is_over = _offer_items()
                      || (you_worship(GOD_FEDHAS) && fedhas_fungal_bloom());

    if (you_worship(GOD_XOM))
        mprf(MSGCH_GOD, "%s", getSpeakString("Xom prayer").c_str());
    else if (you_worship(GOD_GOZAG))
        mprf(MSGCH_GOD, "%s", getSpeakString("Gozag prayer").c_str());
    else if (player_under_penance())
        simple_god_message(" demands penance!");
    else
        mprf(MSGCH_PRAY, you.religion, "%s", god_prayer_reaction().c_str());

    dprf("piety: %d (-%d)", you.piety, you.piety_hysteresis);
}

int zin_tithe(const item_def& item, int quant, bool quiet, bool converting)
{
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

        if (item.plus == 1) // seen before worshipping Zin
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

/**
 * Sacrifice a scroll to Ashenzari, transforming it into three new curse
 * scrolls.
 *
 * The types of scrolls generated are random, weighted by the number of slots
 * of the appropriate type available to the player.
 *
 * @param item         The scroll to be sacrificed.
 *                     Is not destroyed by this function (obviously!)
 */
static void _ashenzari_sac_scroll(const item_def& item)
{
    mprf("%s flickers black.",
         get_desc_quantity(1, item.quantity,
                           item.name(DESC_THE)).c_str());

    const int wpn_weight = 3;
    const int jwl_weight = (you.species != SP_OCTOPODE) ? 3 : 9;
    int arm_weight = 0;
    for (int i = EQ_MIN_ARMOUR; i <= EQ_MAX_ARMOUR; i++)
        if (you_can_wear(static_cast<equipment_type>(i)))
            arm_weight++;

    map<int, int> generated_scrolls = {
        { SCR_CURSE_WEAPON, 0 },
        { SCR_CURSE_ARMOUR, 0 },
        { SCR_CURSE_JEWELLERY, 0 },
    };
    for (int i = 0; i < 3; i++)
    {
        const int scroll_type = you.species == SP_FELID ?
                        SCR_CURSE_JEWELLERY :
                        random_choose_weighted(wpn_weight, SCR_CURSE_WEAPON,
                                               arm_weight, SCR_CURSE_ARMOUR,
                                               jwl_weight, SCR_CURSE_JEWELLERY,
                                               0);
        generated_scrolls[scroll_type]++;
        dprf("%d: %d", scroll_type, generated_scrolls[scroll_type]);
    }

    vector<string> scroll_names;
    for (auto gen_scroll : generated_scrolls)
    {
        const int scroll_type = gen_scroll.first;
        const int num_generated = gen_scroll.second;
        if (!num_generated)
            continue;

        int it = items(false, OBJ_SCROLLS, scroll_type, 0);
        if (it == NON_ITEM)
        {
            mpr("You feel the world is against you.");
            return;
        }

        mitm[it].quantity = num_generated;
        scroll_names.push_back(mitm[it].name(DESC_A));

        if (!move_item_to_grid(&it, you.pos(), true))
            destroy_item(it, true); // can't happen
    }

    mprf("%s appear.", comma_separated_line(scroll_names.begin(),
                                            scroll_names.end()).c_str());
}

// God effects of sacrificing one item from a stack (e.g., a weapon, one
// out of 20 arrows, etc.). Does not modify the actual item in any way.
static piety_gain_t _sacrifice_one_item_noncount(const item_def& item,
       int *js, bool first)
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

    piety_gain_t relative_piety_gain = PIETY_NONE;
    switch (you.religion)
    {
    case GOD_BEOGH:
    {
        const int item_orig = item.orig_monnum;

        int chance = 4;

        if (item_orig == MONS_SAINT_ROKA)
            chance += 12;
        else if (item_orig == MONS_ORC_HIGH_PRIEST)
            chance += 8;
        else if (item_orig == MONS_ORC_PRIEST)
            chance += 4;

        if (item.sub_type == CORPSE_SKELETON)
            chance -= 2;

        gain_piety(chance, 20);
        if (x_chance_in_y(chance, 20))
            relative_piety_gain = PIETY_SOME;
        break;
    }

    case GOD_JIYVA:
    {
        // compress into range 0..250
        const int stepped = stepdown_value(value, 50, 50, 200, 250);
        gain_piety(stepped, 50);
        relative_piety_gain = (piety_gain_t)min(2, div_rand_round(stepped, 50));
        jiyva_slurp_bonus(div_rand_round(stepped, 50), js);
        break;
    }

    default:
        break;
    }

    return relative_piety_gain;
}

piety_gain_t sacrifice_item_stack(const item_def& item, int *js, int quantity)
{
    if (quantity <= 0)
        quantity = item.quantity;
    piety_gain_t relative_gain = PIETY_NONE;
    for (int j = 0; j < quantity; ++j)
    {
        const piety_gain_t gain = _sacrifice_one_item_noncount(item, js, !j);

        // Update piety gain if necessary.
        if (gain != PIETY_NONE)
        {
            if (relative_gain == PIETY_NONE)
                relative_gain = gain;
            else            // some + some = lots
                relative_gain = PIETY_LOTS;
        }
    }
    return relative_gain;
}

/**
 * Sacrifice the items at the player's location to the player's god.
 *
 * @return  True if an item was sacrificed, false otherwise.
*/
static bool _offer_items()
{
    if (!god_likes_items(you.religion))
        return false;

    if (!_confirm_pray_sacrifice(you.religion))
        return false;

    int i = you.visible_igrd(you.pos());

    god_acting gdact;

    int num_sacced = 0;
    int num_disliked = 0;

    while (i != NON_ITEM)
    {
        item_def &item(mitm[i]);
        const int next = item.link;  // in case we can't get it later.
        const bool disliked = !god_likes_item(you.religion, item);

        if (item_is_stationary_net(item) || disliked)
        {
            if (disliked)
                num_disliked++;
            i = next;
            continue;
        }

        // Ignore {!D} inscribed items.
        if (!check_warning_inscriptions(item, OPER_DESTROY))
        {
            mpr("Won't sacrifice {!D} inscribed item.");
            i = next;
            continue;
        }

        if (god_likes_item(you.religion, item)
            && ((item.inscription.find("=p") != string::npos)
                || item_needs_autopickup(item)
                    && GOD_ASHENZARI != you.religion))
        {
            const string msg = "Really sacrifice " + item.name(DESC_A) + "?";

            if (!yesno(msg.c_str(), false, 'n'))
            {
                i = next;
                continue;
            }
        }

        if (GOD_ASHENZARI == you.religion)
            _ashenzari_sac_scroll(item);
        else
        {
            const piety_gain_t relative_gain = sacrifice_item_stack(item);
            print_sacrifice_message(you.religion, mitm[i], relative_gain);
        }

        if (GOD_ASHENZARI == you.religion && item.quantity > 1)
            item.quantity -= 1;
        else
        {
            item_was_destroyed(mitm[i]);
            destroy_item(i);
        }

        i = next;
        num_sacced++;
    }

    // Explanatory messages if nothing the god likes is sacrificed.
    if (num_sacced == 0 && num_disliked > 0)
    {
        if (you_worship(GOD_BEOGH))
            simple_god_message(" only cares about orcish remains!");
        else if (you_worship(GOD_ASHENZARI))
            simple_god_message(" can corrupt only scrolls of remove curse.");
    }

    return num_sacced > 0;
}
