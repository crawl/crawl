#include "AppHdr.h"

#include "godprayer.h"

#include <cmath>

#include "areas.h"
#include "artefact.h"
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
#include "libutil.h"
#include "player.h"
#include "makeitem.h"
#include "message.h"
#include "misc.h"
#include "monster.h"
#include "notes.h"
#include "options.h"
#include "random.h"
#include "religion.h"
#include "skills2.h"
#include "stash.h"
#include "state.h"
#include "stuff.h"
#include "terrain.h"
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
    string result = god_name(you.religion);
    if (crawl_state.player_is_dead())
        result += " was ";
    else
        result += " is ";
    result +=
        (you.piety > 130) ? "exalted by your worship" :
        (you.piety > 100) ? "extremely pleased with you" :
        (you.piety >  70) ? "greatly pleased with you" :
        (you.piety >  40) ? "most pleased with you" :
        (you.piety >  20) ? "pleased with you" :
        (you.piety >   5) ? "noncommittal"
                          : "displeased";
    result += ".";

    return result;
}

static bool _bless_weapon(god_type god, brand_type brand, int colour)
{
    item_def& wpn = *you.weapon();

    // Only TSO allows blessing ranged weapons.
    if (!is_brandable_weapon(wpn, brand == SPWPN_HOLY_WRATH))
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

    you.duration[DUR_WEAPON_BRAND] = 0;     // just in case

    string old_name = wpn.name(DESC_A);
    set_equip_desc(wpn, ISFLAG_GLOWING);
    set_item_ego_type(wpn, OBJ_WEAPONS, brand);
    wpn.colour = colour;

    const bool is_cursed = wpn.cursed();

    enchant_weapon(wpn, 1 + random2(2), 1 + random2(2), 0);

    if (is_cursed)
        do_uncurse_item(wpn, false);

    if (god == GOD_SHINING_ONE)
    {
        convert2good(wpn);

        if (is_blessed_convertible(wpn))
        {
            origin_acquired(wpn, GOD_SHINING_ONE);
            wpn.flags |= ISFLAG_BLESSED_WEAPON;
        }

        burden_change();
    }
    else if (is_evil_god(god))
    {
        convert2bad(wpn);

        wpn.flags &= ~ISFLAG_BLESSED_WEAPON;

        burden_change();
    }

    you.wield_change = true;
    you.one_time_ability_used.set(god);
    calc_mp(); // in case the old brand was antimagic
    string desc  = old_name + " ";
            desc += (god == GOD_SHINING_ONE   ? "blessed by the Shining One" :
                     god == GOD_LUGONU        ? "corrupted by Lugonu" :
                     god == GOD_KIKUBAAQUDGHA ? "bloodied by Kikubaaqudgha"
                                              : "touched by the gods");

    take_note(Note(NOTE_ID_ITEM, 0, 0,
              wpn.name(DESC_A).c_str(), desc.c_str()));
    wpn.flags |= ISFLAG_NOTED_ID;

    mpr("Your weapon shines brightly!", MSGCH_GOD);

    flash_view(colour);

    simple_god_message(" booms: Use this gift wisely!");

    if (god == GOD_SHINING_ONE)
    {
        holy_word(100, HOLY_WORD_TSO, you.pos(), true);

        // Un-bloodify surrounding squares.
        for (radius_iterator ri(you.pos(), 3, true, true); ri; ++ri)
            if (is_bloodcovered(*ri))
                env.pgrid(*ri) &= ~(FPROP_BLOODY);
    }

    if (god == GOD_KIKUBAAQUDGHA)
    {
        you.gift_timeout = 1; // no protection during pain branding weapon

        torment(&you, TORMENT_KIKUBAAQUDGHA, you.pos());

        you.gift_timeout = 0; // protection after pain branding weapon

        // Bloodify surrounding squares (75% chance).
        for (radius_iterator ri(you.pos(), 2, true, true); ri; ++ri)
            if (!one_chance_in(4))
                maybe_bloodify_square(*ri);
    }

#ifndef USE_TILE_LOCAL
    // Allow extra time for the flash to linger.
    delay(1000);
#endif

    return true;
}

