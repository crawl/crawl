#include "AppHdr.h"

#include "godprayer.h"

#include <cmath>

#include "areas.h"
#include "artefact.h"
#include "coordit.h"
#include "effects.h"
#include "env.h"
#include "food.h"
#include "fprop.h"
#include "godabil.h"
#include "godpassive.h"
#include "invent.h"
#include "itemprop.h"
#include "items.h"
#include "item_use.h"
#include "player.h"
#include "makeitem.h"
#include "message.h"
#include "misc.h"
#include "monster.h"
#include "notes.h"
#include "options.h"
#include "random.h"
#include "religion.h"
#include "stash.h"
#include "state.h"
#include "stuff.h"
#include "terrain.h"
#include "view.h"

static void _offer_items();

static bool _confirm_pray_sacrifice(god_type god)
{
    if (Options.stash_tracking == STM_EXPLICIT && is_stash(you.pos()))
    {
        mpr("You can't sacrifice explicitly marked stashes.");
        return (false);
    }

    for (stack_iterator si(you.pos(), true); si; ++si)
    {
        if (god_likes_item(god, *si)
            && needs_handle_warning(*si, OPER_PRAY))
        {
            std::string prompt = "Really sacrifice stack with ";
            prompt += si->name(DESC_NOCAP_A);
            prompt += " in it?";

            if (!yesno(prompt.c_str(), false, 'n'))
                return (false);
        }
    }

    return (true);
}

std::string god_prayer_reaction()
{
    std::string result = god_name(you.religion);
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

    return (result);
}

bool god_accepts_prayer(god_type god)
{
    harm_protection_type hpt = god_protects_from_harm(god, false);

    if (hpt == HPT_PRAYING
        || hpt == HPT_PRAYING_PLUS_ANYTIME
        || hpt == HPT_RELIABLE_PRAYING_PLUS_ANYTIME)
    {
        return (true);
    }

    if (god_likes_fresh_corpses(god))
        return (true);

    switch (god)
    {
    case GOD_ZIN:
        return (zin_sustenance(false));

    case GOD_KIKUBAAQUDGHA:
        return (you.piety >= piety_breakpoint(4));

    case GOD_JIYVA:
        return (jiyva_can_paralyse_jellies());

    case GOD_BEOGH:
    case GOD_NEMELEX_XOBEH:
    case GOD_ASHENZARI:
        return (true);

    default:
        break;
    }

    return (false);
}

static bool _bless_weapon(god_type god, brand_type brand, int colour)
{
    item_def& wpn = *you.weapon();

    if (wpn.base_type != OBJ_WEAPONS
        || (is_range_weapon(wpn) && brand != SPWPN_HOLY_WRATH)
        || is_artefact(wpn))
    {
        return (false);
    }

    std::string prompt = "Do you wish to have your weapon ";
    if (brand == SPWPN_PAIN)
        prompt += "bloodied with pain";
    else if (brand == SPWPN_DISTORTION)
        prompt += "corrupted";
    else
        prompt += "blessed";
    prompt += "?";

    if (!yesno(prompt.c_str(), true, 'n'))
        return (false);

    you.duration[DUR_WEAPON_BRAND] = 0;     // just in case

    std::string old_name = wpn.name(DESC_NOCAP_A);
    set_equip_desc(wpn, ISFLAG_GLOWING);
    set_item_ego_type(wpn, OBJ_WEAPONS, brand);
    wpn.colour = colour;

    const bool is_cursed = wpn.cursed();

    enchant_weapon(ENCHANT_TO_HIT, true, wpn);

    if (coinflip())
        enchant_weapon(ENCHANT_TO_HIT, true, wpn);

    enchant_weapon(ENCHANT_TO_DAM, true, wpn);

    if (coinflip())
        enchant_weapon(ENCHANT_TO_DAM, true, wpn);

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
    you.num_current_gifts[god]++;
    you.num_total_gifts[god]++;
    std::string desc  = old_name + " ";
            desc += (god == GOD_SHINING_ONE   ? "blessed by the Shining One" :
                     god == GOD_LUGONU        ? "corrupted by Lugonu" :
                     god == GOD_KIKUBAAQUDGHA ? "bloodied by Kikubaaqudgha"
                                              : "touched by the gods");

    take_note(Note(NOTE_ID_ITEM, 0, 0,
              wpn.name(DESC_NOCAP_A).c_str(), desc.c_str()));
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
        you.gift_timeout = 0; // no protection during pain branding weapon

        torment(TORMENT_KIKUBAAQUDGHA, you.pos());

        // Bloodify surrounding squares (75% chance).
        for (radius_iterator ri(you.pos(), 2, true, true); ri; ++ri)
            if (!one_chance_in(4))
                maybe_bloodify_square(*ri);
    }

