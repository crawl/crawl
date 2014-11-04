#include "AppHdr.h"

#include "godprayer.h"

#include <cmath>

#include "artefact.h"
#include "bloodspatter.h"
#include "butcher.h"
#include "coordit.h"
#include "database.h"
#include "effects.h"
#include "env.h"
#include "fprop.h"
#include "godabil.h"
#include "goditem.h"
#include "godpassive.h"
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
#include "spl-wpnench.h"
#include "state.h"
#include "stepdown.h"
#include "stringutil.h"
#include "terrain.h"
#include "unwind.h"
#include "view.h"

static bool _offer_items();
static bool _zin_donate_gold();

static bool _confirm_pray_sacrifice(god_type god)
{
    for (stack_iterator si(you.pos(), true); si; ++si)
    {
        if (god_likes_item(god, *si)
            && needs_handle_warning(*si, OPER_PRAY))
        {
            string prompt = "Really sacrifice stack with ";
            prompt += si->name(DESC_A);
            prompt += " in it?";

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

static bool _bless_weapon(god_type god, brand_type brand, colour_t colour)
{
    int item_slot = prompt_invent_item("Brand which weapon?", MT_INVLIST,
                                       OSEL_BLESSABLE_WEAPON, true, true, false);

    if (item_slot == PROMPT_NOTHING || item_slot == PROMPT_ABORT)
        return false;

    item_def& wpn(you.inv[item_slot]);

    // Only TSO allows blessing ranged weapons.
    if (!is_brandable_weapon(wpn, brand == SPWPN_HOLY_WRATH, true))
        return false;

    string prompt = "Do you wish to have " + wpn.name(DESC_YOUR)
                       + " ";
    if (brand == SPWPN_PAIN)
        prompt += "bloodied with pain";
    else if (brand == SPWPN_DISTORTION)
        prompt += "corrupted";
    else
        prompt += "blessed";
    prompt += "?";

    if (!yesno(prompt.c_str(), true, 'n'))
    {
        canned_msg(MSG_OK);
        return false;
    }

    if (you.duration[DUR_WEAPON_BRAND]) // just in case
    {
        ASSERT(you.weapon());
        end_weapon_brand(*you.weapon());
    }

    string old_name = wpn.name(DESC_A);
    set_equip_desc(wpn, ISFLAG_GLOWING);
    set_item_ego_type(wpn, OBJ_WEAPONS, brand);

    const bool is_cursed = wpn.cursed();

    enchant_weapon(wpn, true);
    enchant_weapon(wpn, true);

    if (is_cursed)
        do_uncurse_item(wpn, false);

    if (god == GOD_SHINING_ONE)
    {
        convert2good(wpn);

        if (is_blessed_convertible(wpn))
            origin_acquired(wpn, GOD_SHINING_ONE);
    }
    else if (is_evil_god(god))
        convert2bad(wpn);

    you.wield_change = true;
    you.one_time_ability_used.set(god);
    calc_mp(); // in case the old brand was antimagic,
    you.redraw_armour_class = true; // protection,
    you.redraw_evasion = true;      // or evasion
    string desc  = old_name + " ";
            desc += (god == GOD_SHINING_ONE   ? "blessed by the Shining One" :
                     god == GOD_LUGONU        ? "corrupted by Lugonu" :
                     god == GOD_KIKUBAAQUDGHA ? "bloodied by Kikubaaqudgha"
                                              : "touched by the gods");

    take_note(Note(NOTE_ID_ITEM, 0, 0,
              wpn.name(DESC_A).c_str(), desc.c_str()));
    wpn.flags |= ISFLAG_NOTED_ID;
    wpn.props[FORCED_ITEM_COLOUR_KEY] = colour;

    mprf(MSGCH_GOD, "Your %s shines brightly!", wpn.name(DESC_QUALNAME).c_str());

    flash_view(UA_PLAYER, colour);

    simple_god_message(" booms: Use this gift wisely!");

    if (god == GOD_SHINING_ONE)
    {
        holy_word(100, HOLY_WORD_TSO, you.pos(), true);

        // Un-bloodify surrounding squares.
        for (radius_iterator ri(you.pos(), 3, C_ROUND, LOS_SOLID); ri; ++ri)
            if (is_bloodcovered(*ri))
                env.pgrid(*ri) &= ~FPROP_BLOODY;
    }

    if (god == GOD_KIKUBAAQUDGHA)
    {
        you.gift_timeout = 1; // no protection during pain branding weapon

        torment(&you, TORMENT_KIKUBAAQUDGHA, you.pos());

        you.gift_timeout = 0; // protection after pain branding weapon

        // Bloodify surrounding squares (75% chance).
        for (radius_iterator ri(you.pos(), 2, C_ROUND, LOS_SOLID); ri; ++ri)
            if (!one_chance_in(4))
                maybe_bloodify_square(*ri);
    }

#ifndef USE_TILE_LOCAL
    // Allow extra time for the flash to linger.
    delay(1000);
#endif

    return true;
}

/** Would a god currently allow using a one-time six-star ability?
 * Does not check whether the god actually grants such an ability.
 */
bool can_do_capstone_ability(god_type god)
{
   return in_good_standing(god, 5) && !you.one_time_ability_used[god];
}

static bool _altar_prayer()
{
    // Different message from when first joining a religion.
    mpr("You prostrate yourself in front of the altar and pray.");

    god_acting gdact;

    // donate gold to gain piety distributed over time
    if (you_worship(GOD_ZIN))
        return _zin_donate_gold();

    // TSO blesses weapons with holy wrath, and demon weapons specially.
    else if (can_do_capstone_ability(GOD_SHINING_ONE))
    {
        simple_god_message(" will bless one of your weapons.");
        more();
        return _bless_weapon(GOD_SHINING_ONE, SPWPN_HOLY_WRATH, YELLOW);
    }

    // Lugonu blesses weapons with distortion.
    else if (can_do_capstone_ability(GOD_LUGONU))
    {
        simple_god_message(" will brand one of your weapons with the corruption of the Abyss.");
        more();
        return _bless_weapon(GOD_LUGONU, SPWPN_DISTORTION, MAGENTA);
    }

    // Kikubaaqudgha blesses weapons with pain, or gives you a Necronomicon.
    else if (can_do_capstone_ability(GOD_KIKUBAAQUDGHA))
    {
        if (you.species != SP_FELID)
        {
            simple_god_message(
                " will bloody your weapon with pain or grant you the Necronomicon.");

            more();

            if (_bless_weapon(GOD_KIKUBAAQUDGHA, SPWPN_PAIN, RED))
                return true;

            // If not, ask if the player wants a Necronomicon.
            if (!yesno("Do you wish to receive the Necronomicon?", true, 'n'))
            {
                canned_msg(MSG_OK);
                return false;
            }
        }

        int thing_created = items(true, OBJ_BOOKS, BOOK_NECRONOMICON, 1, 0,
                                  you.religion);

        if (thing_created == NON_ITEM || !move_item_to_grid(&thing_created, you.pos()))
            return false;

        simple_god_message(" grants you a gift!");
        more();

        you.one_time_ability_used.set(you.religion);
        take_note(Note(NOTE_GOD_GIFT, you.religion));

        return true;
    }

    else if (in_good_standing(GOD_GOZAG)
             && !you.one_time_ability_used[GOD_GOZAG])
    {
        bool found = false;
        bool prompted = false;
        for (stack_iterator j(you.pos()); j; ++j)
        {
            if (is_artefact(*j)
                || item_is_orb(*j)
                || item_is_rune(*j)
                || j->base_type == OBJ_GOLD)
            {
                found = true;
                continue;
            }

            string message = "";
            {
                unwind_var<short int> old_quant = j->quantity;
                j->quantity = 1;

                prompted = true;

                string prompt =
                    make_stringf("Do you wish to duplicate %s?",
                                 j->name(old_quant.original_value() > 1
                                         ? DESC_A : DESC_THE).c_str());

                if (!yesno(prompt.c_str(), true, 'n'))
                    continue;

                message = " duplicates " + j->name(DESC_YOUR) + "!";
            }
            if (!copy_item_to_grid(*j, you.pos(), 1))
            {
                mprf("Something went wrong!");
                return false;
            }

            simple_god_message(message.c_str());
            you.one_time_ability_used.set(you.religion);
            take_note(Note(NOTE_ID_ITEM, 0, 0,
                      j->name(DESC_A).c_str(), "duplicated by Gozag"));
            return true;
        }
        if (prompted)
            canned_msg(MSG_OK);
        else if (found)
            mprf("There's no item here that can be duplicated.");
        return false;
    }

    // None of above are true, nothing happens.
    return false;
}

/**
 * Pray at, or convert to, an altar at the current position.
 *
 * @return Whether anything happened that took time.
 */
static bool _altar_pray_or_convert()
{
    const god_type altar_god = feat_altar_god(grd(you.pos()));
    if (altar_god == GOD_NO_GOD)
        return false;

    if (you.species == SP_DEMIGOD)
    {
        mpr("A being of your status worships no god.");
        return false;
    }

    if (you_worship(GOD_NO_GOD) || altar_god != you.religion)
    {
        // consider conversion
        you.turn_is_over = true;
        // But if we don't convert then god_pitch
        // makes it not take a turn after all.
        god_pitch(feat_altar_god(grd(you.pos())));
        return you.turn_is_over;
    }

    // pray to your own god's altar
    return _altar_prayer();
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
void pray(bool allow_altar_prayer)
{
    // only successful prayer takes time
    you.turn_is_over = false;

    // try to pray to an altar (if any is present)
    if (allow_altar_prayer && _altar_pray_or_convert())
    {
        you.turn_is_over = true;
        return;
    }

    // convert to beogh via priest.
    if (allow_altar_prayer && you_worship(GOD_NO_GOD)
        && (env.level_state & LSTATE_BEOGH) && can_convert_to_beogh())
    {
        // TODO: deduplicate this with the code in _altar_pray_or_convert.
        you.turn_is_over = true;
        // But if we don't convert then god_pitch
        // makes it not take a turn after all.
        god_pitch(GOD_BEOGH);
        if (you_worship(GOD_BEOGH))
        {
            spare_beogh_convert();
            return;
        }
    }

    ASSERT(!you.turn_is_over);

    // didn't convert to anyone.
    if (you_worship(GOD_NO_GOD))
    {
        // wasn't considering following a god; just meditating.
        if (feat_altar_god(grd(you.pos())) == GOD_NO_GOD)
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
        // together, limit the gains.  You're still required to pay from
        // your sudden fortune, yet it's considered your duty to the Church
        // so piety is capped.  If you want more piety, donate more!
        //
        // Note that the stepdown is not applied to other gains: it would
        // be simpler, yet when a monster combines a number of gold piles
        // you shouldn't be penalized.
        int denom = 2;
        if (item.props.exists("acquired")) // including "acquire any" in vaults
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

static int _gold_to_donation(int gold)
{
    return static_cast<int>((gold * log((float)gold)) / MAX_PIETY);
}

static bool _zin_donate_gold()
{
    if (you.gold == 0)
    {
        mpr("You don't have anything to sacrifice.");
        return false;
    }

    if (!yesno("Do you wish to donate half of your money?", true, 'n'))
    {
        canned_msg(MSG_OK);
        return false;
    }

    const int donation_cost = (you.gold / 2) + 1;
    const int donation = _gold_to_donation(donation_cost);

#if defined(DEBUG_DIAGNOSTICS) || defined(DEBUG_SACRIFICE) || defined(DEBUG_PIETY)
    mprf(MSGCH_DIAGNOSTICS, "A donation of $%d amounts to an "
         "increase of piety by %d.", donation_cost, donation);
#endif
    // Take a note of the donation.
    take_note(Note(NOTE_DONATE_MONEY, donation_cost));

    you.attribute[ATTR_DONATIONS] += donation_cost;

    you.del_gold(donation_cost);

    if (donation < 1)
    {
        simple_god_message(" finds your generosity lacking.");
        return false;
    }

    you.duration[DUR_PIETY_POOL] += donation;
    if (you.duration[DUR_PIETY_POOL] > 30000)
        you.duration[DUR_PIETY_POOL] = 30000;

    const int estimated_piety =
        min(MAX_PENANCE + MAX_PIETY, you.piety + you.duration[DUR_PIETY_POOL]);

    if (player_under_penance())
    {
        if (estimated_piety >= you.penance[GOD_ZIN])
            mpr("You feel that you will soon be absolved of all your sins.");
        else
            mpr("You feel that your burden of sins will soon be lighter.");
    }
    else
    {
        string result = "You feel that " + god_name(GOD_ZIN) + " will soon be ";
        result +=
            (estimated_piety >= piety_breakpoint(5)) ? "exalted by your worship" :
            (estimated_piety >= piety_breakpoint(4)) ? "extremely pleased with you" :
            (estimated_piety >= piety_breakpoint(3)) ? "greatly pleased with you" :
            (estimated_piety >= piety_breakpoint(2)) ? "most pleased with you" :
            (estimated_piety >= piety_breakpoint(1)) ? "pleased with you" :
            (estimated_piety >= piety_breakpoint(0)) ? "aware of your devotion"
                                                     : "noncommittal";
        result += (donation >= 30 && you.piety < piety_breakpoint(5)) ? "!" : ".";

        mpr(result.c_str());
    }

    zin_recite_interrupt();
    return true;
}

static void _ashenzari_sac_scroll(const item_def& item)
{
    int scr = SCR_CURSE_JEWELLERY;
    int num = 3;

    int wpn = 3;
    int arm = 0;
    int jwl = (you.species != SP_OCTOPODE) ? 3 : 9;

    for (int i = EQ_MIN_ARMOUR; i <= EQ_MAX_ARMOUR; i++)
        if (you_can_wear(i, true))
            arm++;

    do
    {
        if (you.species != SP_FELID)
        {
            scr = random_choose_weighted(wpn, SCR_CURSE_WEAPON,
                                         arm, SCR_CURSE_ARMOUR,
                                         jwl, SCR_CURSE_JEWELLERY,
                                         0);
        }
        int it = items(false, OBJ_SCROLLS, scr, 0, 0, GOD_ASHENZARI);
        if (it == NON_ITEM)
        {
            mpr("You feel the world is against you.");
            return;
        }

        mitm[it].quantity = 1;
        if (!move_item_to_grid(&it, you.pos(), true))
            destroy_item(it, true); // can't happen
    }
    while (--num > 0);
}

// Is the destroyed weapon valuable enough to gain piety by doing so?
// Unholy and evil weapons are handled specially.
static bool _destroyed_valuable_weapon(int value, int type)
{
    // value/500 chance of piety normally
    if (value > random2(500))
        return true;

    // But all non-missiles are acceptable if you've never reached *.
    if (you.piety_max[GOD_ELYVILON] < piety_breakpoint(0)
        && type != OBJ_MISSILES)
    {
        return true;
    }

    return false;
}

static piety_gain_t _sac_corpse(const item_def& item)
{
    if (you_worship(GOD_OKAWARU))
    {
        monster dummy;
        dummy.type = (monster_type)(item.orig_monnum ? item.orig_monnum
                                                     : item.plus);
        if (item.props.exists(MONSTER_NUMBER))
            dummy.number   = item.props[MONSTER_NUMBER].get_short();
        define_monster(&dummy);

        // Hit dice are overridden by define_monster, so only set them now.
        if (item.props.exists(MONSTER_HIT_DICE))
        {
            int hd = item.props[MONSTER_HIT_DICE].get_short();
            const monsterentry *m = get_monster_data(dummy.type);
            int hp = hit_points(hd, m->hpdice[1], m->hpdice[2]) + m->hpdice[3];

            dummy.set_hit_dice(hd);
            dummy.max_hit_points = hp;
        }
        int gain = get_fuzzied_monster_difficulty(&dummy);
        dprf("fuzzied corpse difficulty: %4.2f", gain*0.01);

        // Shouldn't be needed, but just in case an XL:1 spriggan diver walks
        // into a minotaur corpses vault on D:10 ...
        if (item.props.exists("cap_sacrifice"))
            gain = min(gain, 700 * 3);

        gain_piety(gain, 700);
        gain = div_rand_round(gain, 700);
        return (gain <= 0) ? PIETY_NONE : (gain < 4) ? PIETY_SOME : PIETY_LOTS;
    }

    gain_piety(13, 19);

    // The feedback is not accurate any longer on purpose; it only reveals
    // the rate you get piety at.
    return x_chance_in_y(13, 19) ? PIETY_SOME : PIETY_NONE;
}

// God effects of sacrificing one item from a stack (e.g., a weapon, one
// out of 20 arrows, etc.).  Does not modify the actual item in any way.
static piety_gain_t _sacrifice_one_item_noncount(const item_def& item,
       int *js, bool first)
{
    // XXX: this assumes that there's no overlap between
    //      item-accepting gods and corpse-accepting gods.
    if (god_likes_fresh_corpses(you.religion))
        return _sac_corpse(item);

    // item_value() multiplies by quantity.
    const int shop_value = item_value(item, true) / item.quantity;
    // Since the god is taking the items as a sacrifice, they must have at
    // least minimal value, otherwise they wouldn't be taken.
    const int value = (item.base_type == OBJ_CORPSES ?
                          50 * stepdown_value(max(1,
                          get_max_corpse_chunks(item.mon_type)), 4, 4, 12, 12) :
                      (is_worthless_consumable(item) ? 1 : shop_value));

#if defined(DEBUG_DIAGNOSTICS) || defined(DEBUG_SACRIFICE)
        mprf(MSGCH_DIAGNOSTICS, "Sacrifice item value: %d", value);
#endif

    piety_gain_t relative_piety_gain = PIETY_NONE;
    switch (you.religion)
    {
    case GOD_ELYVILON:
    {
        const bool valuable_weapon =
            _destroyed_valuable_weapon(value, item.base_type);
        const bool unholy_weapon = is_unholy_item(item);
        const bool evil_weapon = is_evil_item(item);

        if (valuable_weapon || unholy_weapon || evil_weapon)
        {
            if (unholy_weapon || evil_weapon)
            {
                const char *desc_weapon = evil_weapon ? "evil" : "unholy";

                // Print this in addition to the above!
                if (first)
                {
                    simple_god_message(make_stringf(
                             " welcomes the destruction of %s %s weapon%s.",
                             item.quantity == 1 ? "this" : "these",
                             desc_weapon,
                             item.quantity == 1 ? ""     : "s").c_str(),
                             GOD_ELYVILON);
                }
            }

            gain_piety(1);
            relative_piety_gain = PIETY_SOME;
        }
        break;
    }

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

    case GOD_ASHENZARI:
        _ashenzari_sac_scroll(item);
        break;

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
                || item_needs_autopickup(item)))
        {
            const string msg = "Really sacrifice " + item.name(DESC_A) + "?";

            if (!yesno(msg.c_str(), false, 'n'))
            {
                i = next;
                continue;
            }
        }

        piety_gain_t relative_gain = sacrifice_item_stack(item);
        print_sacrifice_message(you.religion, mitm[i], relative_gain);
        item_was_destroyed(mitm[i]);
        destroy_item(i);
        i = next;
        num_sacced++;
    }

    // Explanatory messages if nothing the god likes is sacrificed.
    if (num_sacced == 0 && num_disliked > 0)
    {
        if (god_likes_fresh_corpses(you.religion))
            simple_god_message(" only cares about fresh corpses!");
        else if (you_worship(GOD_BEOGH))
            simple_god_message(" only cares about orcish remains!");
        else if (you_worship(GOD_ASHENZARI))
            simple_god_message(" can corrupt only scrolls of remove curse.");
    }
    if (num_sacced == 0 && you_worship(GOD_ELYVILON))
    {
        mprf("There are no %sweapons here to destroy!",
             you.piety_max[GOD_ELYVILON] < piety_breakpoint(2) ? "" : "unholy or evil ");
    }

    return num_sacced > 0;
}