// Prayer at your god's altar.
static bool _altar_prayer()
{
    // Different message from when first joining a religion.
    mpr("You prostrate yourself in front of the altar and pray.");

    god_acting gdact;

    bool did_something = false;

    // donate gold to gain piety distributed over time
    if (you_worship(GOD_ZIN))
        did_something = _zin_donate_gold();

    // TSO blesses weapons with holy wrath, and long blades and demon
    // whips specially.
    if (you_worship(GOD_SHINING_ONE)
        && !you.one_time_ability_used[GOD_SHINING_ONE]
        && !player_under_penance()
        && you.piety > 160)
    {
        item_def *wpn = you.weapon();

        if (wpn
            && (get_weapon_brand(*wpn) != SPWPN_HOLY_WRATH
                || is_blessed_convertible(*wpn)))
        {
            did_something = _bless_weapon(GOD_SHINING_ONE, SPWPN_HOLY_WRATH,
                                          YELLOW);
        }
    }

    // Lugonu blesses weapons with distortion.
    if (you_worship(GOD_LUGONU)
        && !you.one_time_ability_used[GOD_LUGONU]
        && !player_under_penance()
        && you.piety > 160)
    {
        item_def *wpn = you.weapon();

        if (wpn && get_weapon_brand(*wpn) != SPWPN_DISTORTION)
            did_something = _bless_weapon(GOD_LUGONU, SPWPN_DISTORTION, MAGENTA);
    }

    // Kikubaaqudgha blesses weapons with pain, or gives you a Necronomicon.
    if (you_worship(GOD_KIKUBAAQUDGHA)
        && !you.one_time_ability_used[GOD_KIKUBAAQUDGHA]
        && !player_under_penance()
        && you.piety > 160)
    {
        if (you.species != SP_FELID)
        {
            simple_god_message(
                " will bloody your weapon with pain or grant you the Necronomicon.");

            item_def *wpn = you.weapon();

            // Does the player want a pain branding?
            if (wpn && get_weapon_brand(*wpn) != SPWPN_PAIN)
            {
                if (_bless_weapon(GOD_KIKUBAAQUDGHA, SPWPN_PAIN, RED))
                    return true;
            }
            else
                mpr("You have no weapon to bloody with pain.");

            // If not, ask if the player wants a Necronomicon.
            if (!yesno("Do you wish to receive the Necronomicon?", true, 'n'))
            {
                canned_msg(MSG_OK);
                return false;
            }
        }

        int thing_created = items(1, OBJ_BOOKS, BOOK_NECRONOMICON, true, 1,
                                  MAKE_ITEM_RANDOM_RACE,
                                  0, 0, you.religion);

        if (thing_created == NON_ITEM)
            return false;

        move_item_to_grid(&thing_created, you.pos());

        if (thing_created != NON_ITEM)
        {
            simple_god_message(" grants you a gift!");
            more();

            you.one_time_ability_used.set(you.religion);
            did_something = true;
            take_note(Note(NOTE_GOD_GIFT, you.religion));
        }

        // Return early so we don't offer our Necronomicon to Kiku.
        return true;
    }

    return did_something;
}

void pray()
{
    if (you.cannot_speak())
    {
        mpr("You are unable to make a sound!");
        return;
    }

    // only successful prayer takes time
    you.turn_is_over = false;

    bool something_happened = false;
    const god_type altar_god = feat_altar_god(grd(you.pos()));
    if (altar_god != GOD_NO_GOD)
    {
        if (!you_worship(GOD_NO_GOD) && altar_god == you.religion)
            something_happened = _altar_prayer();
        else if (altar_god != GOD_NO_GOD)
        {
            if (you.species == SP_DEMIGOD)
            {
                mpr("A being of your status worships no god.");
                return;
            }

            you.turn_is_over = true;
            // But if we don't convert then god_pitch
            // makes it not take a turn after all.
            god_pitch(feat_altar_god(grd(you.pos())));
            return;
        }
    }

    if (you_worship(GOD_NO_GOD))
    {
        const mon_holy_type holi = you.holiness();

        mprf(MSGCH_PRAY,
             "You spend a moment contemplating the meaning of %s.",
             holi == MH_NONLIVING ? "existence" : holi == MH_UNDEAD ? "unlife" : "life");

        // Zen meditation is timeless.
        return;
    }

    mprf(MSGCH_PRAY, "You offer a prayer to %s.",
         god_name(you.religion).c_str());

    if (you_worship(GOD_FEDHAS) && fedhas_fungal_bloom())
        something_happened = true;

    // All sacrifices affect items you're standing on.
    something_happened |= _offer_items();

    if (you_worship(GOD_XOM))
        mpr(getSpeakString("Xom prayer"), MSGCH_GOD);
    else if (player_under_penance())
        simple_god_message(" demands penance!");
    else
        mpr(god_prayer_reaction().c_str(), MSGCH_PRAY, you.religion);

    if (something_happened)
        you.turn_is_over = true;
    dprf("piety: %d (-%d)", you.piety, you.piety_hysteresis);
}