#ifndef USE_TILE
    // Allow extra time for the flash to linger.
    delay(1000);
#endif

    return (true);
}

static bool _altar_prayer()
{
    // Different message from when first joining a religion.
    mpr("You prostrate yourself in front of the altar and pray.");

    if (you.religion == GOD_XOM)
        return (false);

    god_acting gdact;

    bool did_bless = false;

    // TSO blesses weapons with holy wrath, and long blades and demon
    // whips specially.
    if (you.religion == GOD_SHINING_ONE
        && !you.num_total_gifts[GOD_SHINING_ONE]
        && !player_under_penance()
        && you.piety > 160)
    {
        item_def *wpn = you.weapon();

        if (wpn
            && (get_weapon_brand(*wpn) != SPWPN_HOLY_WRATH
                || is_blessed_convertible(*wpn)))
        {
            did_bless = _bless_weapon(GOD_SHINING_ONE, SPWPN_HOLY_WRATH,
                                      YELLOW);
        }
    }

    // Lugonu blesses weapons with distortion.
    if (you.religion == GOD_LUGONU
        && !you.num_total_gifts[GOD_LUGONU]
        && !player_under_penance()
        && you.piety > 160)
    {
        item_def *wpn = you.weapon();

        if (wpn && get_weapon_brand(*wpn) != SPWPN_DISTORTION)
            did_bless = _bless_weapon(GOD_LUGONU, SPWPN_DISTORTION, MAGENTA);
    }

    // Kikubaaqudgha blesses weapons with pain, or gives you a Necronomicon.
    if (you.religion == GOD_KIKUBAAQUDGHA
        && !you.num_total_gifts[GOD_KIKUBAAQUDGHA]
        && !player_under_penance()
        && you.piety > 160)
    {
        simple_god_message(
            " will bloody your weapon with pain or grant you the Necronomicon.");

        bool kiku_did_bless_weapon = false;

        item_def *wpn = you.weapon();

        // Does the player want a pain branding?
        if (wpn && get_weapon_brand(*wpn) != SPWPN_PAIN)
        {
            kiku_did_bless_weapon =
                _bless_weapon(GOD_KIKUBAAQUDGHA, SPWPN_PAIN, RED);
            did_bless = kiku_did_bless_weapon;
        }
        else
            mpr("You have no weapon to bloody with pain.");

        // If not, ask if the player wants a Necronomicon.
        if (!kiku_did_bless_weapon)
        {
            if (!yesno("Do you wish to receive the Necronomicon?", true, 'n'))
                return (false);

            int thing_created = items(1, OBJ_BOOKS, BOOK_NECRONOMICON, true, 1,
                                      MAKE_ITEM_RANDOM_RACE,
                                      0, 0, you.religion);

            if (thing_created == NON_ITEM)
                return (false);

            move_item_to_grid(&thing_created, you.pos());

            if (thing_created != NON_ITEM)
            {
                simple_god_message(" grants you a gift!");
                more();

                you.num_current_gifts[you.religion]++;
                you.num_total_gifts[you.religion]++;
                did_bless = true;
                take_note(Note(NOTE_GOD_GIFT, you.religion));
                mitm[thing_created].inscription = "god gift";
            }
        }

        // Return early so we don't offer our Necronomicon to Kiku.
        return (did_bless);
    }

    _offer_items();

    return (did_bless);
}

void pray()
{
    if (silenced(you.pos()))
    {
        mpr("You are unable to make a sound!");
        return;
    }

    // almost all prayers take time
    you.turn_is_over = true;

    const bool was_praying = !!you.duration[DUR_PRAYER];

    bool something_happened = false;
    const god_type altar_god = feat_altar_god(grd(you.pos()));
    if (altar_god != GOD_NO_GOD)
    {
        if (you.flight_mode() == FL_LEVITATE)
        {
            you.turn_is_over = false;
            mpr("You are floating high above the altar.");
            return;
        }

        if (you.religion != GOD_NO_GOD && altar_god == you.religion)
        {
            something_happened = _altar_prayer();
        }
        else if (altar_god != GOD_NO_GOD)
        {
            if (you.species == SP_DEMIGOD)
            {
                you.turn_is_over = false;
                mpr("Sorry, a being of your status cannot worship here.");
                return;
            }

            god_pitch(feat_altar_god(grd(you.pos())));
            return;
        }
    }

    if (you.religion == GOD_NO_GOD)
    {
        mprf(MSGCH_PRAY,
             "You spend a moment contemplating the meaning of %slife.",
             you.is_undead ? "un" : "");

        // Zen meditation is timeless.
        you.turn_is_over = false;
        return;
    }
    else if (!god_accepts_prayer(you.religion))
    {
        if (!something_happened)
        {
            simple_god_message(" ignores your prayer.");
            you.turn_is_over = false;
        }
        return;
    }

    // Beoghites and Nemelexites can abort now instead of offering
    // something they don't want to lose.
    if (altar_god == GOD_NO_GOD
        && (you.religion == GOD_BEOGH ||  you.religion == GOD_NEMELEX_XOBEH)
        && !_confirm_pray_sacrifice(you.religion))
    {
        you.turn_is_over = false;
        return;
    }

    mprf(MSGCH_PRAY, "You %s prayer to %s.",
         was_praying ? "renew your" : "offer a",
         god_name(you.religion).c_str());

    you.duration[DUR_PRAYER] = 9 + (random2(you.piety) / 20)
                                 + (random2(you.piety) / 20);

    if (player_under_penance())
        simple_god_message(" demands penance!");
    else
    {
        mpr(god_prayer_reaction().c_str(), MSGCH_PRAY, you.religion);

        if (you.piety > 130)
            you.duration[DUR_PRAYER] *= 3;
        else if (you.piety > 70)
            you.duration[DUR_PRAYER] *= 2;
    }

    // Assume for now that gods who like fresh corpses and/or butchery
    // don't use prayer for anything else.
    if (you.religion == GOD_ZIN
        || you.religion == GOD_KIKUBAAQUDGHA
        || you.religion == GOD_BEOGH
        || you.religion == GOD_NEMELEX_XOBEH
        || you.religion == GOD_JIYVA
        || you.religion == GOD_ASHENZARI
        || god_likes_fresh_corpses(you.religion))
    {
        you.duration[DUR_PRAYER] = 1;
    }
    else if (you.religion == GOD_ELYVILON
        || you.religion == GOD_YREDELEMNUL)
    {
        you.duration[DUR_PRAYER] = 20;
    }

    // Gods who like fresh corpses, Kikuites, Beoghites, Nemelexites and
    // Ashenzariites offer the items they're standing on.
    if (altar_god == GOD_NO_GOD
        && (god_likes_fresh_corpses(you.religion)
            || you.religion == GOD_BEOGH
            || you.religion == GOD_NEMELEX_XOBEH
            || you.religion == GOD_ASHENZARI))
    {
        _offer_items();
    }

    if (!was_praying)
        do_god_gift(true);

    dprf("piety: %d (-%d)", you.piety, you.piety_hysteresis);
}

void end_prayer(void)
{
    mpr("Your prayer is over.", MSGCH_PRAY, you.religion);
    you.duration[DUR_PRAYER] = 0;
}

static int _gold_to_donation(int gold)
{
    return static_cast<int>((gold * log((float)gold)) / MAX_PIETY);
}

static void _zin_donate_gold()
{
    if (you.gold == 0)
    {
        mpr("You don't have anything to sacrifice.");
        return;
    }

    if (!yesno("Do you wish to donate half of your money?", true, 'n'))
        return;

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
        return;
    }

    you.duration[DUR_PIETY_POOL] += donation;
    if (you.duration[DUR_PIETY_POOL] > 30000)
        you.duration[DUR_PIETY_POOL] = 30000;

    const int estimated_piety =
        std::min(MAX_PENANCE + MAX_PIETY,
                 you.piety + you.duration[DUR_PIETY_POOL]);

    if (player_under_penance())
    {
        if (estimated_piety >= you.penance[GOD_ZIN])
            mpr("You feel that you will soon be absolved of all your sins.");
        else
            mpr("You feel that your burden of sins will soon be lighter.");
        return;
    }

    std::string result = "You feel that " + god_name(GOD_ZIN)
                       + " will soon be ";
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
    ASSERT(which >= 0 && which < 5);
    const char* names[] = {
        "Escape", "Destruction", "Dungeons", "Summoning", "Wonder"
    };
    mprf(MSGCH_GOD, "A symbol of %s coalesces before you, then vanishes.",
         names[which]);
}