int zin_tithe(item_def& item, int quant, bool quiet, bool converting)
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
            simple_god_message(" is a bit unhappy you did not bring this "
                               "gold earlier.");
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
            if (player_in_branch(BRANCH_ORCISH_MINES) && !converting)
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
            (estimated_piety > 130) ? "exalted by your worship" :
            (estimated_piety > 100) ? "extremely pleased with you" :
            (estimated_piety >  70) ? "greatly pleased with you" :
            (estimated_piety >  40) ? "most pleased with you" :
            (estimated_piety >  20) ? "pleased with you" :
            (estimated_piety >   5) ? "noncommittal"
                                    : "displeased";
        result += (donation >= 30 && you.piety <= 170) ? "!" : ".";

        mpr(result.c_str());
    }

    return true;
}

static int _leading_sacrifice_group()
{
    int weights[5];
    get_pure_deck_weights(weights);
    int best_i = -1, maxweight = -1;
    for (int i = 0; i < 5; ++i)
    {
        if (best_i == -1 || weights[i] > maxweight)
        {
            maxweight = weights[i];
            best_i = i;
        }
    }
    return best_i;
}

static void _give_sac_group_feedback(int which)
{
    ASSERT_RANGE(which, 0, 5);
    const char* names[] = {
        "Escape", "Destruction", "Dungeons", "Summoning", "Wonder"
    };
    mprf(MSGCH_GOD, "A symbol of %s coalesces before you, then vanishes.",
         names[which]);
}

static void _ashenzari_sac_scroll(const item_def& item)
{
    int scr = SCR_CURSE_JEWELLERY;
    if (you.species != SP_FELID
        && (you.species != SP_OCTOPODE || one_chance_in(4)))
    {
        scr = random_choose(SCR_CURSE_WEAPON, SCR_CURSE_ARMOUR,
                            SCR_CURSE_JEWELLERY, -1);
    }
    int it = items(0, OBJ_SCROLLS, scr, true, 0, MAKE_ITEM_NO_RACE,
                   0, 0, GOD_ASHENZARI);
    if (it == NON_ITEM)
    {
        mpr("You feel the world is against you.");
        return;
    }

    mitm[it].quantity = 1;
    if (!move_item_to_grid(&it, you.pos(), true))
        destroy_item(it, true); // can't happen
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

            dummy.hit_dice = hd;
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

        if (food_is_rotten(item))
            chance--;
        else if (item.sub_type == CORPSE_SKELETON)
            chance -= 2;

        gain_piety(chance, 20);
        if (x_chance_in_y(chance, 20))
            relative_piety_gain = PIETY_SOME;
        break;
    }

    case GOD_NEMELEX_XOBEH:
    {
        if (you.attribute[ATTR_CARD_COUNTDOWN] && x_chance_in_y(value, 800))
        {
            you.attribute[ATTR_CARD_COUNTDOWN]--;
#if defined(DEBUG_DIAGNOSTICS) || defined(DEBUG_CARDS) || defined(DEBUG_SACRIFICE)
            mprf(MSGCH_DIAGNOSTICS, "Countdown down to %d",
                 you.attribute[ATTR_CARD_COUNTDOWN]);
#endif
        }
        // Nemelex piety gain is fairly fast... at least when you
        // have low piety.
        int piety_change = value/2 + 1;
        if (is_artefact(item))
            piety_change *= 2;
        int piety_denom = 30 + you.piety/2;

        gain_piety(piety_change, piety_denom);

        // Preserving the old behaviour of giving the big message for
        // artefacts and artefacts only.
        relative_piety_gain = x_chance_in_y(piety_change, piety_denom) ?
                                is_artefact(item) ?
                                  PIETY_LOTS : PIETY_SOME : PIETY_NONE;

        if (item.base_type == OBJ_FOOD && item.sub_type == FOOD_CHUNK
            || is_blood_potion(item))
        {
            // Count chunks and blood potions towards decks of
            // Summoning.
            you.sacrifice_value[OBJ_CORPSES] += value;
        }
        else
            you.sacrifice_value[item.base_type] += value;
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

bool check_nemelex_sacrificing_item_type(const item_def& item)
{
    switch (item.base_type)
    {
    case OBJ_ARMOUR:
        return you.nemelex_sacrificing[NEM_GIFT_ESCAPE];

    case OBJ_WEAPONS:
    case OBJ_STAVES:
    case OBJ_RODS:
    case OBJ_MISSILES:
        return you.nemelex_sacrificing[NEM_GIFT_DESTRUCTION];

    case OBJ_CORPSES:
        return you.nemelex_sacrificing[NEM_GIFT_SUMMONING];

    case OBJ_POTIONS:
        if (is_blood_potion(item))
            return you.nemelex_sacrificing[NEM_GIFT_SUMMONING];
        return you.nemelex_sacrificing[NEM_GIFT_WONDERS];

    case OBJ_FOOD:
        if (item.sub_type == FOOD_CHUNK)
            return you.nemelex_sacrificing[NEM_GIFT_SUMMONING];
    // else fall through
    case OBJ_WANDS:
    case OBJ_SCROLLS:
        return you.nemelex_sacrificing[NEM_GIFT_WONDERS];

    case OBJ_JEWELLERY:
    case OBJ_BOOKS:
    case OBJ_MISCELLANY:
        return you.nemelex_sacrificing[NEM_GIFT_DUNGEONS];

    default:
        return false;
    }
}

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
    item_def *disliked_item = 0;

    const int old_leading = _leading_sacrifice_group();

    while (i != NON_ITEM)
    {
        item_def &item(mitm[i]);
        const int next = item.link;  // in case we can't get it later.
        const bool disliked = !god_likes_item(you.religion, item);

        if (item_is_stationary(item) || disliked)
        {
            i = next;
            if (disliked)
            {
                num_disliked++;
                disliked_item = &item;
            }
            continue;
        }

        // Skip items you don't want to sacrifice right now.
        if (you_worship(GOD_NEMELEX_XOBEH)
            && !check_nemelex_sacrificing_item_type(item))
        {
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
            && (item.inscription.find("=p") != string::npos))
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

    if (num_sacced > 0 && you_worship(GOD_NEMELEX_XOBEH))
    {
        const int new_leading = _leading_sacrifice_group();
        if (old_leading != new_leading || one_chance_in(50))
            _give_sac_group_feedback(new_leading);

#if defined(DEBUG_GIFTS) || defined(DEBUG_CARDS) || defined(DEBUG_SACRIFICE)
        _show_pure_deck_chances();
#endif
    }
    // Explanatory messages if nothing the god likes is sacrificed.
    else if (num_sacced == 0 && num_disliked > 0)
    {
        ASSERT(disliked_item);

        if (item_is_orb(*disliked_item))
            simple_god_message(" wants the Orb's power used on the surface!");
        else if (item_is_rune(*disliked_item))
            simple_god_message(" wants the runes to be proudly displayed.");
        else if (item_is_horn_of_geryon(*disliked_item))
            simple_god_message(" doesn't want your path blocked.");
        // Zin was handled above, and the other gods don't care about
        // sacrifices.
        else if (god_likes_fresh_corpses(you.religion))
            simple_god_message(" only cares about fresh corpses!");
        else if (you_worship(GOD_BEOGH))
            simple_god_message(" only cares about orcish remains!");
        else if (you_worship(GOD_NEMELEX_XOBEH))
            if (disliked_item->base_type == OBJ_GOLD)
                simple_god_message(" does not care about gold!");
            else
                simple_god_message(" expects you to use your decks, not offer them!");
        else if (you_worship(GOD_ASHENZARI))
            simple_god_message(" can corrupt only scrolls of remove curse.");
    }
    if (num_sacced == 0 && you_worship(GOD_ELYVILON))
    {
        mprf("There are no %sweapons here to destroy!",
             you.piety_max[GOD_ELYVILON] < piety_breakpoint(2) ? "" : "unholy or evil ");
    }

    return (num_sacced > 0);
}