static void _ashenzari_sac_scroll(const item_def& item)
{
    int scr = (you.species == SP_CAT) ? SCR_CURSE_JEWELLERY :
              random_choose(SCR_CURSE_WEAPON, SCR_CURSE_ARMOUR,
                            SCR_CURSE_JEWELLERY, -1);
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

// God effects of sacrificing one item from a stack (e.g., a weapon, one
// out of 20 arrows, etc.).  Does not modify the actual item in any way.
static piety_gain_t _sacrifice_one_item_noncount(const item_def& item,
       int *js)
{
    piety_gain_t relative_piety_gain = PIETY_NONE;

    // XXX: this assumes that there's no overlap between
    //      item-accepting gods and corpse-accepting gods.
    if (god_likes_fresh_corpses(you.religion))
    {
        gain_piety(13, 19);

        // The feedback is not accurate any longer on purpose; it only reveals
        // the rate you get piety at.
        if (x_chance_in_y(13, 19))
            relative_piety_gain = PIETY_SOME;
    }
    else
    {
        switch (you.religion)
        {
        case GOD_BEOGH:
        {
            const int item_orig = item.orig_monnum - 1;

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
            // item_value() multiplies by quantity.
            const int value = item_value(item) / item.quantity;

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
            int piety_change, piety_denom;
            if (item.base_type == OBJ_CORPSES)
            {
                piety_change = 1;
                piety_denom = 2 + you.piety/50;
            }
            else
            {
                piety_change = value/2 + 1;
                if (is_artefact(item))
                    piety_change *= 2;
                piety_denom = 30 + you.piety/2;
            }

            gain_piety(piety_change, piety_denom);

            // Preserving the old behaviour of giving the big message for
            // artifacts and artifacts only.
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
            else if (item.base_type == OBJ_CORPSES)
            {
#if defined(DEBUG_GIFTS) || defined(DEBUG_CARDS) || defined(DEBUG_SACRIFICE)
                mprf(MSGCH_DIAGNOSTICS, "Corpse mass is %d",
                     item_mass(item));
#endif
                you.sacrifice_value[item.base_type] += item_mass(item);
            }
            else
                you.sacrifice_value[item.base_type] += value;
            break;
        }

        case GOD_JIYVA:
        {
            // item_value() multiplies by quantity.
            const int value = item_value(item) / item.quantity;
            // compress into range 0..250
            const int stepped = stepdown_value(value, 50, 50, 200, 250);
            gain_piety(stepped, 50);
            relative_piety_gain = (piety_gain_t)std::min(2,
                                    div_rand_round(stepped, 50));
            jiyva_slurp_bonus(div_rand_round(stepped, 50), js);
            break;
        }

        case GOD_ASHENZARI:
            _ashenzari_sac_scroll(item);
            break;

        default:
            break;
        }
    }

    return (relative_piety_gain);
}

piety_gain_t sacrifice_item_stack(const item_def& item, int *js)
{
    piety_gain_t relative_gain = PIETY_NONE;
    for (int j = 0; j < item.quantity; ++j)
    {
        const piety_gain_t gain = _sacrifice_one_item_noncount(item, js);

        // Update piety gain if necessary.
        if (gain != PIETY_NONE)
        {
            if (relative_gain == PIETY_NONE)
                relative_gain = gain;
            else            // some + some = lots
                relative_gain = PIETY_LOTS;
        }
    }
    return (relative_gain);
}

static bool _check_nemelex_sacrificing_item_type(const item_def& item)
{
    switch (item.base_type)
    {
    case OBJ_ARMOUR:
        return (you.nemelex_sacrificing[NEM_GIFT_ESCAPE]);

    case OBJ_WEAPONS:
    case OBJ_STAVES:
    case OBJ_MISSILES:
        return (you.nemelex_sacrificing[NEM_GIFT_DESTRUCTION]);

    case OBJ_CORPSES:
        return (you.nemelex_sacrificing[NEM_GIFT_SUMMONING]);

    case OBJ_POTIONS:
        if (is_blood_potion(item))
            return (you.nemelex_sacrificing[NEM_GIFT_SUMMONING]);
        return (you.nemelex_sacrificing[NEM_GIFT_WONDERS]);

    case OBJ_FOOD:
        if (item.sub_type == FOOD_CHUNK)
            return (you.nemelex_sacrificing[NEM_GIFT_SUMMONING]);
    // else fall through
    case OBJ_WANDS:
    case OBJ_SCROLLS:
        return (you.nemelex_sacrificing[NEM_GIFT_WONDERS]);

    case OBJ_JEWELLERY:
    case OBJ_BOOKS:
    case OBJ_MISCELLANY:
        return (you.nemelex_sacrificing[NEM_GIFT_DUNGEONS]);

    default:
        return false;
    }
}

static void _offer_items()
{
    if (you.religion == GOD_NO_GOD || !god_likes_items(you.religion))
        return;

    int i = you.visible_igrd(you.pos());

    god_acting gdact;

    // donate gold to gain piety distributed over time
    if (you.religion == GOD_ZIN)
    {
        _zin_donate_gold();

        return; // doesn't accept anything else for sacrifice
    }

    if (i == NON_ITEM) // nothing to sacrifice
        return;

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
        if (you.religion == GOD_NEMELEX_XOBEH
            && !_check_nemelex_sacrificing_item_type(item))
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
            && (item.inscription.find("=p") != std::string::npos))
        {
            const std::string msg =
                  "Really sacrifice " + item.name(DESC_NOCAP_A) + "?";

            if (!yesno(msg.c_str(), false, 'n'))
            {
                i = next;
                continue;
            }
        }


#if defined(DEBUG_DIAGNOSTICS) || defined(DEBUG_SACRIFICE)
        mprf(MSGCH_DIAGNOSTICS, "Sacrifice item value: %d",
             item_value(item));
#endif

        piety_gain_t relative_gain = sacrifice_item_stack(item);
        print_sacrifice_message(you.religion, mitm[i], relative_gain);
        item_was_destroyed(mitm[i]);
        destroy_item(i);
        i = next;
        num_sacced++;
    }

    if (num_sacced > 0 && you.religion == GOD_NEMELEX_XOBEH)
    {
        const int new_leading = _leading_sacrifice_group();
        if (old_leading != new_leading || one_chance_in(50))
            _give_sac_group_feedback(new_leading);

#if defined(DEBUG_GIFTS) || defined(DEBUG_CARDS) || defined(DEBUG_SACRIFICE)
        _show_pure_deck_chances();
#endif
    }

    if (num_sacced > 0 && you.religion == GOD_KIKUBAAQUDGHA)
    {
        simple_god_message(" torments the living!");
        torment(TORMENT_KIKUBAAQUDGHA, you.pos());
        lose_piety(random_range(8, 12));
    }

    // Explanatory messages if nothing the god likes is sacrificed.
    else if (num_sacced == 0 && num_disliked > 0)
    {
        ASSERT(disliked_item);

        if (item_is_orb(*disliked_item))
            simple_god_message(" wants the Orb's power used on the surface!");
        else if (item_is_rune(*disliked_item))
            simple_god_message(" wants the runes to be proudly displayed.");
        else if (disliked_item->base_type == OBJ_MISCELLANY
                 && disliked_item->sub_type == MISC_HORN_OF_GERYON)
            simple_god_message(" doesn't want your path blocked.");
        // Zin was handled above, and the other gods don't care about
        // sacrifices.
        else if (god_likes_fresh_corpses(you.religion))
            simple_god_message(" only cares about fresh corpses!");
        else if (you.religion == GOD_BEOGH)
            simple_god_message(" only cares about orcish remains!");
        else if (you.religion == GOD_NEMELEX_XOBEH)
            if (disliked_item->base_type == OBJ_GOLD)
                simple_god_message(" does not care about gold!");
            else
                simple_god_message(" expects you to use your decks, not offer them!");
        else if (you.religion == GOD_ASHENZARI)
            simple_god_message(" can corrupt only scrolls of remove curse.");
    }
}
