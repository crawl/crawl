/*
 *  File:       religion.cc
 *  Summary:    Misc religion related functions.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 *
 *  Change History (most recent first):
 *
 *
 *   <7>   11jan2001     gdl    added M. Valvoda's changes to
 *                              god_colour() and god_name()
 *   <6>   06-Mar-2000   bwr    added penance, gift_timeout,
 *                              divine_retribution(), god_speaks()
 *   <5>   11/15/99      cdl    Fixed Daniel's yellow Xom patch  :)
 *                              Xom will sometimes answer prayers
 *   <4>   10/11/99      BCR    Added Daniel's yellow Xom patch
 *   <3>    6/13/99      BWR    Vehumet book giving code.
 *   <2>    5/20/99      BWR    Added screen redraws
 *   <1>    -/--/--      LRH    Created
 */

#include "AppHdr.h"
#include "religion.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <cmath>

#ifdef DOS
#include <dos.h>
#endif

#include "externs.h"

#include "abl-show.h"
#include "beam.h"
#include "chardump.h"
#include "cloud.h"
#include "database.h"
#include "debug.h"
#include "decks.h"
#include "describe.h"
#include "effects.h"
#include "files.h"
#include "food.h"
#include "invent.h"
#include "it_use2.h"
#include "itemname.h"
#include "itemprop.h"
#include "item_use.h"
#include "items.h"
#include "macro.h"
#include "makeitem.h"
#include "message.h"
#include "misc.h"
#include "monplace.h"
#include "monstuff.h"
#include "mutation.h"
#include "newgame.h"
#include "notes.h"
#include "ouch.h"
#include "player.h"
#include "randart.h"
#include "shopping.h"
#include "skills2.h"
#include "spells1.h"
#include "spells2.h"
#include "spells3.h"
#include "spells4.h"
#include "spl-book.h"
#include "spl-cast.h"
#include "spl-util.h"
#include "stash.h"
#include "state.h"
#include "stuff.h"
#include "terrain.h"
#include "tutorial.h"
#include "view.h"
#include "xom.h"

#if DEBUG_RELIGION
#    define DEBUG_DIAGNOSTICS 1
#    define DEBUG_GIFTS       1
#    define DEBUG_SACRIFICE   1
#    define DEBUG_PIETY       1
#endif

#define PIETY_HYSTERESIS_LIMIT 1

// Item offer messages for the gods:
// & is replaced by "is" or "are" as appropriate for the item.
// % is replaced by "s" or "" as appropriate.
// First message is if there's no piety gain, second is if piety gain
// is one, third message is for piety gain > 1 (currently unused).
const char *sacrifice[NUM_GODS][3] =
{
    // No god
    {
        " & eaten by a bored swarm of bugs.",
        " & eaten by a swarm of bugs.",
        " & eaten by a ravening swarm of bugs."
    },
    // Zin
    {
         " barely glow% and disappear%.",
         " glow% silver and disappear%.",
         " glow% blindingly silver and disappear%.",
    },
    // TSO
    {
         " faintly glow% and disappear%.",
         " glow% a golden colour and disappear%.",
         " glow% a brilliant golden colour and disappear%.",
    },
    // Kikubaaqudgha
    {
         " slowly rot% away.",
         " rot% away.",
         " rot% away in an instant.",
    },
    // Yredelemnul
    {
        " slowly crumble% to dust.",
        " crumble% to dust.",
        " turn% to dust in an instant.",
    },
    // Xom (no sacrifices)
    {
        " & eaten by a bored bug.",
        " & eaten by a bug.",
        " & eaten by a greedy bug.",
    },
    // Vehumet
    {
         " fade% into nothingness.",
         " burn% into nothingness.",
         " explode% into nothingness.",
    },
    // Okawaru
    {
         " slowly burn% to ash.",
         " & consumed by flame.",
         " & consumed in a burst of flame.",
    },
    // Makhleb
    {
         " disappear% without a sign.",
         " flare% red and disappear%.",
         " flare% blood-red and disappear%.",
    },
    // Sif Muna
    {
         " & gone without a glow.",
         " glow% faintly for a moment, and & gone.",
         " glow% for a moment, and & gone.",
    },
    // Trog
    {
         " & slowly consumed by flames.",
         " & consumed in a column of flame.",
         " & consumed in a roaring column of flame.",
    },
    // Nemelex
    {
         " disappear% without a glow.",
         " glow% slightly and disappear%.",
         " glow% with a rainbow of weird colours and disappear%.",
    },
    // Elyvilon
    {
         " slowly evaporate%.",
         " evaporate%.",
         " glow% and evaporate%.",
    },
    // Lugonu
    {
         " & disappears into the void.",
         " & consumed by the void.",
         " & voraciously consumed by the void.",
    },
    // Beogh
    {
         " slowly crumble% into the ground.",
         " crumble% into the ground.",
         " disintegrate% into the ground.",
    }
};

const char* god_gain_power_messages[NUM_GODS][MAX_GOD_ABILITIES] =
{
    // no god
    { "", "", "", "", "" },
    // Zin
    { "recite Zin's Axioms of Law",
      "smite your foes",
      "call upon Zin for revitalisation",
      "",
      "call upon Zin to create a Sanctuary" },
    // TSO
    { "You and your allies can now gain power from killing evil.",
      "call upon The Shining One for a divine shield",
      "",
      "hurl blasts of cleansing flame",
      "summon a divine warrior" },
    // Kikubaaqudgha
    { "recall your undead slaves",
      "Kikubaaqudgha is protecting you from some side-effects of death magic.",
      "permanently enslave the undead",
      "",
      "summon an emissary of Death" },
    // Yredelemnul
    { "animate corpses",
      "recall your undead slaves",
      "animate legions of the dead",
      "drain ambient lifeforce",
      "control the undead" },
    // Xom
    { "", "", "", "", "" },
    // Vehumet
    { "gain magical power from killing",
      "You can call upon Vehumet to aid your destructive magics with prayer.",
      "During prayer you have some protection from summoned creatures.",
      "",
      "" },
    // Okawaru
    { "give your body great, but temporary strength",
      "",
      "",
      "",
      "haste yourself" },
    // Makhleb
    { "gain power from killing",
      "harness Makhleb's destructive might",
      "summon a lesser servant of Makhleb",
      "hurl Makhleb's greater destruction",
      "summon a greater servant of Makhleb" },
    // Sif Muna
    { "tap ambient magical fields",
      "freely open your mind to new spells",
      "",
      "Sif Muna is protecting you from some side-effects of spellcasting.",
      "" },
    // Trog
    { "go berserk at will",
      "call upon Trog for regeneration",
      "",
      "call in reinforcement",
      "" },
    // Nemelex
    { "peek at three random cards from a deck",
      "draw cards from decks in your inventory",
      "choose one out of three cards",
      "mark decks",
      "order the the top five cards of a deck, forfeiting the rest" },
    // Elyvilon
    { "call upon Elyvilon for minor healing",
      "call upon Elyvilon for purification",
      "call upon Elyvilon for moderate healing",
      "call upon Elyvilon to restore your abilities",
      "call upon Elyvilon for incredible healing" },
    // Lugonu
    { "depart the Abyss - at a permanent cost",
      "bend space around yourself",
      "banish your foes",
      "corrupt the fabric of space",
      "gate yourself to the Abyss" },
    // Beogh
    { "Beogh supports the use of orcish gear.",
      "smite your foes",
      "gain orcish followers",
      "recall your orcish followers",
      "walk on water" }
};

const char* god_lose_power_messages[NUM_GODS][MAX_GOD_ABILITIES] =
{
    // no god
    { "", "", "", "", "" },
    // Zin
    { "recite Zin's Axioms of Law",
      "smite your foes",
      "call upon Zin for revitalisation",
      "",
      "call upon Zin to create a Sanctuary" },
    // TSO
    { "You and your allies can no longer gain power from killing evil.",
      "call upon The Shining One for a divine shield",
      "",
      "hurl blasts of cleansing flame",
      "summon a divine warrior" },
    // Kikubaaqudgha
    { "recall your undead slaves",
      "Kikubaaqudgha is no longer shielding you from miscast death magic.",
      "permanently enslave the undead",
      "",
      "summon an emissary of Death" },
    // Yredelemnul
    { "animate corpses",
      "recall your undead slaves",
      "animate legions of the dead",
      "drain ambient lifeforce",
      "control the undead" },
    // Xom
    { "", "", "", "", "" },
    // Vehumet
    { "gain magical power from killing",
      "Vehumet will no longer aid your destructive magics.",
      "Vehumet will no longer shield you from summoned creatures.",
      "",
      "" },
    // Okawaru
    { "give your body great, but temporary strength",
      "",
      "",
      "",
      "haste yourself" },
    // Makhleb
    { "gain power from killing",
      "harness Makhleb's destructive might",
      "summon a lesser servant of Makhleb",
      "hurl Makhleb's greater destruction",
      "summon a greater servant of Makhleb" },
    // Sif Muna
    { "tap ambient magical fields",
      "forget spells at will",
      "",
      "Sif Muna is no longer protecting you from miscast magic.",
      "" },
    // Trog
    { "go berserk at will",
      "call upon Trog for regeneration",
      "",
      "call in reinforcement",
      "" },
    // Nemelex
    { "peek at three random cards from a deck",
      "draw cards from decks in your inventory",
      "choose one out of three cards",
      "mark decks",
      "order decks" },
    // Elyvilon
    { "call upon Elyvilon for minor healing",
      "call upon Elyvilon for purification",
      "call upon Elyvilon for moderate healing",
      "call upon Elyvilon to restore your abilities",
      "call upon Elyvilon for incredible healing" },
    // Lugonu
    { "depart the Abyss at will",
      "bend space around yourself",
      "banish your foes",
      "corrupt the fabric of space",
      "gate yourself to the Abyss" },
    // Beogh
    { "Beogh no longer supports the use of orcish gear.",
      "smite your foes",
      "gain orcish followers",
      "recall your orcish followers",
      "walk on water" }
};

void altar_prayer(void);
void dec_penance(god_type god, int val);
void dec_penance(int val);
void inc_penance(god_type god, int val);
void inc_penance(int val);
static bool moral_beings_attitude_change();
static bool beogh_followers_abandon_you(void);
static void dock_piety(int piety_loss, int penance);
static bool make_god_gifts_disappear(bool level_only = true);
static bool make_god_gifts_neutral(bool level_only = true);
static bool make_god_gifts_hostile(bool level_only = true);

bool is_evil_god(god_type god)
{
    return
        god == GOD_KIKUBAAQUDGHA ||
        god == GOD_MAKHLEB ||
        god == GOD_YREDELEMNUL ||
        god == GOD_BEOGH ||
        god == GOD_LUGONU;
}

bool is_good_god(god_type god)
{
    return
        god == GOD_SHINING_ONE ||
        god == GOD_ZIN ||
        god == GOD_ELYVILON;
}

bool is_priest_god(god_type god)
{
    return
        god == GOD_ZIN ||
        god == GOD_YREDELEMNUL ||
        god == GOD_BEOGH;
}

void dec_penance(god_type god, int val)
{
    if (you.penance[god] > 0)
    {
#if DEBUG_PIETY
        mprf(MSGCH_DIAGNOSTICS, "Decreasing penance by %d", val);
#endif
        if (you.penance[god] <= val)
        {
            simple_god_message(" seems mollified.", god);
            take_note(Note(NOTE_MOLLIFY_GOD, god));
            you.penance[god] = 0;

            // the halo is once more available
            if (god == GOD_SHINING_ONE && you.religion == GOD_SHINING_ONE
                && you.piety >= piety_breakpoint(0))
            {
                mpr("Your divine halo starts to return!");
            }

            // bonuses now once more effective
            if (god == GOD_BEOGH && you.religion == GOD_BEOGH)
                 you.redraw_armour_class = true;

            // When you've worked through all your penance, you get
            // another chance to make hostile holy beings neutral.
            if (is_good_god(you.religion))
                moral_beings_attitude_change();
        }
        else
            you.penance[god] -= val;
    }
}                               // end dec_penance()

void dec_penance(int val)
{
    dec_penance(you.religion, val);
}                               // end dec_penance()

bool beogh_water_walk()
{
    return
        you.religion == GOD_BEOGH &&
        !player_under_penance() &&
        you.piety >= piety_breakpoint(4);
}

static bool need_water_walking()
{
    return
        !player_is_airborne() && you.species != SP_MERFOLK &&
        grd[you.x_pos][you.y_pos] == DNGN_DEEP_WATER;
}

void inc_penance(god_type god, int val)
{
    if (you.penance[god] == 0 && val > 0)
    {
        take_note(Note(NOTE_PENANCE, god));

        // orcish bonuses don't apply under penance
        if (god == GOD_BEOGH)
            you.redraw_armour_class = true;
        // nor does TSO's halo or divine shield
        else if (god == GOD_SHINING_ONE)
        {
            mpr("Your divine halo starts to fade.");

            if (you.duration[DUR_DIVINE_SHIELD])
            {
                mpr("Your divine shield disappears!");
                you.duration[DUR_DIVINE_SHIELD] = 0;
                you.attribute[ATTR_DIVINE_SHIELD] = 0;
                you.redraw_armour_class = true;
            }
            make_god_gifts_disappear(true); // only on level
        }
    }

    if (you.penance[god] + val > 200)
        you.penance[god] = 200;
    else
        you.penance[god] += val;

    if (god == GOD_BEOGH && need_water_walking() && !beogh_water_walk())
    {
         fall_into_a_pool( you.x_pos, you.y_pos, true,
                           grd[you.x_pos][you.y_pos] );
    }
}                               // end inc_penance()

void inc_penance(int val)
{
    inc_penance(you.religion, val);
}                               // end inc_penance()

static void inc_gift_timeout(int val)
{
    if (you.gift_timeout + val > 200)
        you.gift_timeout = 200;
    else
        you.gift_timeout += val;
}                               // end inc_gift_timeout()

// Only Yredelemnul and Okawaru use this for now
static monster_type random_servant(god_type god)
{
    // error trapping {dlb}
    monster_type thing_called = MONS_PROGRAM_BUG;
    int temp_rand = random2(100);

    switch (god)
    {
    case GOD_YREDELEMNUL:
        // undead
        thing_called = ((temp_rand > 66) ? MONS_WRAITH :            // 33%
                        (temp_rand > 52) ? MONS_WIGHT :             // 13%
                        (temp_rand > 40) ? MONS_SPECTRAL_WARRIOR :  // 11%
                        (temp_rand > 31) ? MONS_ROTTING_HULK :      //  8%
                        (temp_rand > 23) ? MONS_SKELETAL_WARRIOR :  //  7%
                        (temp_rand > 16) ? MONS_VAMPIRE :           //  6%
                        (temp_rand > 10) ? MONS_GHOUL :             //  5%
                        (temp_rand >  4) ? MONS_MUMMY               //  5%
                                         : MONS_FLAYED_GHOST);      //  4%
        break;
    case GOD_OKAWARU:
        // warriors
        thing_called = ((temp_rand > 84) ? MONS_ORC_WARRIOR :       // 15%
                        (temp_rand > 69) ? MONS_ORC_KNIGHT :        // 14%
                        (temp_rand > 59) ? MONS_NAGA_WARRIOR :      //  9%
                        (temp_rand > 49) ? MONS_CENTAUR_WARRIOR :   //  9%
                        (temp_rand > 39) ? MONS_STONE_GIANT :       //  9%
                        (temp_rand > 29) ? MONS_FIRE_GIANT :        //  9%
                        (temp_rand > 19) ? MONS_FROST_GIANT :       //  9%
                        (temp_rand >  9) ? MONS_CYCLOPS :           //  9%
                        (temp_rand >  4) ? MONS_HILL_GIANT          //  4%
                                         : MONS_TITAN);             //  4%

        break;
    default:
        break;
    }

    return (thing_called);
}

static const item_def *find_missile_launcher(int skill)
{
    for (int i = 0; i < ENDOFPACK; ++i)
    {
        if (!is_valid_item(you.inv[i]))
            continue;

        const item_def &item = you.inv[i];
        if (is_range_weapon(item)
            && range_skill(item) == skill_type(skill))
        {
            return (&item);
        }
    }
    return (NULL);
}

static int ammo_count(const item_def *launcher)
{
    int count = 0;
    const missile_type mt = launcher? fires_ammo_type(*launcher) : MI_DART;

    for (int i = 0; i < ENDOFPACK; ++i)
    {
        if (!is_valid_item(you.inv[i]))
            continue;

        const item_def &item = you.inv[i];
        if (item.base_type == OBJ_MISSILES && item.sub_type == mt)
            count += item.quantity;
    }

    return (count);
}

static bool need_missile_gift()
{
    const int best_missile_skill = best_skill(SK_SLINGS, SK_THROWING);
    const item_def *launcher = find_missile_launcher(best_missile_skill);
    return (you.piety > 80
            && random2( you.piety ) > 70
            && !grid_destroys_items( grd[you.x_pos][you.y_pos] )
            && one_chance_in(8)
            && you.skills[ best_missile_skill ] >= 8
            && (launcher || best_missile_skill == SK_DARTS)
            && ammo_count(launcher) < 20 + random2(35));
}

static void get_pure_deck_weights(int weights[])
{
    weights[0] = you.sacrifice_value[OBJ_ARMOUR] + 1;
    weights[1] = you.sacrifice_value[OBJ_WEAPONS] +
        you.sacrifice_value[OBJ_STAVES] +
        you.sacrifice_value[OBJ_MISSILES] + 1;
    weights[2] = you.sacrifice_value[OBJ_MISCELLANY] +
        you.sacrifice_value[OBJ_JEWELLERY] +
        you.sacrifice_value[OBJ_BOOKS] +
        you.sacrifice_value[OBJ_GOLD];
    weights[3] = you.sacrifice_value[OBJ_CORPSES] / 2;
    weights[4] = you.sacrifice_value[OBJ_POTIONS] +
        you.sacrifice_value[OBJ_SCROLLS] +
        you.sacrifice_value[OBJ_WANDS] +
        you.sacrifice_value[OBJ_FOOD];
}

static void update_sacrifice_weights(int which)
{
    switch ( which )
    {
    case 0:
        you.sacrifice_value[OBJ_ARMOUR] /= 5;
        you.sacrifice_value[OBJ_ARMOUR] *= 4;
        break;
    case 1:
        you.sacrifice_value[OBJ_WEAPONS]  /= 5;
        you.sacrifice_value[OBJ_STAVES]   /= 5;
        you.sacrifice_value[OBJ_MISSILES] /= 5;
        you.sacrifice_value[OBJ_WEAPONS]  *= 4;
        you.sacrifice_value[OBJ_STAVES]   *= 4;
        you.sacrifice_value[OBJ_MISSILES] *= 4;
        break;
    case 2:
        you.sacrifice_value[OBJ_MISCELLANY] /= 5;
        you.sacrifice_value[OBJ_JEWELLERY]  /= 5;
        you.sacrifice_value[OBJ_BOOKS]      /= 5;
        you.sacrifice_value[OBJ_GOLD]       /= 5;
        you.sacrifice_value[OBJ_MISCELLANY] *= 4;
        you.sacrifice_value[OBJ_JEWELLERY]  *= 4;
        you.sacrifice_value[OBJ_BOOKS]      *= 4;
        you.sacrifice_value[OBJ_GOLD]       *= 4;
    case 3:
        you.sacrifice_value[OBJ_CORPSES] /= 5;
        you.sacrifice_value[OBJ_CORPSES] *= 4;
        break;
    case 4:
        you.sacrifice_value[OBJ_POTIONS] /= 5;
        you.sacrifice_value[OBJ_SCROLLS] /= 5;
        you.sacrifice_value[OBJ_WANDS]   /= 5;
        you.sacrifice_value[OBJ_FOOD]    /= 5;
        you.sacrifice_value[OBJ_POTIONS] *= 4;
        you.sacrifice_value[OBJ_SCROLLS] *= 4;
        you.sacrifice_value[OBJ_WANDS]   *= 4;
        you.sacrifice_value[OBJ_FOOD]    *= 4;
        break;
    }
}

#if DEBUG_GIFTS || DEBUG_CARDS
static void show_pure_deck_chances()
{
    int weights[5];

    get_pure_deck_weights(weights);

    float total = (float) (weights[0] + weights[1] + weights[2] + weights[3] +
                           weights[4]);

    mprf(MSGCH_DIAGNOSTICS, "Pure cards chances: "
         "escape %0.2f%%, destruction %0.2f%%, dungeons %0.2f%%,"
         "summoning %0.2f%%, wonders %0.2f%%",
         (float)weights[0] / total * 100.0,
         (float)weights[1] / total * 100.0,
         (float)weights[2] / total * 100.0,
         (float)weights[3] / total * 100.0,
         (float)weights[4] / total * 100.0);
}
#endif

static void give_nemelex_gift()
{
    if ( grid_destroys_items(grd[you.x_pos][you.y_pos]) )
        return;

    // Nemelex will give at least one gift early.
    if ((you.num_gifts[GOD_NEMELEX_XOBEH] == 0
         && random2(piety_breakpoint(1)) < you.piety) ||
        (random2(MAX_PIETY) <= you.piety
         && one_chance_in(3)
         && !you.attribute[ATTR_CARD_COUNTDOWN]))
    {
        misc_item_type gift_type;

        // make a pure deck
        const misc_item_type pure_decks[] = {
            MISC_DECK_OF_ESCAPE,
            MISC_DECK_OF_DESTRUCTION,
            MISC_DECK_OF_DUNGEONS,
            MISC_DECK_OF_SUMMONING,
            MISC_DECK_OF_WONDERS
        };
        int weights[5];
        get_pure_deck_weights(weights);
        const int choice = choose_random_weighted(weights, weights+5);
        gift_type = pure_decks[choice];
#if DEBUG_GIFTS || DEBUG_CARDS
        show_pure_deck_chances();
#endif
        update_sacrifice_weights(choice);

        int thing_created = items( 1, OBJ_MISCELLANY, gift_type,
                                   true, 1, MAKE_ITEM_RANDOM_RACE );

        if (thing_created != NON_ITEM)
        {
            // Piety|Common  | Rare  |Legendary
            // --------------------------------
            //     0:  95.00%,  5.00%,  0.00%
            //    20:  86.00%, 10.50%,  3.50%
            //    40:  77.00%, 16.00%,  7.00%
            //    60:  68.00%, 21.50%, 10.50%
            //    80:  59.00%, 27.00%, 14.00%
            //   100:  50.00%, 32.50%, 17.50%
            //   120:  41.00%, 38.00%, 21.00%
            //   140:  32.00%, 43.50%, 24.50%
            //   160:  23.00%, 49.00%, 28.00%
            //   180:  14.00%, 54.50%, 31.50%
            //   200:   5.00%, 60.00%, 35.00%
            int common_weight = 95 - (90 * you.piety / MAX_PIETY);
            int rare_weight   = 5  + (55 * you.piety / MAX_PIETY);
            int legend_weight = 0  + (35 * you.piety / MAX_PIETY);

            deck_rarity_type rarity = static_cast<deck_rarity_type>(
                random_choose_weighted(common_weight,
                                       DECK_RARITY_COMMON,
                                       rare_weight,
                                       DECK_RARITY_RARE,
                                       legend_weight,
                                       DECK_RARITY_LEGENDARY,
                                       0));

            item_def &deck(mitm[thing_created]);

            deck.special = rarity;
            deck.colour  = deck_rarity_to_color(rarity);

            move_item_to_grid( &thing_created, you.x_pos, you.y_pos );
            origin_acquired(deck, you.religion);

            simple_god_message(" grants you a gift!");
            more();
            canned_msg(MSG_SOMETHING_APPEARS);

            you.attribute[ATTR_CARD_COUNTDOWN] = 10;
            inc_gift_timeout(5 + random2avg(9, 2));
            you.num_gifts[you.religion]++;
            take_note(Note(NOTE_GOD_GIFT, you.religion));
        }
    }
}

static bool promote_to_priest(monsters* mon)
{
    monster_type priest_type = MONS_PROGRAM_BUG;

    // Possible promotions.
    if (mon->type == MONS_ORC)
        priest_type = MONS_ORC_PRIEST;

    if (priest_type != MONS_PROGRAM_BUG)
    {
        // Turn an ordinary monster into a priestly monster, preserving
        // important characteristics.  Keep this as generic as possible,
        // in case more promotions are added.
        const unsigned long old_flags = mon->flags;
        const int old_hp = mon->hit_points;
        const int old_hp_max = mon->max_hit_points;
        const char old_ench_countdown = mon->ench_countdown;
        mon_enchant abj = mon->get_ench(ENCH_ABJ);
        mon_enchant shifter = mon->get_ench(ENCH_GLOWING_SHAPESHIFTER,
                                            ENCH_SHAPESHIFTER);
        const bool old_mon_caught = mons_is_caught(mon);

        mon->type = priest_type;
        define_monster(monster_index(mon));

        mon->flags = old_flags;
        mon->hit_points = mon->max_hit_points *
            ((old_hp * 100) / old_hp_max) / 100;
        mon->ench_countdown = old_ench_countdown;
        mon->add_ench(abj);
        mon->add_ench(shifter);
        if (old_mon_caught)
            mon->add_ench(ENCH_HELD);
        if (mons_class_flag(mon->type, M_INVIS))
            mon->add_ench(ENCH_INVIS);
        mon->fix_speed();

        return true;
    }

    return false;
}

void bless_follower(god_type god,
                    bool (*suitable)(const monsters* mon))
{
    if (!there_are_monsters_nearby())
        return;

    int monster = choose_random_nearby_monster(0, suitable);

    if (monster == NON_MONSTER)
        return;

    monsters* mon = &menv[monster];
    const char *blessed = mon->name(DESC_NOCAP_A).c_str();
    const char *result = NULL;

    switch (random2(100))
    {
        // 5% chance: Turn a monster into a priestly monster, if
        // possible.  This is currently only used for Beogh.
        case 0:
        case 1:
        case 2:
        case 3:
        case 4:
            if (god == GOD_BEOGH && promote_to_priest(mon))
            {
                result = "priesthood";
                break;
            }
            // Deliberate fall through.

        // 95% chance: full healing.
        default:
        {
            bool healing = false;
            bool vigour = true;

            healing = heal_monster(mon, mon->max_hit_points, false);

            if (!healing || coinflip())
            {
                // Full healing, plus one added hit point.
                heal_monster(mon, mon->max_hit_points, true);

                if (coinflip())
                    // Full healing, plus another added hit point.
                    heal_monster(mon, mon->max_hit_points, true);

                vigour = true;
            }

            if (healing && vigour)
                result = "healing and extra vigour";
            else if (healing)
                result = "healing";
            else
                result = "extra vigour";

            break;
        }
    }

    mprf(MSGCH_GOD, "%s blesses %s with %s.", god_name(god).c_str(),
        blessed, result);
}

static void do_god_gift(bool prayed_for)
{
    ASSERT(you.religion != GOD_NO_GOD);

    // Zin worshippers are the only ones that can pray to ask Zin for stuff.
    if (prayed_for != (you.religion == GOD_ZIN))
        return;

    god_acting gdact;

#if DEBUG_DIAGNOSTICS || DEBUG_GIFTS
    int old_gifts = you.num_gifts[ you.religion ];
#endif

    // Consider a gift if we don't have a timeout and weren't
    // already praying when we prayed.
    if (!player_under_penance() && !you.gift_timeout
        || you.religion == GOD_ZIN)
    {
        bool success = false;

        // Remember to check for water/lava
        switch (you.religion)
        {
        default:
            break;

        case GOD_ZIN:
            //jmf: this "good" god will feed you (a la Nethack)
            if (you.hunger_state <= HS_STARVING
                && you.piety >= piety_breakpoint(0))
            {
                god_speaks(you.religion, "Your stomach feels content.");
                set_hunger(6000, true);
                lose_piety(5 + random2avg(10, 2) + (you.gift_timeout? 5 : 0));
                inc_gift_timeout(30 + random2avg(10, 2));
            }
            break;

        case GOD_NEMELEX_XOBEH:
            give_nemelex_gift();
            break;

        case GOD_OKAWARU:
        case GOD_TROG:
            if (you.piety > 130
                && random2(you.piety) > 120
                && !grid_destroys_items(grd[you.x_pos][you.y_pos])
                && one_chance_in(4))
            {
                if (you.religion == GOD_TROG 
                    || (you.religion == GOD_OKAWARU && coinflip()))
                {
                    success = acquirement(OBJ_WEAPONS, you.religion);
                }
                else
                {
                    success = acquirement(OBJ_ARMOUR, you.religion);
                    // Okawaru charges extra for armour acquirements.
                    inc_gift_timeout(20 + random2avg(15, 2));
                }

                if (success)
                {
                    simple_god_message(" has granted you a gift!");
                    more();

                    inc_gift_timeout(30 + random2avg(19, 2));
                    you.num_gifts[ you.religion ]++;
                    take_note(Note(NOTE_GOD_GIFT, you.religion));
                }
                break;
            }

            if (you.religion == GOD_OKAWARU
                && need_missile_gift())
            {
                success = acquirement( OBJ_MISSILES, you.religion );
                if (success)
                {
                    simple_god_message( " has granted you a gift!" );
                    more();

                    inc_gift_timeout( 4 + roll_dice(2,4) );
                    you.num_gifts[ you.religion ]++;
                    take_note(Note(NOTE_GOD_GIFT, you.religion));
                }
                break;
            }
            break;

        case GOD_YREDELEMNUL:
            if (random2(you.piety) > 80 && one_chance_in(5))
            {
                monster_type thing_called =
                    random_servant(GOD_YREDELEMNUL);

                if (create_monster(thing_called, 0, BEH_FRIENDLY,
                                   you.x_pos, you.y_pos, you.pet_target,
                                   MAKE_ITEM_RANDOM_RACE) != -1)
                {
                    simple_god_message(" grants you an undead servant!");
                    more();
                    inc_gift_timeout(4 + random2avg(7, 2));
                    you.num_gifts[you.religion]++;
                    take_note(Note(NOTE_GOD_GIFT, you.religion));
                }
            }
            break;

        case GOD_KIKUBAAQUDGHA:
        case GOD_SIF_MUNA:
        case GOD_VEHUMET:
            if (you.piety > 160 && random2(you.piety) > 100)
            {
                unsigned int gift = NUM_BOOKS;

                switch (you.religion)
                {
                case GOD_KIKUBAAQUDGHA:     // gives death books
                    if (!you.had_book[BOOK_NECROMANCY])
                        gift = BOOK_NECROMANCY;
                    else if (!you.had_book[BOOK_DEATH])
                        gift = BOOK_DEATH;
                    else if (!you.had_book[BOOK_UNLIFE])
                        gift = BOOK_UNLIFE;
                    else if (!you.had_book[BOOK_NECRONOMICON])
                        gift = BOOK_NECRONOMICON;
                    break;

                case GOD_SIF_MUNA:
                    gift = OBJ_RANDOM;      // Sif Muna - gives any
                    break;

                // Vehumet - gives conj/summ. books (higher skill first)
                case GOD_VEHUMET:
                    if (!you.had_book[BOOK_CONJURATIONS_I])
                        gift = give_first_conjuration_book();
                    else if (!you.had_book[BOOK_POWER])
                        gift = BOOK_POWER;
                    else if (!you.had_book[BOOK_ANNIHILATIONS])
                        gift = BOOK_ANNIHILATIONS;  // conj books

                    if (you.skills[SK_CONJURATIONS] <
                                    you.skills[SK_SUMMONINGS]
                        || gift == NUM_BOOKS)
                    {
                        if (!you.had_book[BOOK_CALLINGS])
                            gift = BOOK_CALLINGS;
                        else if (!you.had_book[BOOK_SUMMONINGS])
                            gift = BOOK_SUMMONINGS;
                        else if (!you.had_book[BOOK_DEMONOLOGY])
                            gift = BOOK_DEMONOLOGY; // summoning bks
                    }
                    break;
                default:
                    break;
                }

                if (gift != NUM_BOOKS
                    && !(grid_destroys_items(grd[you.x_pos][you.y_pos])))
                {
                    if (gift == OBJ_RANDOM)
                        success = acquirement(OBJ_BOOKS, you.religion);
                    else
                    {
                        int thing_created = items(1, OBJ_BOOKS, gift, true, 1,
                                                  MAKE_ITEM_RANDOM_RACE);
                        if (thing_created == NON_ITEM)
                            return;

                        move_item_to_grid( &thing_created, you.x_pos, you.y_pos );

                        if (thing_created != NON_ITEM)
                        {
                            success = true;
                            origin_acquired(mitm[thing_created], you.religion);
                        }
                    }

                    if (success)
                    {
                        simple_god_message(" has granted you a gift!");
                        more();

                        inc_gift_timeout(40 + random2avg(19, 2));
                        you.num_gifts[ you.religion ]++;
                        take_note(Note(NOTE_GOD_GIFT, you.religion));
                    }

                    // Vehumet gives books less readily
                    if (you.religion == GOD_VEHUMET && success)
                        inc_gift_timeout(10 + random2(10));
                }                   // end of giving book
            }                       // end of book gods
            break;

        case GOD_BEOGH:
            // Blessings for followers.
            if (you.piety >= piety_breakpoint(2)
                && random2(you.piety) >= piety_breakpoint(0))
            {
                bless_follower(GOD_BEOGH, is_orcish_follower);
            }
            break;
        }
    }                           // end of gift giving

#if DEBUG_DIAGNOSTICS || DEBUG_GIFTS
    if (old_gifts < you.num_gifts[ you.religion ])
        mprf(MSGCH_DIAGNOSTICS, "Total number of gifts from this god: %d",
             you.num_gifts[ you.religion ] );
#endif
}

static bool is_risky_sacrifice(const item_def& item)
{
    return item.base_type == OBJ_ORBS || is_rune(item);
}

static bool confirm_pray_sacrifice()
{
    if (Options.stash_tracking == STM_EXPLICIT
        && is_stash(you.x_pos, you.y_pos))
    {
        mpr("You can't sacrifice explicitly marked stashes.");
        return false;
    }

    for ( int i = igrd[you.x_pos][you.y_pos]; i != NON_ITEM;
          i = mitm[i].link )
    {
        const item_def& item = mitm[i];
        if ( is_risky_sacrifice(item) ||
             has_warning_inscription(item, OPER_PRAY) )
        {
            std::string prompt = "Really sacrifice stack with ";
            prompt += item.name(DESC_NOCAP_A);
            prompt += " in it?";
            if ( !yesno(prompt.c_str(), false, 'n') )
                return false;
        }
    }
    return true;
}

std::string god_prayer_reaction()
{
    std::string result = god_name(you.religion);
    if (!crawl_state.need_save && crawl_state.updating_scores)
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

void pray()
{
    if (silenced(you.x_pos, you.y_pos))
    {
        mpr("You are unable to make a sound!");
        return;
    }

    // almost all prayers take time
    you.turn_is_over = true;

    const bool was_praying = !!you.duration[DUR_PRAYER];

    const god_type altar_god = grid_altar_god(grd[you.x_pos][you.y_pos]);
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
            altar_prayer();
        }
        else if (altar_god != GOD_NO_GOD)
        {
            if (you.species == SP_DEMIGOD)
            {
                you.turn_is_over = false;
                mpr("Sorry, a being of your status cannot worship here.");
                return;
            }
            god_pitch( grid_altar_god(grd[you.x_pos][you.y_pos]) );
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
    else if (you.religion == GOD_XOM)
    {
        mpr("Xom ignores you.");
        return;
    }

    // Nemelexites can abort out now instead of offering something
    // they don't want to lose
    if ( you.religion == GOD_NEMELEX_XOBEH && altar_god == GOD_NO_GOD &&
         !confirm_pray_sacrifice() )
    {
        you.turn_is_over = false;
        return;
    }

    mprf(MSGCH_PRAY, "You %s prayer to %s.",
         was_praying ? "renew your" : "offer a",
         god_name(you.religion).c_str());

    // ...otherwise, they offer what they're standing on
    if ( you.religion == GOD_NEMELEX_XOBEH && altar_god == GOD_NO_GOD )
        offer_items();

    you.duration[DUR_PRAYER] = 9 + (random2(you.piety) / 20)
                                 + (random2(you.piety) / 20);

    if (player_under_penance())
        simple_god_message(" demands penance!");
    else
    {
        mpr( god_prayer_reaction().c_str(), MSGCH_PRAY, you.religion );

        if (you.piety > 130)
            you.duration[DUR_PRAYER] *= 3;
        else if (you.piety > 70)
            you.duration[DUR_PRAYER] *= 2;
    }

    if (you.religion == GOD_NEMELEX_XOBEH)
        you.duration[DUR_PRAYER] = 1;
    
    if (!was_praying)
        do_god_gift(true);

#if DEBUG_DIAGNOSTICS
    mprf(MSGCH_DIAGNOSTICS, "piety: %d (-%d)", you.piety, you.piety_hysteresis );
#endif

}                               // end pray()

std::string god_name( god_type which_god, bool long_name )
{
    switch (which_god)
    {
    case GOD_NO_GOD: return "No God";
    case GOD_RANDOM: return "random";
    case GOD_ZIN: return (long_name ? "Zin the Law-Giver" : "Zin");
    case GOD_SHINING_ONE: return "The Shining One";
    case GOD_KIKUBAAQUDGHA: return "Kikubaaqudgha";
    case GOD_YREDELEMNUL:
        return (long_name ? "Yredelemnul the Dark" : "Yredelemnul");
    case GOD_VEHUMET: return "Vehumet";
    case GOD_OKAWARU: return (long_name ? "Warmaster Okawaru" : "Okawaru");
    case GOD_MAKHLEB: return (long_name ? "Makhleb the Destroyer" : "Makhleb");
    case GOD_SIF_MUNA:
        return (long_name ? "Sif Muna the Loreminder" : "Sif Muna");
    case GOD_TROG: return (long_name ? "Trog the Wrathful" : "Trog");
    case GOD_NEMELEX_XOBEH: return "Nemelex Xobeh";
    case GOD_ELYVILON: return (long_name ? "Elyvilon the Healer" : "Elyvilon");
    case GOD_LUGONU: return (long_name ? "Lugonu the Unformed" : "Lugonu");
    case GOD_BEOGH: return (long_name ? "Beogh the Brigand" : "Beogh");

    case GOD_XOM:
        if (!long_name)
            return "Xom";
        else
        {
            const char* xom_names[] = {
                "Xom the Random", "Xom the Random Number God",
                "Xom the Tricky", "Xom the Less-Predictable",
                "Xom the Unpredictable", "Xom of Many Doors",
                "Xom the Capricious", "Xom of Bloodstained Whimsey",
                "Xom of Enforced Whimsey", "Xom of Bone-Dry Humour",
                "Xom of Malevolent Giggling", "Xom of Malicious Giggling",
                "Xom the Psychotic", "Xom of Gnomic Intent",
                "Xom the Fickle", "Xom of Unknown Intention",
                "The Xom-Meister", "Xom the Begetter of Turbulence",
                "Xom the Begetter of Discontinuities"
            };
            return (one_chance_in(3) ? RANDOM_ELEMENT(xom_names)
                    : "Xom of Chaos");
        }
    case NUM_GODS: return "Buggy";
    }
    return "";
}

void god_speaks( god_type god, const char *mesg )
{
    mpr( mesg, MSGCH_GOD, god );
}                               // end god_speaks()

// This function is the merger of done_good() and naughty().
// Returns true if god was interested (good or bad) in conduct.
bool did_god_conduct( conduct_type thing_done, int level, bool known,
                      const monsters *victim )
{
    bool ret = false;
    int piety_change = 0;
    int penance = 0;

    if (you.religion == GOD_NO_GOD || you.religion == GOD_XOM)
        return (false);

    god_acting gdact;

    switch (thing_done)
    {
    case DID_DRINK_BLOOD:
        switch (you.religion)
        {
        case GOD_SHINING_ONE:
            if (!known)
            {
                simple_god_message(" did not appreciate that!");
                break;
            }
            penance = level;
            // deliberate fall-through
        case GOD_ZIN:
        case GOD_ELYVILON:
            if (!known)
            {
                simple_god_message(" did not appreciate that!");
                break;
            }
            piety_change = -2*level;
            ret = true;
            break;
        default:
            break;
        }
        break;

    case DID_CANNIBALISM:
        switch (you.religion)
        {
        case GOD_ZIN:
        case GOD_SHINING_ONE:
        case GOD_ELYVILON:
            piety_change = -level;
            penance = level;
            ret = true;
            break;
        default:
            break;
        }
        break;

    // If you make some god like these acts, modify did_god_conduct call
    // in beam.cc with god_likes_necromancy check or something similar
    case DID_NECROMANCY:
    case DID_UNHOLY:
    case DID_ATTACK_HOLY:
        switch (you.religion)
        {
        case GOD_ZIN:
        case GOD_SHINING_ONE:
        case GOD_ELYVILON:
            if (!known && thing_done != DID_ATTACK_HOLY)
            {
                simple_god_message(" did not appreciate that!");
                break;
            }
            piety_change = -level;
            if (known || DID_ATTACK_HOLY && victim->attitude != ATT_HOSTILE)
                penance = level * ((you.religion == GOD_SHINING_ONE) ? 2 : 1);
            ret = true;
            break;
        default:
            break;
        }
        break;

    case DID_STABBING:
    case DID_POISON:
        if (you.religion == GOD_SHINING_ONE)
        {
            ret = true;
            piety_change = -level;
            penance = level * 2;
        }
        break;

    case DID_ATTACK_FRIEND:
        if (god_hates_attacking_friend(you.religion, victim))
        {
            piety_change = -level;
            if (known)
                penance = level * 3;
            ret = true;
        }
        break;

    case DID_FRIEND_DIES:
        switch (you.religion)
        {
        case GOD_ELYVILON:
            penance = level;    // healer god cares more about this
            // fall through
        case GOD_ZIN: // in contrast to TSO, who doesn't mind martyrs
        case GOD_OKAWARU:
            piety_change = -level;
            ret = true;
            break;
        default:
            break;
        }
        break;

    case DID_DEDICATED_BUTCHERY:  // aka field sacrifice
        switch (you.religion)
        {
        case GOD_ELYVILON:
            simple_god_message(" did not appreciate that!");
            ret = true;
            piety_change = -10;
            penance = 10;
            break;

        case GOD_OKAWARU:
        case GOD_MAKHLEB:
        case GOD_TROG:
        case GOD_BEOGH:
        case GOD_LUGONU:
            simple_god_message(" accepts your offering.");
            ret = true;
            if (random2(level + 10) > 5)
                piety_change = 1;
            break;

        default:
            break;
        }
        break;

    case DID_KILL_LIVING:
        switch (you.religion)
        {
        case GOD_ELYVILON:
            // killing only disapproved during prayer
            if (you.duration[DUR_PRAYER])
            {
                simple_god_message(" did not appreciate that!");
                ret = true;
                piety_change = -level;
                penance = level * 2;
            }
            break;

        case GOD_KIKUBAAQUDGHA:
        case GOD_YREDELEMNUL:
        case GOD_OKAWARU:
        case GOD_VEHUMET:
        case GOD_MAKHLEB:
        case GOD_TROG:
        case GOD_BEOGH:
        case GOD_LUGONU:
            if (god_hates_attacking_friend(you.religion, victim))
                break;

            simple_god_message(" accepts your kill.");
            ret = true;
            if (random2(level + 18 - you.experience_level / 2) > 5)
                piety_change = 1;
            break;

        default:
            break;
        }
        break;

    case DID_KILL_UNDEAD:
        switch (you.religion)
        {
        case GOD_SHINING_ONE:
        case GOD_OKAWARU:
        case GOD_VEHUMET:
        case GOD_MAKHLEB:
        case GOD_LUGONU:
            if (god_hates_attacking_friend(you.religion, victim))
                break;

            simple_god_message(" accepts your kill.");
            ret = true;
            // Holy gods are easier to please this way
            if (random2(level + 18 - (is_good_god(you.religion) ? 0 :
                                      you.experience_level / 2)) > 4)
                piety_change = 1;
            break;

        default:
            break;
        }
        break;

    case DID_KILL_DEMON:
        switch (you.religion)
        {
        case GOD_SHINING_ONE:
        case GOD_OKAWARU:
        case GOD_MAKHLEB:
            if (god_hates_attacking_friend(you.religion, victim))
                break;

            simple_god_message(" accepts your kill.");
            ret = true;
            // Holy gods are easier to please this way
            if (random2(level + 18 - (is_good_god(you.religion) ? 0 :
                                      you.experience_level / 2)) > 3)
                piety_change = 1;
            break;

        default:
            break;
        }
        break;

    case DID_KILL_NATURAL_EVIL:
        if (you.religion == GOD_SHINING_ONE)
        {
            if (god_hates_attacking_friend(you.religion, victim))
                break;

            simple_god_message(" accepts your kill.");
            ret = true;
            if (random2(level + 18) > 3)
                piety_change = 1;
        }
        break;

    case DID_KILL_MUTATOR_OR_ROTTER:
        if (you.religion == GOD_ZIN)
        {
            if (god_hates_attacking_friend(you.religion, victim))
                break;

            simple_god_message(" appreciates your killing of a "
                               "spawn of chaos.");
            ret = true;
            if (random2(level + 18) > 3)
                piety_change = 1;
        }
        break;

    case DID_KILL_PRIEST:
        if (you.religion == GOD_BEOGH
            && !god_hates_attacking_friend(you.religion, victim))
        {
            simple_god_message(" appreciates your killing of a "
                               "heretic priest.");
            ret = true;
            if (random2(level + 10) > 5)
                piety_change = 1;
        }
        break;

    case DID_KILL_WIZARD:
        if (you.religion == GOD_TROG
            && !god_hates_attacking_friend(you.religion, victim))
        {
            simple_god_message(" appreciates your killing of a magic user.");
            ret = true;
            if (random2(level + 10) > 5)
                piety_change = 1;
        }
        break;

    // Note that holy deaths are special, they are always noticed...
    // if you or any friendly kills one you'll get the credit or the blame.
    case DID_HOLY_KILLED_BY_SERVANT:
    case DID_KILL_HOLY:
        switch (you.religion)
        {
        case GOD_ZIN:
        case GOD_SHINING_ONE:
        case GOD_ELYVILON:
            if (testbits(victim->flags, MF_CREATED_FRIENDLY)
                || testbits(victim->flags, MF_WAS_NEUTRAL))
            {
                level *= 3;
                penance = level * ((you.religion == GOD_ZIN) ? 2 : 1);
            }
            piety_change = -level;
            ret = true;
            break;

        case GOD_KIKUBAAQUDGHA:
        case GOD_YREDELEMNUL:
        case GOD_MAKHLEB:
        case GOD_LUGONU:
            snprintf( info, INFO_SIZE, " accepts your %skill.",
                      (thing_done == DID_KILL_HOLY) ? "" : "collateral " );

            simple_god_message( info );

            ret = true;
            if (random2(level + 18) > 2)
                piety_change = 1;
            break;

        default:
            break;
        }
        break;

    // Undead slave is any friendly undead... Kiku and Yred pay attention
    // to the undead and both like the death of living things.
    case DID_LIVING_KILLED_BY_UNDEAD_SLAVE:
        switch (you.religion)
        {
        case GOD_KIKUBAAQUDGHA:
        case GOD_YREDELEMNUL:
        case GOD_MAKHLEB:
        case GOD_LUGONU:
            simple_god_message(" accepts your slave's kill.");
            ret = true;
            if (random2(level + 10 - you.experience_level/3) > 5)
                piety_change = 1;
            break;
        default:
            break;
        }
        break;

    // Servants are currently any friendly monster under Vehumet, or
    // any god given pet for everyone else (excluding undead which are
    // handled above).
    case DID_LIVING_KILLED_BY_SERVANT:
        switch (you.religion)
        {
        case GOD_KIKUBAAQUDGHA: // note: reapers aren't undead
        case GOD_VEHUMET:
        case GOD_MAKHLEB:
        case GOD_BEOGH:
        case GOD_LUGONU:
            simple_god_message(" accepts your collateral kill.");
            ret = true;
            if (random2(level + 10 - you.experience_level/3) > 5)
                piety_change = 1;
            break;
        default:
            break;
        }
        break;

    case DID_UNDEAD_KILLED_BY_SERVANT:
        switch (you.religion)
        {
        case GOD_SHINING_ONE:
        case GOD_VEHUMET:
        case GOD_MAKHLEB:
        case GOD_LUGONU:
            simple_god_message(" accepts your collateral kill.");
            ret = true;
            if (random2(level + 10 - (is_good_god(you.religion) ? 0 :
                                      you.experience_level/3)) > 5)
                piety_change = 1;
            break;
        default:
            break;
        }
        break;

    case DID_DEMON_KILLED_BY_SERVANT:
        switch (you.religion)
        {
        case GOD_SHINING_ONE:
            simple_god_message(" accepts your collateral kill.");
            ret = true;
            // only holy gods care about this, so no XP level deduction
            if (random2(level + 10) > 5)
                piety_change = 1;
            break;
        default:
            break;
        }
        break;

    case DID_NATURAL_EVIL_KILLED_BY_SERVANT:
        if (you.religion == GOD_SHINING_ONE)
        {
            simple_god_message(" accepts your collateral kill.");
            ret = true;

            if (random2(level + 10) > 5)
                piety_change = 1;
        }
        break;

    case DID_SPELL_MEMORISE:
        if (you.religion == GOD_TROG)
        {
            penance = level * 10;
            piety_change = -penance;
            ret = true;
        }
        break;

    case DID_SPELL_CASTING:
        if (you.religion == GOD_TROG)
        {
            piety_change = -level;
            penance = level * 5;
            ret = true;
        }
        break;

    case DID_SPELL_PRACTISE:
        // Like CAST, but for skill advancement.
        // Level is number of skill points gained... typically 10 * exerise,
        // but may be more/less if the skill is at 0 (INT adjustment), or
        // if the PC's pool is low and makes change.
        if (you.religion == GOD_SIF_MUNA)
        {
            // Old curve: random2(12) <= spell-level, this is similar,
            // but faster at low levels (to help ease things for low level
            // Power averages about (level * 20 / 3) + 10 / 3 now.  Also
            // note that spell skill practise comes just after XP gain, so
            // magical kills tend to do both at the same time (unlike melee).
            // This means high level spells probably work pretty much like
            // they used to (use spell, get piety).
            piety_change = div_rand_round( level + 10, 80 );
#ifdef DEBUG_DIAGNOSTICS
            mprf(MSGCH_DIAGNOSTICS, "Spell practise, level: %d, dpiety: %d",
                    level, piety_change);
#endif
            ret = true;
        }
        break;

    case DID_CARDS:
        if (you.religion == GOD_NEMELEX_XOBEH)
        {
            piety_change = level;
            ret = true;

            // For a stacked deck, 0% chance of card countdown decrement
            // drawing a card which doesn't use up the deck, and 40%
            // on a card which does.  For a non-stacked deck, an
            // average 50% of decrement for drawing a card which doesn't
            // use up the deck, and 80% on a card which does use up the
            // deck.
            int chance = 0;
            switch(level)
            {
            case 0: chance = 0;   break;
            case 1: chance = 40;  break;
            case 2: chance = 70;  break;
            default:
            case 3: chance = 100; break;
            }

            if (random2(100) < chance && you.attribute[ATTR_CARD_COUNTDOWN])
            {
                you.attribute[ATTR_CARD_COUNTDOWN]--;
#if DEBUG_DIAGNOSTICS || DEBUG_CARDS || DEBUG_GIFTS
                mprf(MSGCH_DIAGNOSTICS, "Countdown down to %d",
                     you.attribute[ATTR_CARD_COUNTDOWN]);
#endif
            }
        }
        break;

    case DID_DELIBERATE_MUTATING:
        if (you.religion == GOD_ZIN)
        {
            if (!known)
            {
                simple_god_message(" did not appreciate that!");
                break;
            }
            piety_change = -level;
            ret = true;
        }
        break;

    case DID_EAT_SOULED_BEING:
        if (you.religion == GOD_ZIN)
        {
            piety_change = -level;
            penance = level * 5;
            ret = true;
        }
        break;

    case DID_KILL_NEUTRAL:                      // unused
    case DID_STIMULANTS:                        // unused
    case DID_EAT_MEAT:                          // unused
    case DID_CREATED_LIFE:                      // unused
    case DID_SPELL_NONUTILITY:                  // unused
    case NUM_CONDUCTS:
        break;
    }

    if (piety_change > 0)
        gain_piety( piety_change );
    else 
        dock_piety(-piety_change, penance);

#if DEBUG_DIAGNOSTICS
    if (ret)
    {
        static const char *conducts[] =
        {
          "",
          "Necromancy", "Unholy", "Attack Holy", "Attack Friend",
          "Friend Died", "Stab", "Poison", "Field Sacrifice",
          "Kill Living", "Kill Undead", "Kill Demon", "Kill Natural Evil",
          "Kill Mutator Or Rotter", "Kill Wizard", "Kill Priest",
          "Kill Holy", "Kill Neutral", "Undead Slave Kill Living",
          "Servant Kill Living", "Servant Kill Undead", "Servant Kill Demon",
          "Servant Kill Natural Evil", "Servant Kill Holy",
          "Spell Memorise", "Spell Cast", "Spell Practise", "Spell Nonutility",
          "Cards", "Stimulants", "Drink Blood", "Cannibalism", "Eat Meat",
          "Eat Souled Beings", "Create Life", "Deliberate Mutation"
        };

        COMPILE_CHECK(ARRAYSIZE(conducts) == NUM_CONDUCTS, c1);
        mprf( MSGCH_DIAGNOSTICS,
             "conduct: %s; piety: %d (%+d); penance: %d (%+d)",
             conducts[thing_done],
             you.piety, piety_change, you.penance[you.religion], penance );

    }
#endif

    return (ret);
}

static void dock_piety(int piety_loss, int penance)
{
    static long last_piety_lecture = -1L;
    static long last_penance_lecture = -1L;

    if (piety_loss <= 0 && penance <= 0)
        return;

    if (piety_loss)
    {
        if (last_piety_lecture != you.num_turns)
        {
            // output guilt message:
            mprf( "You feel%sguilty.",
                  (piety_loss == 1) ? " a little " :
                  (piety_loss <  5) ? " " :
                  (piety_loss < 10) ? " very "
                  : " extremely " );
        }

        last_piety_lecture = you.num_turns;
        lose_piety( piety_loss );
    }

    if (you.piety < 1)
        excommunication();
    else if (penance)       // only if still in religion
    {
        if (last_penance_lecture != you.num_turns)
            god_speaks( you.religion,
                        "\"You will pay for your transgression, mortal!\"" );
        last_penance_lecture = you.num_turns;
        inc_penance( penance );
    }
}

void gain_piety(int pgn)
{
    // Xom uses piety differently...
    if (you.religion == GOD_XOM || you.religion == GOD_NO_GOD)
        return;

    // check to see if we owe anything first
    if (you.penance[you.religion] > 0)
    {
        dec_penance(pgn);
        return;
    }
    else if (you.gift_timeout > 0)
    {
        if (you.gift_timeout > pgn)
            you.gift_timeout -= pgn;
        else
            you.gift_timeout = 0;

        // Slow down piety gain to account for the fact that gifts
        // no longer have a piety cost for getting them
        if (!one_chance_in(4))
        {
#if DEBUG_PIETY
            mprf(MSGCH_DIAGNOSTICS, "Piety slowdown due to gift timeout.");
#endif
            return;
        }
    }

    // slow down gain at upper levels of piety
    if (you.religion != GOD_SIF_MUNA)
    {
        if (you.piety >= MAX_PIETY
            || (you.piety > 150 && one_chance_in(3))
            || (you.piety > 100 && one_chance_in(3)))
        {
            do_god_gift(false);
            return;
        }
    }
    else
    {
        // Sif Muna has a gentler taper off because training becomes
        // naturally slower as the player gains in spell skills.
        if ((you.piety >= MAX_PIETY) ||
            (you.piety > 150 && one_chance_in(5)))
        {
            do_god_gift(false);
            return;
        }
    }

    // Apply hysteresis
    {
        // piety_hysteresis is the amount of _loss_ stored up, so this
        // may look backwards.
        const int old_hysteresis = you.piety_hysteresis;
        you.piety_hysteresis = (unsigned char)std::max<int>(
            0, you.piety_hysteresis - pgn);
        const int pgn_borrowed = (old_hysteresis - you.piety_hysteresis);
        pgn -= pgn_borrowed;
#if DEBUG_PIETY
mprf(MSGCH_DIAGNOSTICS, "Piety increasing by %d (and %d taken from hysteresis)", pgn, pgn_borrowed);
#endif
    }

    int old_piety = you.piety;

    you.piety += pgn;
    if (you.piety > MAX_PIETY)
        you.piety = MAX_PIETY;

    for ( int i = 0; i < MAX_GOD_ABILITIES; ++i )
    {
        if ( you.piety >= piety_breakpoint(i) &&
             old_piety < piety_breakpoint(i) )
        {
            take_note(Note(NOTE_GOD_POWER, you.religion, i));
            const char* pmsg = god_gain_power_messages[you.religion][i];
            const char first = pmsg[0];
            if ( first )
            {
                if ( isupper(first) )
                    god_speaks(you.religion, pmsg);
                else
                {
                    snprintf(info, INFO_SIZE, "You can now %s.", pmsg);
                    god_speaks(you.religion, info);
                }
                learned_something_new(TUT_NEW_ABILITY);
            }

            if (you.religion == GOD_SHINING_ONE)
            {
                if (i == 0)
                    mpr("A divine halo surrounds you!");
            }

            // When you gain a piety level, you get another chance to
            // make hostile holy beings neutral.
            if (is_good_god(you.religion))
                moral_beings_attitude_change();
        }
    }

    if (you.religion == GOD_BEOGH)
    {
        // every piety level change also affects AC from orcish gear
        you.redraw_armour_class = true;
    }

    if (you.piety > 160 && old_piety <= 160)
    {
        if ((you.religion == GOD_SHINING_ONE || you.religion == GOD_LUGONU)
            && you.num_gifts[you.religion] == 0)
        {
            simple_god_message( " will now bless your weapon at an altar...once.");
        }

        // When you gain piety of more than 160, you get another chance
        // to make hostile holy beings neutral.
        if (is_good_god(you.religion))
            moral_beings_attitude_change();
    }

    do_god_gift(false);
}

bool is_evil_item(const item_def& item)
{
    bool retval = false;

    switch (item.base_type)
    {
    case OBJ_WEAPONS:
        {
        int item_brand = get_weapon_brand(item);
        int item_eff = item_special_wield_effect(item);

        retval = (is_demonic(item)
             || item.special == SPWPN_SCEPTRE_OF_ASMODEUS
             || item.special == SPWPN_STAFF_OF_DISPATER
             || item.special == SPWPN_SWORD_OF_CEREBOV
             || item_eff == SPWLD_CURSE
             || item_eff == SPWLD_TORMENT
             || item_eff == SPWLD_ZONGULDROK
             || item_brand == SPWPN_DRAINING
             || item_brand == SPWPN_PAIN
             || item_brand == SPWPN_VAMPIRICISM);
        }
        break;
    case OBJ_WANDS:
        retval = (item.sub_type == WAND_DRAINING);
        break;
    case OBJ_SCROLLS:
        retval = (item.sub_type == SCR_TORMENT);
        break;
    case OBJ_BOOKS:
        retval = (item.sub_type == BOOK_NECROMANCY
                    || item.sub_type == BOOK_DEATH
                    || item.sub_type == BOOK_UNLIFE
                    || item.sub_type == BOOK_NECRONOMICON
                    || item.sub_type == BOOK_DEMONOLOGY);
        break;
    case OBJ_STAVES:
        retval = (item.sub_type == STAFF_DEATH
             || item.sub_type == STAFF_DEMONOLOGY);
        break;
    case OBJ_MISCELLANY:
        retval = (item.sub_type == MISC_LANTERN_OF_SHADOWS);
        break;
    default:
        break;
    }

    return retval;
}

bool ely_destroy_weapons()
{
    if (you.religion != GOD_ELYVILON)
        return false;

    god_acting gdact;

    bool success = false;
    int i = igrd[you.x_pos][you.y_pos];
    while (i != NON_ITEM)
    {
        const int next = mitm[i].link;  // in case we can't get it later.

        if (mitm[i].base_type != OBJ_WEAPONS
            && mitm[i].base_type != OBJ_MISSILES
            || item_is_stationary(mitm[i])) // held in a net
        {
            i = next;
            continue;
        }

        const int value = item_value( mitm[i], true );
#ifdef DEBUG_DIAGNOSTICS
        mprf(MSGCH_DIAGNOSTICS, "Destroyed weapon value: %d", value);
#endif

        if (is_evil_item(mitm[i]))
        {
            simple_god_message(" welcomes the destruction of this evil weapon.",
                               GOD_ELYVILON);
            gain_piety(1);
        }
        else
        {
            bool pgain = false;
            if (random2(value) >= random2(250) // artefacts (incl. most randarts)
                || (random2(value) >= random2(100)
                    && one_chance_in(1 + you.piety/50))
                || (mitm[i].base_type == OBJ_WEAPONS
                    && (you.piety < 30 || player_under_penance())))
            {
                pgain = true;
                gain_piety(1);
            }

            std::ostream& strm = msg::streams(MSGCH_GOD);
            strm << mitm[i].name(DESC_CAP_THE);

            if (!pgain)
                strm << " barely";

            if ( mitm[i].quantity == 1 )
                strm << " shimmers and breaks into pieces." << std::endl;
            else
                strm << " shimmer and break into pieces." << std::endl;
        }

        destroy_item(i);
        success = true;
        i = next;
    }

    if (!success)
        mpr("There are no weapons here to destroy!");

    return success;
}

// returns false if invocation fails (no books in sight etc.)
bool trog_burn_books()
{
    if (you.religion != GOD_TROG)
        return (false);

    god_acting gdact;

    int i = igrd[you.x_pos][you.y_pos];
    while (i != NON_ITEM)
    {
        const int next = mitm[i].link;  // in case we can't get it later.

        if (mitm[i].base_type == OBJ_BOOKS
           && mitm[i].sub_type != BOOK_MANUAL
           && mitm[i].sub_type != BOOK_DESTRUCTION)
        {
            mpr("Burning your own feet might not be such a smart idea!");
            return (false);
        }
        i = next;
    }

    int totalpiety = 0;
    for (int xpos = you.x_pos - 8; xpos < you.x_pos + 8; xpos++)
        for (int ypos = you.y_pos - 8; ypos < you.y_pos + 8; ypos++)
        {
             // checked above
             if (xpos == you.x_pos && ypos == you.y_pos)
                 continue;

             // burn only squares in sight
             if (!see_grid(xpos, ypos))
                 continue;

             // if a grid is blocked, books lying there will be ignored
             // allow bombing of monsters
             const int cloud = env.cgrid[xpos][ypos];
             if (grid_is_solid(grd[ xpos ][ ypos ]) ||
//                 mgrd[ xpos ][ ypos ] != NON_MONSTER ||
                 (cloud != EMPTY_CLOUD && env.cloud[cloud].type != CLOUD_FIRE))
             {
                 continue;
             }

             int count = 0;
             int rarity = 0;
             i = igrd[xpos][ypos];
             while (i != NON_ITEM)
             {
                 const int next = mitm[i].link; // in case we can't get it later.

                 if (mitm[i].base_type != OBJ_BOOKS
                     || mitm[i].sub_type == BOOK_MANUAL
                     || mitm[i].sub_type == BOOK_DESTRUCTION)
                 {
                     i = next;
                     continue;
                 }

                 rarity += book_rarity(mitm[i].sub_type);
                 // piety increases by 2 for books never picked up, else by 1
                 if (mitm[i].flags & ISFLAG_DROPPED
                     || mitm[i].flags & ISFLAG_THROWN)
                 {
                     totalpiety++;
                 }
                 else
                     totalpiety += 2;

#ifdef DEBUG_DIAGNOSTICS
                 mprf(MSGCH_DIAGNOSTICS, "Burned book rarity: %d", rarity);
#endif

                 destroy_item(i);
                 count++;
                 i = next;
             }

             if (count)
             {
                  if ( cloud != EMPTY_CLOUD )
                  {
                     // reinforce the cloud
                     mpr( "The fire roars with new energy!" );
                     const int extra_dur = count + random2(rarity/2);
                     env.cloud[cloud].decay += extra_dur * 5;
                     env.cloud[cloud].whose = KC_YOU;
                     continue;
                  }

                  int durat = 4 + count + random2(rarity/2);

                  if (durat > 23)
                      durat = 23;

                  place_cloud( CLOUD_FIRE, xpos, ypos, durat, KC_YOU );

                  mpr(count == 1 ? "The book bursts into flames."
                      : "The books burst into flames.", MSGCH_GOD);
             }

        }

    if (!totalpiety)
    {
         mpr("There are no books in sight to burn!");
         return (false);
    }
    else
    {
         simple_god_message(" is delighted!", GOD_TROG);
         gain_piety(totalpiety);
    }
    return (true);
}

void lose_piety(int pgn)
{
    const int old_piety = you.piety;

    // Apply hysteresis
    {
        const int old_hysteresis = you.piety_hysteresis;
        you.piety_hysteresis = (unsigned char)std::min<int>(
            PIETY_HYSTERESIS_LIMIT, you.piety_hysteresis + pgn);
        const int pgn_borrowed = (you.piety_hysteresis - old_hysteresis);
        pgn -= pgn_borrowed;
#if DEBUG_PIETY
        mprf(MSGCH_DIAGNOSTICS, "Piety decreasing by %d (and %d added to hysteresis)", pgn, pgn_borrowed);
#endif
    }


    if (you.piety - pgn < 0)
        you.piety = 0;
    else
        you.piety -= pgn;

    // Don't bother printing out these messages if you're under
    // penance, you wouldn't notice since all these abilities
    // are withheld.
    if (!player_under_penance() && you.piety != old_piety)
    {
        if (you.piety <= 160 && old_piety > 160 &&
            (you.religion == GOD_SHINING_ONE || you.religion == GOD_LUGONU)
             && you.num_gifts[you.religion] == 0)
            simple_god_message(" is no longer ready to bless your weapon.");

        for ( int i = 0; i < MAX_GOD_ABILITIES; ++i )
        {
            if ( you.piety < piety_breakpoint(i) &&
                 old_piety >= piety_breakpoint(i) )
            {
                const char* pmsg = god_lose_power_messages[you.religion][i];
                const char first = pmsg[0];
                if ( first )
                {
                    if ( isupper(first) )
                        god_speaks(you.religion, pmsg);
                    else
                    {
                        snprintf(info,INFO_SIZE,"You can no longer %s.",pmsg);
                        god_speaks(you.religion, info);
                    }
                }

                if (you.religion == GOD_SHINING_ONE)
                {
                    if (i == 0)
                        mpr("Your divine halo starts to fade.");
                }

                if ( need_water_walking() && !beogh_water_walk() )
                {
                    fall_into_a_pool( you.x_pos, you.y_pos, true,
                                      grd[you.x_pos][you.y_pos] );
                }
            }
        }
    }

    if ( you.religion == GOD_BEOGH )
    {
        // every piety level change also affects AC from orcish gear
        you.redraw_armour_class = true;
    }
}

static bool tso_retribution()
{
    const god_type god = GOD_SHINING_ONE;

    // daevas/cleansing theme
    // doesn't care unless you've gone over to evil
    if ( !is_evil_god(you.religion) )
        return false;

    int punishment = random2(7);

    switch (punishment)
    {
    case 0:
    case 1:
    case 2: // summon daevas (3/7)
    {
        bool success = false;
        int how_many = 1 + random2(you.experience_level / 5) + random2(3);

        for (int i = 0; i < how_many; i++)
            if (create_monster( MONS_DAEVA, 0, BEH_HOSTILE,
                                you.x_pos, you.y_pos, MHITYOU,
                                MONS_PROGRAM_BUG ) != -1)
            {
                success = true;
            }

        simple_god_message( success ?
                            " sends the divine host to punish you "
                            "for your evil ways!" :
                            "'s divine host fails to appear.",
                            god );

        break;
    }
    case 3:
    case 4: // cleansing flame (2/7)
    {
        simple_god_message(" blasts you with cleansing flame!", god);

        bolt beam;
        beam.beam_source = NON_MONSTER;
        beam.type = dchar_glyph(DCHAR_FIRED_BURST);
        beam.damage = calc_dice( 3, 20 + (you.experience_level * 7) / 3 );
        beam.flavour = BEAM_HOLY;
        beam.target_x = you.x_pos;
        beam.target_y = you.y_pos;
        beam.name = "golden flame";
        beam.colour = YELLOW;
        beam.thrower = KILL_MISC;
        beam.aux_source = "The Shining One's cleansing flame";
        beam.ex_size = 2;
        beam.is_tracer = false;
        beam.is_explosion = true;
        explosion(beam);
        break;
    }
    case 5:
    case 6: // either noisiness or silence (2/7)
       if (coinflip())
       {
           simple_god_message(" booms out: \"Turn back to righteousness! REPENT!\"", god);
           noisy( 25, you.x_pos, you.y_pos ); // same as scroll of noise
       }
       else
       {
           god_speaks(god, "You feel The Shining One's silent rage upon you!");
           cast_silence( 25 );
       }
       break;
    }
    return false;
}

static bool zin_retribution()
{
    const god_type god = GOD_ZIN;

    // angels/creeping doom theme
    // doesn't care unless you've gone over to evil
    if (!is_evil_god(you.religion))
        return false;

    int punishment = random2(8);

    // if little mutated, do something else instead
    if (punishment < 2 && count_mutations() <= random2(3))
        punishment = random2(6)+2;

    switch(punishment)
    {
    case 0: // remove good mutations (25%)
    case 1:
    {
       simple_god_message(" draws some chaos from your body!", god);
       bool success = false;
       for (int i = 0; i < 7; i++)
            if (random2(10) > i
                && delete_mutation(RANDOM_MUTATION, true, true))
            {
                success = true;
            }

       if (success && !count_mutations())
       {
           simple_god_message(" rids your body of chaos!");
           // lower penance a bit more for being particularly successful
           dec_penance(god, 1);
       }

       break;
    }
    case 2:
    case 3:
    case 4: // summon angels or bugs (pestilence), 3/8
       if (random2(you.experience_level) > 7 && !one_chance_in(5))
       {
           const int how_many = 1 + (you.experience_level / 10) + random2(3);
           bool success = false;

           for (int i = 0; i < how_many; i++)
              if (create_monster(MONS_ANGEL, 0, BEH_HOSTILE,
                                 you.x_pos, you.y_pos, MHITYOU,
                                 MONS_PROGRAM_BUG) != -1)
              {
                  success = true;
              }

           simple_god_message( success ?
                              " sends the divine host to punish you "
                              "for your evil ways!" :
                              "'s divine host fails to appear.",
                              god);
       }
       else
       {
          // god_gift == false gives unfriendly
          bool success = summon_swarm( you.experience_level * 20, true, false );
          simple_god_message(success ?
                             " sends a plague down upon you!" :
                             "'s plague fails to arrive.",
                             god);
       }
       break;
    case 5:
    case 6: // famine, 25%
       simple_god_message(" sends a famine down upon you!", god);
       make_hungry( you.hunger/2, false );
       break;
    case 7: // noisiness, 12.5%
       simple_god_message(" booms out: \"Return to the light! REPENT!\"", god);
       noisy( 25, you.x_pos, you.y_pos ); // same as scroll of noise
       break;
    } // plus Smiting?
    return false;
}

static bool makhleb_retribution()
{
    const god_type god = GOD_MAKHLEB;

     // demonic servant theme
    if (random2(you.experience_level) > 7 && !one_chance_in(5))
    {
        const bool success = create_monster(MONS_EXECUTIONER + random2(5), 0,
                                            BEH_HOSTILE, you.x_pos, you.y_pos,
                                            MHITYOU, MONS_PROGRAM_BUG) != -1;
        simple_god_message(success ?
                           " sends a greater servant after you!" :
                           "'s greater servant is unavoidably detained.",
                           god);
    }
    else
    {
        bool success = false;
        int how_many = 1 + (you.experience_level / 7);

        for (int i = 0; i < how_many; i++)
            if (create_monster(MONS_NEQOXEC + random2(5), 0, BEH_HOSTILE,
                               you.x_pos, you.y_pos, MHITYOU,
                               MONS_PROGRAM_BUG) != -1)
                success = true;

        simple_god_message(success ?
                           " sends minions to punish you." :
                           "'s minions fail to arrive.",
                           god);
    }

    return true;
}

static bool kikubaaqudgha_retribution()
{
    const god_type god = GOD_KIKUBAAQUDGHA;

    // death/necromancy theme
    if (random2(you.experience_level) > 7 && !one_chance_in(5))
    {
        bool success = false;
        int how_many = 1 + (you.experience_level / 5) + random2(3);

        for (int i = 0; i < how_many; i++)
            if (create_monster(MONS_REAPER, 0, BEH_HOSTILE, you.x_pos,
                               you.y_pos, MHITYOU, MONS_PROGRAM_BUG) != -1)
                success = true;

        if (success)
            simple_god_message(" unleashes Death upon you!", god);
        else
            god_speaks(god, "Death has been delayed...for now.");
    }
    else
    {
        god_speaks(god, (coinflip()) ? "You hear Kikubaaqudgha cackling."
                   : "Kikubaaqudgha's malice focuses upon you.");

        miscast_effect(SPTYP_NECROMANCY, 5 + you.experience_level,
                       random2avg(88, 3), 100, "the malice of Kikubaaqudgha");
    }

    return true;
}

static bool yredelemnul_retribution()
{
    const god_type god = GOD_YREDELEMNUL;

    // undead theme
    if (random2(you.experience_level) > 4)
    {
        int count = 0;
        int how_many = 1 + random2(1 + (you.experience_level / 5));

        for (int i = 0; i < how_many; i++)
        {
            monster_type punisher = random_servant(GOD_YREDELEMNUL);

            if (create_monster(punisher, 0, BEH_HOSTILE,
                               you.x_pos, you.y_pos, MHITYOU,
                               MONS_PROGRAM_BUG) != -1)
                count++;
        }

        simple_god_message(count > 1? " sends servants to punish you." :
                           count > 0? " sends a servant to punish you." :
                                      "'s servants fail to arrive.", god);
    }
    else
    {
        simple_god_message("'s anger turns toward you for a moment.", god);
        miscast_effect( SPTYP_NECROMANCY, 5 + you.experience_level,
                        random2avg(88, 3), 100, "the anger of Yredelemnul" );
    }

    return true;
}

static bool trog_retribution()
{
    const god_type god = GOD_TROG;

    // physical/berserk theme
    if ( coinflip() )
    {
        int count = 0;
        int points = 3 + you.experience_level * 3;

        {
            no_messages msg;

            while (points > 0)
            {
                int cost = random2(8) + 3;

                if (cost > points)
                    cost = points;

                // quick reduction for large values
                if (points > 20 && coinflip())
                {
                    points -= 10;
                    cost = 10;
                }

                points -= cost;

                if (summon_berserker(cost * 20, false))
                    count++;
            }
        }

        simple_god_message(count > 1 ? " sends monsters to punish you." :
                           count > 0 ? " sends a monster to punish you." :
                           " has no time to punish you... now.", god);
    }
    else if ( !one_chance_in(3) )
    {
        simple_god_message("'s voice booms out, \"Feel my wrath!\"", god );

        // A collection of physical effects that might be better
        // suited to Trog than wild fire magic... messages could
        // be better here... something more along the lines of apathy
        // or loss of rage to go with the anti-berserk effect-- bwr
        switch (random2(6))
        {
        case 0:
            potion_effect( POT_DECAY, 100 );
            break;

        case 1:
        case 2:
            lose_stat(STAT_STRENGTH, 1 + random2(you.strength / 5), true,
                      "divine retribution from Trog");
            break;

        case 3:
            if (!you.duration[DUR_PARALYSIS])
            {
                dec_penance(god, 3);
                mpr( "You suddenly pass out!", MSGCH_WARN );
                you.duration[DUR_PARALYSIS] = 2 + random2(6);
            }
            break;

        case 4:
        case 5:
            if (you.duration[DUR_SLOW] < 90)
            {
                dec_penance(god, 1);
                mpr( "You suddenly feel exhausted!", MSGCH_WARN );
                you.duration[DUR_EXHAUSTED] = 100;
                slow_player( 100 );
            }
            break;
        }
    }
    else
    {
        //jmf: returned Trog's old Fire damage
        // -- actually, this function partially exists to remove that,
        //    we'll leave this effect in, but we'll remove the wild
        //    fire magic. -- bwr
        dec_penance(god, 2);
        mpr( "You feel Trog's fiery rage upon you!", MSGCH_WARN );
        miscast_effect( SPTYP_FIRE, 8 + you.experience_level,
                        random2avg(98, 3), 100, "the fiery rage of Trog" );
    }

    return true;
}

static bool beogh_retribution()
{
    const god_type god = GOD_BEOGH;

    // orcish theme
    switch (random2(8))
    {
    case 0: // smiting (25%)
    case 1:
        god_smites_you(GOD_BEOGH, KILLED_BY_BEOGH_SMITING);
        break;

    case 2: // send out one or two dancing weapons of orc slaying (12.5%)
    {
        int num_created = 0;
        int num_to_create = (coinflip() ? 1 : 2);
        for (int i = 0; i < num_to_create; i++)
        {
            // first create item
            int slot = items(0, OBJ_WEAPONS, WPN_CLUB + random2(13),
                             true, you.experience_level, MAKE_ITEM_NO_RACE);

            if ( slot == -1 )
                continue;

            item_def& item = mitm[slot];
            set_item_ego_type( item, OBJ_WEAPONS, SPWPN_ORC_SLAYING );

            // manually override plusses
            item.plus = random2(3);
            item.plus2 = random2(3);

            if (coinflip())
                item.flags |= ISFLAG_CURSED;

            // let the player see that he's being attacked by
            // orc slayers
            set_ident_flags( item, ISFLAG_KNOW_TYPE );

            // now create monster
            int mons = create_monster( MONS_DANCING_WEAPON, 0, BEH_HOSTILE,
                                       you.x_pos, you.y_pos, MHITYOU,
                                       MONS_PROGRAM_BUG );

            // hand item information over to monster
            if (mons != -1)
            {
                // destroy the old weapon
                // arguably we should use destroy_item() here
                mitm[menv[mons].inv[MSLOT_WEAPON]].clear();

                menv[mons].inv[MSLOT_WEAPON] = slot;
                num_created++;

                // 50% chance of weapon disappearing on "death"
                if (coinflip())
                    menv[mons].flags |= MF_HARD_RESET;
            }
            else // didn't work out! delete item
            {
                mitm[slot].clear();
            }
        }
        if (num_created)
        {
            snprintf(info, INFO_SIZE, " throws %s of orc slaying at you.",
                     num_created > 1 ? "implements" : "an implement");
            simple_god_message(info, god);
            break;
        } // else fall through
    }
    case 4: // 25%, relatively harmless
    case 5: // in effect, only for penance
        if (you.religion == GOD_BEOGH && beogh_followers_abandon_you())
            break;
        // else fall through
    default: // send orcs after you (3/8 to 5/8)
    {
        const int points = you.experience_level + 3
            + random2(you.experience_level * 3);

        monster_type punisher;
        // "natural" bands
        if (points >= 30) // min: lvl 7, always: lvl 27
            punisher = MONS_ORC_WARLORD;
        else if (points >= 24) // min: lvl 6, always: lvl 21
            punisher = MONS_ORC_HIGH_PRIEST;
        else if (points >= 18) // min: lvl 4, always: lvl 15
            punisher = MONS_ORC_KNIGHT;
        else if (points > 10) // min: lvl 3, always: lvl 8
            punisher = MONS_ORC_WARRIOR;
        else
            punisher = MONS_ORC;

        bool success =
            (create_monster(punisher, 0, BEH_HOSTILE, you.x_pos,
                            you.y_pos, MHITYOU,
                            MONS_PROGRAM_BUG, true) != -1);

        simple_god_message(success ?
                           " sends forth an army of orcs." :
                           " is still gathering forces against you.",
                           god);
    }
    }

    return true;
}

static bool okawaru_retribution()
{
    const god_type god = GOD_OKAWARU;

    // warrior theme
    bool success = false;
    const int how_many = 1 + (you.experience_level / 5);

    for (int i = 0; i < how_many; i++)
    {
        monster_type punisher = random_servant(GOD_OKAWARU);

        if (create_monster(punisher, 0, BEH_HOSTILE,
                           you.x_pos, you.y_pos, MHITYOU,
                           MONS_PROGRAM_BUG) != -1)
            success = true;
    }

    simple_god_message(success ?
                       " sends forces against you!" :
                       "'s forces are busy with other wars.",
                       god);

    return true;
}

static bool sif_muna_retribution()
{
    const god_type god = GOD_SIF_MUNA;

    simple_god_message("'s wrath finds you.", god);
    dec_penance(god, 1);

    // magic and intelligence theme
    switch (random2(10))
    {
    case 0:
    case 1:
        lose_stat(STAT_INTELLIGENCE, 1 + random2( you.intel / 5 ), true,
                  "divine retribution from Sif Muna");
        break;

    case 2:
    case 3:
    case 4:
        confuse_player( 3 + random2(10), false );
        break;

    case 5:
    case 6:
        miscast_effect(SPTYP_DIVINATION, 9, 90, 100, "the will of Sif Muna");
        break;

    case 7:
    case 8:
        if (you.magic_points)
        {
            dec_mp( 100 );  // this should zero it.
            mpr( "You suddenly feel drained of magical energy!", MSGCH_WARN );
        }
        break;

    case 9:
        // This will set all the extendable duration spells to
        // a duration of one round, thus potentially exposing
        // the player to real danger.
        antimagic();
        mpr( "You sense a dampening of magic.", MSGCH_WARN );
        break;
    }

    return true;
}

static bool lugonu_retribution()
{
    const god_type god = GOD_LUGONU;

    if (coinflip())
    {
        simple_god_message("'s wrath finds you!", god);
        miscast_effect( SPTYP_TRANSLOCATION, 9, 90, 100, "Lugonu's touch" );

        // No return - Lugonu's touch is independent of other effects.
    }
    else if (coinflip())
    {
        // Give extra opportunities for embarrassing teleports.
        simple_god_message("'s wrath finds you!", god);
        mpr("Space warps around you!");
        if (!one_chance_in(3))
            you_teleport_now(false);
        else
            random_blink(false);

        // No return.
    }

    // abyssal servant theme
    if (random2(you.experience_level) > 7 && !one_chance_in(5))
    {
        if (create_monster(MONS_GREEN_DEATH + random2(3), 0,
                           BEH_HOSTILE, you.x_pos, you.y_pos,
                           MHITYOU, MONS_PROGRAM_BUG) != -1)
        {
            simple_god_message(" sends a demon after you!", god);
        }
        else
        {
            simple_god_message("'s demon is unavoidably detained.", god);
        }
    }
    else
    {
        bool success = false;
        int how_many = 1 + (you.experience_level / 7);

        for (int loopy = 0; loopy < how_many; loopy++)
        {
            if (create_monster(MONS_NEQOXEC + random2(5), 0, BEH_HOSTILE,
                               you.x_pos, you.y_pos, MHITYOU,
                               MONS_PROGRAM_BUG) != -1)
            {
                success = true;
            }
        }

        if (success)
            simple_god_message(" sends minions to punish you.", god);
        else
            simple_god_message("'s minions fail to arrive.", god);
    }

    return false;
}

static bool vehumet_retribution()
{
    const god_type god = GOD_VEHUMET;

    // conjuration and summoning theme
    simple_god_message("'s vengeance finds you.", god);
    miscast_effect( coinflip() ? SPTYP_CONJURATION : SPTYP_SUMMONING,
                    8 + you.experience_level, random2avg(98, 3), 100,
                    "the wrath of Vehumet" );
    return true;
}

static bool nemelex_retribution()
{
    const god_type god = GOD_NEMELEX_XOBEH;
    // like Xom, this might actually help the player -- bwr
    simple_god_message(" makes you draw from the Deck of Punishment.", god);
    draw_from_deck_of_punishment();
    return true;
}

static void ely_destroy_inventory_weapon()
{
    int count = 0;
    int item  = ENDOFPACK;

    for (int i = 0; i < ENDOFPACK; i++)
    {
         if (!is_valid_item( you.inv[i] ))
             continue;

         if (you.inv[i].base_type == OBJ_WEAPONS
             || you.inv[i].base_type == OBJ_MISSILES)
         {
             if (is_artefact(you.inv[i]))
                 continue;

             // item is valid for destroying, so give it a chance
             count++;
             if (one_chance_in( count ))
                 item = i;
         }
    }

    // any item to destroy?
    if (item == ENDOFPACK)
        return;

    int value = 1;
    bool wielded = false;
    
    // increase value wielded weapons or large stacks of ammo
    if (you.inv[item].base_type == OBJ_WEAPONS
        && you.inv[item].link == you.equip[EQ_WEAPON])
    {
        wielded = true;
        value += 2;
    }
    else if (you.inv[item].quantity > random2(you.penance[GOD_ELYVILON]))
        value += 1 + random2(2);

    std::ostream& strm = msg::streams(MSGCH_GOD);
    strm << you.inv[item].name(DESC_CAP_YOUR);

    if (value == 1)
        strm << " barely";

    if ( you.inv[item].quantity == 1 )
         strm << " shimmers and breaks into pieces." << std::endl;
    else
         strm << " shimmer and break into pieces." << std::endl;

    if (wielded)
    {
        unwield_item(true);
        you.wield_change = true;
    }
    // just in case
    you.quiver_change = true;
    
    destroy_item(you.inv[item]);
    burden_change();

    dec_penance(GOD_ELYVILON, value);
}

// comparatively lenient
static bool elyvilon_retribution()
{
    const god_type god = GOD_ELYVILON;

    simple_god_message("'s displeasure finds you.", god);

    // healing theme and interfering with fighting
    switch (random2(5))
    {
    case 0:
    case 1:
        confuse_player( 3 + random2(10), false );
        break;

    case 2: // mostly flavour messages
        miscast_effect(SPTYP_POISON, 0, 0, one_chance_in(3),
                       "the will of Elyvilon");
        break;
        
    case 3:
    case 4: // destroy weapons in your inventory
        ely_destroy_inventory_weapon();
        break;
    }
    
    return true;
}

void divine_retribution( god_type god )
{
    ASSERT(god != GOD_NO_GOD);

    // Good gods don't use divine retribution on their followers, they
    // will consider it for those who have gone astray however.
    if (god == you.religion && is_good_god(god) )
        return;

    god_acting gdact(god, true);

    bool do_more = true;
    switch (god)
    {
    // One in ten chance that Xom might do something good...
    case GOD_XOM: xom_acts(one_chance_in(10), abs(you.piety - 100)); break;
    case GOD_SHINING_ONE:   do_more = tso_retribution(); break;
    case GOD_ZIN:           do_more = zin_retribution(); break;
    case GOD_MAKHLEB:       do_more = makhleb_retribution(); break;
    case GOD_KIKUBAAQUDGHA: do_more = kikubaaqudgha_retribution(); break;
    case GOD_YREDELEMNUL:   do_more = yredelemnul_retribution(); break;
    case GOD_TROG:          do_more = trog_retribution(); break;
    case GOD_BEOGH:         do_more = beogh_retribution(); break;
    case GOD_OKAWARU:       do_more = okawaru_retribution(); break;
    case GOD_LUGONU:        do_more = lugonu_retribution(); break;
    case GOD_VEHUMET:       do_more = vehumet_retribution(); break;
    case GOD_NEMELEX_XOBEH: do_more = nemelex_retribution(); break;
    case GOD_SIF_MUNA:      do_more = sif_muna_retribution(); break;
    case GOD_ELYVILON:      do_more = elyvilon_retribution(); break;

    default:
        do_more = false;
        break;
    }

    // Sometimes divine experiences are overwhelming...
    if (do_more && one_chance_in(5) && you.experience_level < random2(37))
    {
        if (coinflip())
        {
            mpr( "The divine experience confuses you!", MSGCH_WARN);
            confuse_player( 3 + random2(10) );
        }
        else
        {
            if (you.duration[DUR_SLOW] < 90)
            {
                mpr( "The divine experience leaves you feeling exhausted!",
                     MSGCH_WARN );

                slow_player( random2(20) );
            }
        }
    }

    // Just the thought of retribution mollifies the god by at least a
    // point...the punishment might have reduced penance further.
    dec_penance( god, 1 + random2(3) );
}

static bool moral_beings_on_level_attitude_change()
{
    bool success = false;

    for ( int i = 0; i < MAX_MONSTERS; ++i )
    {
        monsters *monster = &menv[i];
        if (monster->type != -1)
        {
#ifdef DEBUG_DIAGNOSTICS
            mprf(MSGCH_DIAGNOSTICS, "Attitude changing: %s on level %d, branch %d",
                 monster->name(DESC_PLAIN).c_str(),
                 static_cast<int>(you.your_level),
                 static_cast<int>(you.where_are_you));
#endif

            if (mons_is_holy(monster) && is_good_god(you.religion))
            {
                // If you worship a good god, you get another chance to
                // make hostile holy beings neutral.
                if (monster->attitude == ATT_HOSTILE &&
                    (monster->flags & MF_ATT_CHANGE_ATTEMPT))
                {
                    monster->flags &= ~MF_ATT_CHANGE_ATTEMPT;

                    success = true;
                }
            }
            // If you don't worship a good god, you make all
            // non-hostile holy beings hostile.  If you do worship a
            // good god, you make all non-hostile evil and unholy beings
            // hostile.
            else if ((mons_is_holy(monster) &&
                        !is_good_god(you.religion)) ||
                    (mons_is_evil_or_unholy(monster) &&
                        is_good_god(you.religion)))
            {
                if (monster->attitude != ATT_HOSTILE)
                {
                    monster->attitude = ATT_HOSTILE;
                    behaviour_event(monster, ME_ALERT, MHITYOU);
                    // for now CREATED_FRIENDLY/WAS_NEUTRAL stays

                    success = true;
                }
            }
        }
    }

    return success;
}

static bool moral_beings_attitude_change()
{
    return apply_to_all_dungeons(moral_beings_on_level_attitude_change);
}

// Make summoned (temporary) friendly god gifts disappear on penance
// or when abandoning the god in question.  If seen, only count monsters
// where the player can see the change, and output a message.
static bool make_god_gifts_on_level_disappear(bool seen = false)
{
    int count = 0;
    for ( int i = 0; i < MAX_MONSTERS; ++i )
    {
        monsters *monster = &menv[i];
        if (monster->type != -1
            && monster->attitude == ATT_FRIENDLY
            && monster->has_ench(ENCH_ABJ)
            && (monster->flags & MF_GOD_GIFT))
        {
            if (!seen || simple_monster_message(monster, " abandons you!"))
                count++;

            // monster disappears
            monster_die(monster, KILL_DISMISSED, 0);
        }
    }
    return (count);
}

static bool god_gifts_disappear_wrapper()
{
    return (make_god_gifts_on_level_disappear());
}

// Make friendly god gifts disappear on all levels, or on only the
// current one.
static bool make_god_gifts_disappear(bool level_only)
{
   bool success = make_god_gifts_on_level_disappear(true);

   if (level_only)
       return (success);

   return (apply_to_all_dungeons(god_gifts_disappear_wrapper) || success);
}

// When abandoning the god in question, turn friendly god gifts neutral.
// If seen, only count monsters where the player can see the change, and
// output a message.
static bool make_god_gifts_on_level_neutral(bool seen = false)
{
    int count = 0;
    for ( int i = 0; i < MAX_MONSTERS; ++i )
    {
        monsters *monster = &menv[i];
        if (monster->type != -1
            && monster->attitude == ATT_FRIENDLY
            && (monster->flags & MF_GOD_GIFT))
        {
            // monster changes attitude
            monster->attitude = ATT_NEUTRAL;

            if (!seen || simple_monster_message(monster, " becomes indifferent."))
                count++;
        }
    }
    return (count);
}

static bool god_gifts_neutral_wrapper()
{
    return (make_god_gifts_on_level_neutral());
}

// Make friendly god gifts turn neutral on all levels, or on only the
// current one.
static bool make_god_gifts_neutral(bool level_only)
{
    bool success = make_god_gifts_on_level_neutral(true);

    if (level_only)
        return (success);

    return (apply_to_all_dungeons(god_gifts_neutral_wrapper) || success);
}

// When abandoning the god in question, turn friendly god gifts hostile.
// If seen, only count monsters where the player can see the change, and
// output a message.
static bool make_god_gifts_on_level_hostile(bool seen = false)
{
    int count = 0;
    for ( int i = 0; i < MAX_MONSTERS; ++i )
    {
        monsters *monster = &menv[i];
        if (monster->type != -1
            && monster->attitude == ATT_FRIENDLY
            && (monster->flags & MF_GOD_GIFT))
        {
            // monster changes attitude and behaviour
            monster->attitude = ATT_HOSTILE;
            behaviour_event(monster, ME_ALERT, MHITYOU);

            if (!seen || simple_monster_message(monster, " turns against you!"))
                count++;
        }
    }
    return (count);
}

static bool god_gifts_hostile_wrapper()
{
    return (make_god_gifts_on_level_hostile());
}

// Make friendly god gifts turn hostile on all levels, or on only the
// current one.
static bool make_god_gifts_hostile(bool level_only)
{
    bool success = make_god_gifts_on_level_hostile(true);

    if (level_only)
        return (success);

    return (apply_to_all_dungeons(god_gifts_hostile_wrapper) || success);
}

static bool orcish_followers_on_level_abandon_you()
{
    bool success = false;

    for ( int i = 0; i < MAX_MONSTERS; ++i )
    {
        monsters *monster = &menv[i];
        if (monster->type != -1
            && is_orcish_follower(monster))
        {
#ifdef DEBUG_DIAGNOSTICS
            mprf(MSGCH_DIAGNOSTICS, "Abandoning: %s on level %d, branch %d",
                 monster->name(DESC_PLAIN).c_str(),
                 static_cast<int>(you.your_level),
                 static_cast<int>(you.where_are_you));
#endif

            monster->attitude = ATT_HOSTILE;
            behaviour_event(monster, ME_ALERT, MHITYOU);
            // for now CREATED_FRIENDLY stays

            success = true;
        }
    }

    return success;
}

// Upon excommunication, ex-Beoghites lose all their orcish followers.
// When under penance, Beoghites can lose all orcish followers in sight,
// subject to a few limitations.
static bool beogh_followers_abandon_you()
{
    bool reconvert = false;
    int num_reconvert = 0;
    int num_followers = 0;

    if (you.religion != GOD_BEOGH)
    {
        reconvert =
            apply_to_all_dungeons(orcish_followers_on_level_abandon_you);
    }
    else
    {
        int ystart = you.y_pos - 9, xstart = you.x_pos - 9;
        int yend = you.y_pos + 9, xend = you.x_pos + 9;
        if ( xstart < 0 ) xstart = 0;
        if ( ystart < 0 ) ystart = 0;
        if ( xend >= GXM ) xend = GXM;
        if ( yend >= GYM ) yend = GYM;

        // monster check
        for ( int y = ystart; y < yend; ++y )
        {
            for ( int x = xstart; x < xend; ++x )
            {
                const unsigned short targ_monst = mgrd[x][y];
                if ( targ_monst != NON_MONSTER )
                {
                    monsters *monster = &menv[targ_monst];
                    if (is_orcish_follower(monster))
                    {
                        num_followers++;

                        if (mons_player_visible(monster)
                            && !mons_is_sleeping(monster)
                            && !mons_is_confused(monster)
                            && !mons_is_paralysed(monster))
                        {
                            const int hd = monster->hit_dice;

                            // during penance followers get a saving throw
                            if (random2((you.piety-you.penance[GOD_BEOGH])/18) +
                                random2(you.skills[SK_INVOCATIONS]-6)
                                  > random2(hd) + hd + random2(5))
                            {
                                continue;
                            }

                            monster->attitude = ATT_HOSTILE;
                            behaviour_event(monster, ME_ALERT, MHITYOU);
                            // for now CREATED_FRIENDLY stays

                            if (player_monster_visible(monster))
                                num_reconvert++; // only visible ones

                            reconvert = true;
                        }
                    }
                }
            }
        }
    }

    if (reconvert) // maybe all of them invisible
    {
        simple_god_message("'s voice booms out, \"Who do you think you "
                           "are?\"", GOD_BEOGH);

        std::ostream& chan = msg::streams(MSGCH_MONSTER_ENCHANT);

        if (you.religion != GOD_BEOGH)
            chan << "All of your followers decide to abandon you.";
        else if (num_reconvert > 0)
        {
            if (num_reconvert == 1 && num_followers > 1)
                chan << "One of your followers decides to abandon you.";
            else
            {
                if (num_reconvert == num_followers)
                    chan << "Your";
                else
                    chan << "Some of your";
                chan << " followers decide to abandon you.";
            }
        }

        chan << std::endl;

        return true;
    }

    return false;
}

// currently only used when orcish idols have been destroyed
static const char* _get_beogh_speech(const std::string key)
{
    std::string result = getSpeakString("Beogh " + key);

    if (!result.empty())
        return (result.c_str());

    return ("Beogh is angry!");
}

// Destroying orcish idols (a.k.a. idols of Beogh) may anger Beogh.
void beogh_idol_revenge()
{
    god_acting gdact(GOD_BEOGH, true);

    // Beogh watches his charges closely, but for others doesn't always notice
    if (you.religion == GOD_BEOGH
        || (you.species == SP_HILL_ORC && coinflip())
        || one_chance_in(3))
    {
        const char *revenge;

        if (you.religion == GOD_BEOGH)
            revenge = _get_beogh_speech("idol follower");
        else if (you.species == SP_HILL_ORC)
            revenge = _get_beogh_speech("idol hill orc");
        else
            revenge = _get_beogh_speech("idol other");

        god_smites_you(GOD_BEOGH, KILLED_BY_BEOGH_SMITING, revenge);

        if (you.religion == GOD_BEOGH)
        {
            // count this as an attack on a fellow orc; it comes closest
            // and gives the same result (penance + piety loss)
            monsters dummy;
            dummy.type = MONS_ORC;
            dummy.attitude = ATT_FRIENDLY;

            did_god_conduct(DID_ATTACK_FRIEND, 8, true,
                            static_cast<const monsters *>(&dummy));
        }
    }
}

static void _print_good_god_neutral_holy_being_speech(const std::string key,
                                                      monsters *mon,
                                                      msg_channel_type channel)
{
    std::string msg = getSpeakString("good_god_neutral_holy_being_" + key);

    if (!msg.empty())
    {
        msg = do_mon_str_replacements(msg, mon);
        mpr(msg.c_str(), channel);
    }
}



void good_god_holy_attitude_change(monsters *holy)
{
    ASSERT(mons_is_holy(holy));

    if (player_monster_visible(holy)) // show reaction
    {
        _print_good_god_neutral_holy_being_speech("reaction", holy,
                                                 MSGCH_MONSTER_ENCHANT);

        if (!one_chance_in(3))
            _print_good_god_neutral_holy_being_speech("speech", holy,
                                                      MSGCH_TALK);
    }

    holy->attitude  = ATT_NEUTRAL;

    // not really *created* neutral, but should it become
    // hostile later on, it won't count as a good kill
    holy->flags |= MF_WAS_NEUTRAL;

    // to avoid immobile "followers"
    behaviour_event(holy, ME_ALERT, MHITNOT);
}

static void _print_converted_orc_speech(const std::string key,
                                        monsters *mon,
                                        msg_channel_type channel)
{
    std::string msg = getSpeakString("beogh_converted_orc_" + key);

    if (!msg.empty())
    {
        msg = do_mon_str_replacements(msg, mon);
        mpr(msg.c_str(), channel);
    }
}

void beogh_convert_orc(monsters *orc, bool emergency,
                       bool converted_by_follower)
{
    ASSERT(mons_species(orc->type) == MONS_ORC);

    if (player_monster_visible(orc)) // show reaction
    {
        if (emergency || !orc->alive())
        {
            if (converted_by_follower)
            {
                _print_converted_orc_speech("reaction_battle_follower", orc,
                                            MSGCH_MONSTER_ENCHANT);
                _print_converted_orc_speech("speech_battle_follower", orc,
                                            MSGCH_TALK);
            }
            else
            {
                _print_converted_orc_speech("reaction_battle", orc,
                                            MSGCH_MONSTER_ENCHANT);
                _print_converted_orc_speech("speech_battle", orc, MSGCH_TALK);
            }
        }
        else
        {
            _print_converted_orc_speech("reaction_sight", orc,
                                        MSGCH_MONSTER_ENCHANT);

            if (!one_chance_in(3))
                _print_converted_orc_speech("speech_sight", orc, MSGCH_TALK);
        }
    }

    orc->attitude  = ATT_FRIENDLY;

    // not really "created" friendly, but should it become
    // hostile later on, it won't count as a good kill
    orc->flags |= MF_CREATED_FRIENDLY;

    orc->flags |= MF_GOD_GIFT;

    if (orc->hit_points <= 0)
        orc->hit_points = std::min(random_range(1, 4), orc->max_hit_points);

    // to avoid immobile "followers"
    behaviour_event(orc, ME_ALERT, MHITNOT);
}

bool is_orcish_follower(const monsters* mon)
{
    return (mon->alive() && mons_species(mon->type) == MONS_ORC
        && mon->attitude == ATT_FRIENDLY
        && (mon->flags & MF_GOD_GIFT));
}

void excommunication(god_type new_god)
{
    const god_type old_god = you.religion;
    god_acting gdact(old_god, true);

    take_note(Note(NOTE_LOSE_GOD, old_god));

    you.duration[DUR_PRAYER] = 0;
    you.duration[DUR_PIETY_POOL] = 0; // your loss
    you.religion = GOD_NO_GOD;
    you.piety = 0;
    you.piety_hysteresis = 0;
    redraw_skill( you.your_name, player_title() );

    mpr("You have lost your religion!");
    more();

    switch (old_god)
    {
    case GOD_XOM:
        xom_acts( false, abs(you.piety - 100) * 2);
        inc_penance( old_god, 50 );
        break;

    case GOD_KIKUBAAQUDGHA:
        simple_god_message( " does not appreciate desertion!", old_god );
        miscast_effect( SPTYP_NECROMANCY, 5 + you.experience_level,
                        random2avg(88, 3), 100, "the malice of Kikubaaqudgha" );
        inc_penance( old_god, 30 );
        break;

    case GOD_YREDELEMNUL:
        simple_god_message( " does not appreciate desertion!", old_god );
        miscast_effect( SPTYP_NECROMANCY, 5 + you.experience_level,
                        random2avg(88, 3), 100, "the anger of Yredelemnul" );
        inc_penance( old_god, 30 );
        break;

    case GOD_VEHUMET:
        simple_god_message( " does not appreciate desertion!", old_god );
        miscast_effect( (coinflip() ? SPTYP_CONJURATION : SPTYP_SUMMONING),
                        8 + you.experience_level, random2avg(98, 3), 100,
                        "the wrath of Vehumet" );
        inc_penance( old_god, 25 );
        break;

    case GOD_MAKHLEB:
        simple_god_message( " does not appreciate desertion!", old_god );
        miscast_effect( (coinflip() ? SPTYP_CONJURATION : SPTYP_SUMMONING),
                        8 + you.experience_level, random2avg(98, 3), 100,
                        "the fury of Makhleb" );
        inc_penance( old_god, 25 );
        break;

    case GOD_TROG:
        simple_god_message( " does not appreciate desertion!", old_god );
        make_god_gifts_hostile(false);

        // Penance has to come before retribution to prevent "mollify"
        inc_penance( old_god, 50 );
        divine_retribution( old_god );
        break;

    case GOD_BEOGH:
        simple_god_message( " does not appreciate desertion!", old_god );
        beogh_followers_abandon_you(); // friendly orcs around turn hostile

        // You might have lost water walking at a bad time...
        if ( need_water_walking() )
            fall_into_a_pool( you.x_pos, you.y_pos, true,
                              grd[you.x_pos][you.y_pos] );

        // Penance has to come before retribution to prevent "mollify"
        inc_penance( old_god, 50 );
        divine_retribution( old_god );
        break;

    case GOD_SIF_MUNA:
        inc_penance( old_god, 50 );
        break;

    case GOD_NEMELEX_XOBEH:
        nemelex_shuffle_decks();
        inc_penance( old_god, 150 ); // Nemelex penance is special
        break;

    case GOD_LUGONU:
        if ( you.level_type == LEVEL_DUNGEON )
        {
            simple_god_message(" casts you back into the Abyss!", old_god);
            banished(DNGN_ENTER_ABYSS, "Lugonu's wrath");
        }
        inc_penance(old_god, 50);
        break;

    case GOD_ELYVILON:
        inc_penance( old_god, 50 );
        break;

    case GOD_SHINING_ONE:
        mpr("Your divine halo starts to fade.");

        if (you.duration[DUR_DIVINE_SHIELD])
        {
            mpr("Your divine shield disappears!");
            you.duration[DUR_DIVINE_SHIELD] = 0;
            you.attribute[ATTR_DIVINE_SHIELD] = 0;
            you.redraw_armour_class = true;
        }

        if (!is_good_god(new_god))
            make_god_gifts_hostile(false);
        else
            make_god_gifts_neutral(false);

        inc_penance( old_god, 50 );
        break;

    case GOD_ZIN:
        if (env.sanctuary_time)
            remove_sanctuary();
        // deliberate fall-through
    default:
        inc_penance( old_god, 25 );
        break;
    }

    // When you leave one of the good gods for a non-good god, or no
    // god, you make all non-hostile holy beings hostile.
    if (!is_good_god(new_god))
    {
        if (moral_beings_attitude_change())
            mpr("The divine host forsakes you.", MSGCH_MONSTER_ENCHANT);
    }
}                               // end excommunication()

static bool bless_weapon( god_type god, int brand, int colour )
{
    const int wpn = get_player_wielded_weapon();

    // Assuming the type of weapon is correct, we only need to check
    // to see if it's an artefact we can successfully clobber:
    if (!is_fixed_artefact( you.inv[wpn] )
        && !is_random_artefact( you.inv[wpn] ))
    {
        you.duration[DUR_WEAPON_BRAND] = 0;     // just in case

        set_equip_desc( you.inv[wpn], ISFLAG_GLOWING );
        set_item_ego_type( you.inv[wpn], OBJ_WEAPONS, brand );
        you.inv[wpn].colour = colour;

        do_uncurse_item( you.inv[wpn] );
        enchant_weapon( ENCHANT_TO_HIT, true );
        enchant_weapon( ENCHANT_TO_DAM, true );

        you.wield_change = true;
        you.num_gifts[god]++;
        take_note(Note(NOTE_GOD_GIFT, you.religion));

        you.flash_colour = colour;
        viewwindow( true, false );

        mprf( MSGCH_GOD, "Your weapon shines brightly!" );
        simple_god_message( " booms: Use this gift wisely!" );

        if ( god == GOD_SHINING_ONE )
        {
            holy_word( 100, HOLY_WORD_GENERIC, true );
            // un-bloodify surrounding squares
            for (int i=-3;i<=3;i++)
                for (int j=-3;j<=3;j++)
                {
                     if (is_bloodcovered(you.x_pos+i, you.y_pos+j))
                         env.map[you.x_pos+i][you.y_pos+j].property = FPROP_NONE;
                }
        }

        delay(1000);

        return (true);
    }

    return (false);
}

static std::string sacrifice_message(god_type god, const item_def &item,
                                     bool gained_piety)
{
    const char *msg = sacrifice[god][!!gained_piety];
    const char *be = item.quantity == 1? "is" : "are";

    // "The bread ration evaporates", "The arrows evaporate" - the s suffix
    // is for verbs.
    const char *ssuffix = item.quantity == 1? "s" : "";

    return (replace_all(replace_all(msg, "%", ssuffix), "&", be));
}

void altar_prayer(void)
{
    // different message than when first joining a religion
    mpr( "You prostrate yourself in front of the altar and pray." );

    if (you.religion == GOD_XOM)
        return;

    god_acting gdact;

    // TSO blesses long swords with holy wrath
    if (you.religion == GOD_SHINING_ONE
        && !you.num_gifts[GOD_SHINING_ONE]
        && !player_under_penance()
        && you.piety > 160)
    {
        const int wpn = get_player_wielded_weapon();

        if (wpn != -1
            && weapon_skill( you.inv[wpn] ) == SK_LONG_BLADES
            && get_weapon_brand( you.inv[wpn] ) != SPWPN_HOLY_WRATH)
        {
            if (bless_weapon( GOD_SHINING_ONE, SPWPN_HOLY_WRATH, YELLOW ))
            {
                // convert those demon blades if blessed:
                if (you.inv[wpn].sub_type == WPN_DEMON_BLADE)
                    you.inv[wpn].sub_type = WPN_BLESSED_BLADE;
            }
        }
    }

    // Lugonu blesses weapons with distortion
    if (you.religion == GOD_LUGONU
        && !you.num_gifts[GOD_LUGONU]
        && !player_under_penance()
        && you.piety > 160)
    {
        const int wpn = get_player_wielded_weapon();

        if (wpn != -1 && get_weapon_brand(you.inv[wpn]) != SPWPN_DISTORTION)
            bless_weapon(GOD_LUGONU, SPWPN_DISTORTION, RED);
    }

    offer_items();
}                               // end altar_prayer()

bool god_hates_attacking_friend(god_type god, const actor *fr)
{
    if (!fr || fr->kill_alignment() != KC_FRIENDLY)
        return (false);

    switch (god)
    {
        case GOD_ZIN:
        case GOD_SHINING_ONE:
        case GOD_ELYVILON:
        case GOD_OKAWARU:
            return (true);
        case GOD_BEOGH: // added penance to avoid killings for loot
            return (mons_species(fr->id()) == MONS_ORC);

        default:
            return (false);
    }
}

static bool god_likes_items(god_type god)
{
    switch (god)
    {
    case GOD_ZIN:      case GOD_SHINING_ONE:   case GOD_KIKUBAAQUDGHA:
    case GOD_OKAWARU:  case GOD_MAKHLEB:       case GOD_SIF_MUNA:
    case GOD_TROG:     case GOD_NEMELEX_XOBEH:
        return true;

    case GOD_YREDELEMNUL: case GOD_XOM:        case GOD_VEHUMET:
    case GOD_LUGONU:      case GOD_BEOGH:      case GOD_ELYVILON:
        return false;

    case GOD_NO_GOD: case NUM_GODS: case GOD_RANDOM:
        mprf(MSGCH_DANGER, "Bad god, no biscuit! %d", static_cast<int>(god) );
        return false;
    }
    return false;
}

static bool god_likes_item(god_type god, const item_def& item)
{
    if ( !god_likes_items(god) )
        return false;

    switch (god)
    {
    case GOD_KIKUBAAQUDGHA: case GOD_TROG:
        return item.base_type == OBJ_CORPSES;

    case GOD_NEMELEX_XOBEH:
        return !is_deck(item);

    case GOD_SHINING_ONE:
        return is_evil_item(item);

    case GOD_ZIN:
        return item.base_type == OBJ_GOLD;

    default:
        return true;
    }
}

static int leading_sacrifice_group()
{
    int weights[5];
    get_pure_deck_weights(weights);
    int best_i = -1, maxweight = -1;
    for ( int i = 0; i < 5; ++i )
    {
        if ( best_i == -1 || weights[i] > maxweight )
        {
            maxweight = weights[i];
            best_i = i;
        }
    }
    return best_i;
}

static void give_sac_group_feedback(int which)
{
    ASSERT( which >= 0 && which < 5 );
    const char* names[] = {
        "Escape", "Destruction", "Dungeons", "Summoning", "Wonder"
    };
    mprf(MSGCH_GOD, "A symbol of %s coalesces before you, then vanishes.",
         names[which]);
}

void offer_items()
{
    if (you.religion == GOD_NO_GOD)
        return;

    int i = igrd[you.x_pos][you.y_pos];

    if (!god_likes_items(you.religion) && i != NON_ITEM)
    {
        simple_god_message(" doesn't care about such mundane gifts.", you.religion);
        return;
    }

    god_acting gdact;

    // donate gold to gain piety distributed over time
    if (you.religion == GOD_ZIN)
    {
        if (!you.gold)
        {
            mpr("You don't have anything to sacrifice.");
            return;
        }

        if (!yesno("Do you wish to part with all of your money?", true, 'n'))
            return;

        int donation_value = (int) (you.gold/200 * log((double)you.gold));
#if DEBUG_DIAGNOSTICS || DEBUG_SACRIFICE || DEBUG_PIETY
        mprf(MSGCH_DIAGNOSTICS, "A donation of $%d amounts to an "
             "increase of piety by %d.", you.gold, donation_value);
#endif
        you.gold = 0;
        you.redraw_gold = true;

        if (donation_value < 1)
        {
            simple_god_message(" finds your generosity lacking.");
            return;
        }

        int estimated_piety = you.piety + donation_value;

        you.duration[DUR_PIETY_POOL] += donation_value;
        if (you.duration[DUR_PIETY_POOL] > 500)
            you.duration[DUR_PIETY_POOL] = 500;

        if (you.penance[GOD_ZIN])
        {
            if (estimated_piety >= you.penance[GOD_ZIN])
                mpr("You feel that soon you will be absolved of all your sins.");
            else
                mpr("You feel that soon your burden of sins will be lighter.");
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

        if (donation_value >= 30 && you.piety <= 170)
            result += "!";
        else
            result += ".";

        mpr(result.c_str());

        return; // doesn't accept anything else for sacrifice
    }

    if (i == NON_ITEM) // nothing to sacrifice
        return;

    int num_sacced = 0;

    const int old_leading = leading_sacrifice_group();

    while (i != NON_ITEM)
    {
        item_def &item(mitm[i]);
        const int next  = item.link;  // in case we can't get it later.
        const int value = item_value( item, true );

        if (item_is_stationary(item) || !god_likes_item(you.religion, item))
        {
            i = next;
            continue;
        }

        if ( is_risky_sacrifice(item) ||
             item.inscription.find("=p") != std::string::npos)
        {
            const std::string msg =
                  "Really sacrifice " + item.name(DESC_NOCAP_A) + "?";

            if (!yesno(msg.c_str()))
            {
                i = next;
                continue;
            }
        }

        bool gained_piety = false;

#if DEBUG_DIAGNOSTICS || DEBUG_SACRIFICE
        mprf(MSGCH_DIAGNOSTICS, "Sacrifice item value: %d", value);
#endif

        switch (you.religion)
        {
        case GOD_NEMELEX_XOBEH:
            if (you.attribute[ATTR_CARD_COUNTDOWN] && random2(800) < value)
            {
                you.attribute[ATTR_CARD_COUNTDOWN]--;
#if DEBUG_DIAGNOSTICS || DEBUG_CARDS || DEBUG_SACRIFICE
                mprf(MSGCH_DIAGNOSTICS, "Countdown down to %d",
                     you.attribute[ATTR_CARD_COUNTDOWN]);
#endif
            }
            if ((item.base_type == OBJ_CORPSES &&
                 one_chance_in(2+you.piety/50))
                // Nemelex piety gain is fairly fast...at least
                // when you have low piety.
                || value/2 >= random2(30 + you.piety/2))
            {
                gain_piety(1);
                gained_piety = true;
            }

            if (item.base_type == OBJ_FOOD && item.sub_type == FOOD_CHUNK)
                // No sacrifice value for chunks of flesh, since food
                // value goes towards decks of wonder.
                ;
            else if (item.base_type == OBJ_CORPSES)
            {
#if DEBUG_GIFTS || DEBUG_CARDS || DEBUG_SACRIFICE
                mprf(MSGCH_DIAGNOSTICS, "Corpse mass is %d",
                     item_mass(item));
#endif
                you.sacrifice_value[item.base_type] += item_mass(item);
            }
            else
                you.sacrifice_value[item.base_type] += value;
            break;

        case GOD_OKAWARU:
        case GOD_MAKHLEB:
            if (mitm[i].base_type == OBJ_CORPSES
                || random2(value) >= 50
                || player_under_penance())
            {
                gained_piety = true;
                gain_piety(1);
            }
            break;

        case GOD_SIF_MUNA:
            if (value >= 150)
            {
                gain_piety(1 + random2(3));
                gained_piety = true;
            }
            break;

        case GOD_KIKUBAAQUDGHA:
        case GOD_TROG:
            gain_piety(1);
            gained_piety = true;
            break;

        default:
            break;
        }

        msg::streams(MSGCH_GOD) << mitm[i].name(DESC_CAP_THE)
                                << sacrifice_message(you.religion, mitm[i],
                                                     gained_piety)
                                << std::endl;
        item_was_destroyed(mitm[i]);
        destroy_item(i);
        i = next;
        num_sacced++;
    }

    if ( num_sacced > 0 && you.religion == GOD_NEMELEX_XOBEH )
    {
        const int new_leading = leading_sacrifice_group();
        if ( old_leading != new_leading || one_chance_in(50) )
            give_sac_group_feedback(new_leading);

#if DEBUG_GIFTS || DEBUG_CARDS || DEBUG_SACRIFICE
        show_pure_deck_chances();
#endif
    }
    else if (!num_sacced) // explanatory messages if nothing sacrificed
    {
        if (you.religion == GOD_KIKUBAAQUDGHA || you.religion == GOD_TROG)
            simple_god_message(" only cares about primal sacrifices!", you.religion);
        else if (you.religion == GOD_NEMELEX_XOBEH)
            simple_god_message(" expects you to use your decks, not offer them!", you.religion);
        else if (you.religion == GOD_SHINING_ONE)
            simple_god_message(" only cares about evil items!", you.religion);
        // everyone else was handled above (Zin!) or likes everything
    }
}

void god_pitch(god_type which_god)
{
    mprf("You %s the altar of %s.",
         you.species == SP_NAGA ? "coil in front of"
                                : "kneel at",
         god_name(which_god).c_str());
    more();

    // Note: using worship we could make some gods not allow followers to
    // return, or not allow worshippers from other religions.  -- bwr

    // Gods can be racist...
    const bool you_evil = you.is_undead || you.species == SP_DEMONSPAWN;
    if ( (you_evil && is_good_god(which_god)) ||
         (you.species != SP_HILL_ORC && which_god == GOD_BEOGH) )
    {
        you.turn_is_over = false;
        simple_god_message(" does not accept worship from those such as you!",
                           which_god);
        return;
    }

    if (which_god == GOD_LUGONU && you.penance[GOD_LUGONU])
    {
        simple_god_message(" is most displeased with you!", which_god);
        god_acting gdact(GOD_LUGONU, true);
        lugonu_retribution();
        return;
    }

    describe_god( which_god, false );

    snprintf( info, INFO_SIZE, "Do you wish to %sjoin this religion?",
              (you.worshipped[which_god]) ? "re" : "" );

    if (!yesno( info, true, 'n' ) || !yesno("Are you sure?", false, 'n'))
    {
        you.turn_is_over = false; // Okay, opt out.
        redraw_screen();
        return;
    }

    // OK, so join the new religion.

    redraw_screen();

    // Leave your prior religion first.
    if (you.religion != GOD_NO_GOD)
        excommunication(which_god);

    // Welcome to the fold!
    you.religion = static_cast<god_type>(which_god);

    if (you.religion == GOD_XOM)
    {
        // Xom uses piety and gift_timeout differently.
        you.piety = 100;
        you.gift_timeout = random2(40) + random2(40);
    }
    else
    {
        you.piety = 15;             // to prevent near instant excommunication
        you.piety_hysteresis = 0;
        you.gift_timeout = 0;
    }

    set_god_ability_slots();    // remove old god's slots, reserve new god's
#ifdef DGL_WHEREIS
    whereis_record();
#endif
    snprintf( info, INFO_SIZE, " welcomes you%s!",
              (you.worshipped[which_god]) ? " back" : "" );

    simple_god_message( info );
    more();

    if (you.religion == GOD_ELYVILON)
    {
        mpr("You can now call upon Elyvilon to destroy weapons "
            "lying on the ground.", MSGCH_GOD);
    }
    else if (you.religion == GOD_TROG)
    {
        mpr("You can now call upon Trog to burn books in your surroundings.",
            MSGCH_GOD);
    }

    if (you.worshipped[you.religion] < 100)
        you.worshipped[you.religion]++;

    take_note(Note(NOTE_GET_GOD, you.religion));

    // Currently penance is just zeroed, this could be much more interesting.
    you.penance[you.religion] = 0;

    // When you start worshipping a good god, you make all non-hostile
    // evil and unholy beings hostile.
    if (is_good_god(you.religion))
    {
        if (moral_beings_attitude_change())
            mpr("Your evil allies forsake you.", MSGCH_MONSTER_ENCHANT);
    }

    if (is_evil_god(you.religion))
    {
        // Note: Using worshipped[] we could make this sort of grudge
        // permanent instead of based off of penance. -- bwr
        if (you.penance[GOD_SHINING_ONE])
        {
            inc_penance(GOD_SHINING_ONE, 30);
            god_speaks(GOD_SHINING_ONE,
                       "\"You will pay for your evil ways, mortal!\"");
        }
    }

    // note that you.worshipped[] has already been incremented
    if ( you.religion == GOD_LUGONU && you.worshipped[GOD_LUGONU] == 1 )
        gain_piety(20);         // allow instant access to first power

    redraw_skill( you.your_name, player_title() );
}                               // end god_pitch()

bool god_likes_butchery(god_type god)
{
    return (god == GOD_OKAWARU || god == GOD_MAKHLEB ||
            god == GOD_TROG || god == GOD_BEOGH ||
            god == GOD_LUGONU);
}

bool god_hates_butchery(god_type god)
{
    return (god == GOD_ELYVILON);
}

harm_protection_type god_protects_from_harm(god_type god, bool actual)
{
    const int min_piety = piety_breakpoint(0);
    bool praying = (you.duration[DUR_PRAYER] &&
                    random2(you.piety) >= min_piety);
    bool anytime = (one_chance_in(10) || you.piety > random2(1000));
    bool penance = you.penance[god];

    // If actual is true, return HPT_NONE if the given god can protect
    // the player from harm, but doesn't actually do so.
    switch (god)
    {
    case GOD_YREDELEMNUL:
        if (!actual || praying)
        {
            if (you.piety >= min_piety)
                return HPT_PRAYING;
        }
        break;
    case GOD_BEOGH:
        if (!penance && (!actual || anytime))
            return HPT_ANYTIME;
        break;
    case GOD_ZIN:
    case GOD_SHINING_ONE:
        if (!actual || anytime)
            return HPT_ANYTIME;
        break;
    case GOD_ELYVILON:
        if (!actual || praying || anytime)
        {
            return (you.piety >= min_piety) ? HPT_PRAYING_PLUS_ANYTIME :
                                              HPT_ANYTIME;
        }
        break;
    default:
        break;
    }

    return HPT_NONE;
}

void god_smites_you(god_type god, kill_method_type death_type,
                    const char *message)
{
    // Your god won't protect you from his own smiting, and Xom is too
    // capricious to protect you from any god's smiting.
    if (you.religion != god && you.religion != GOD_XOM &&
        !player_under_penance() && you.piety > random2(MAX_PIETY * 2))
    {
        snprintf(info, INFO_SIZE, "Mortal, I have averted the wrath "
                 "of %s... this time.", god_name(god).c_str());
        god_speaks(you.religion, info);
    }
    else
    {
        // If there's a message, display it before smiting.
        if (message)
            god_speaks(god, message);

        int divine_hurt = 10 + random2(10);

        for (int i = 0; i < 5; i++)
            divine_hurt += random2( you.experience_level );

        simple_god_message( " smites you!", god );
        ouch( divine_hurt, 0, death_type );
        dec_penance( god, 1 );
    }
}

void offer_corpse(int corpse)
{
    // We always give the "good" (piety-gain) message when doing
    // dedicated butchery. Uh, call it a feature.
    mprf(MSGCH_GOD, "%s%s", mitm[corpse].name(DESC_CAP_THE).c_str(),
         sacrifice_message(you.religion, mitm[corpse], true).c_str());

    did_god_conduct(DID_DEDICATED_BUTCHERY, 10);

    // ritual sacrifice can also bloodify the ground
    const int mons_class = mitm[corpse].plus;
    const int max_chunks = mons_weight( mons_class ) / 150;
    bleed_onto_floor(you.x_pos, you.y_pos, mons_class, max_chunks, true);
    destroy_item(corpse);
}

// Returns true if the player can use the good gods' passive piety gain.
static bool need_free_piety()
{
    return (you.piety < 150 || you.gift_timeout || you.penance[you.religion]);
}

//jmf: moved stuff from items::handle_time()
void handle_god_time(void)
{
    if (one_chance_in(100))
    {
        // Choose a god randomly from those to whom we owe penance.
        //
        // Proof: (By induction)
        //
        // 1) n = 1, probability of choosing god is one_chance_in(1)
        // 2) Asuume true for n = k (ie. prob = 1 / n for all n)
        // 3) For n = k + 1,
        //
        //      P:new-found-god = 1 / n (see algorithm)
        //      P:other-gods = (1 - P:new-found-god) * P:god-at-n=k
        //                             1        1
        //                   = (1 - -------) * ---
        //                           k + 1      k
        //
        //                          k         1
        //                   = ----------- * ---
        //                        k + 1       k
        //
        //                       1       1
        //                   = -----  = ---
        //                     k + 1     n
        //
        // Therefore, by induction the probability is uniform.  As for
        // why we do it this way... it requires only one pass and doesn't
        // require an array.

        god_type which_god = GOD_NO_GOD;
        unsigned int count = 0;

        for (int i = GOD_NO_GOD; i < NUM_GODS; i++)
        {
            // Nemelex penance is special: it's only "active"
            // when penance > 100, else it's passive.
            if (you.penance[i] && (i != GOD_NEMELEX_XOBEH ||
                                   you.penance[i] > 100))
            {
                count++;
                if (one_chance_in(count))
                    which_god = static_cast<god_type>(i);
            }
        }

        if (which_god != GOD_NO_GOD)
            divine_retribution(which_god);
    }

    // Update the god's opinion of the player
    if (you.religion != GOD_NO_GOD)
    {
        switch (you.religion)
        {
        case GOD_XOM:
        {
            // Xom semi-randomly drifts your piety.
            int delta;
            const char *origfavour = describe_xom_favour();
            const bool good = you.piety >= 100;
            int size = abs(you.piety - 100);
            delta = (random2(1000) < 511) ? 1 : -1;
            size += delta;
            you.piety = 100 + (good ? size : -size);
            const char *newfavour = describe_xom_favour();
            if (strcmp(origfavour, newfavour))
            {
                // Dampen oscillation across announcement boundaries:
                size += delta * 2;
                you.piety = 100 + (good ? size : -size);
            }

            // ... but he gets bored... (I re-use gift_timeout for
            // this instead of making a separate field because I don't
            // want to learn how to save and restore a new field). In
            // this usage, the "gift" is the gift you give to Xom of
            // something interesting happening.
            if (you.gift_timeout == 1)
            {
                god_speaks(you.religion, "Xom is getting BORED.");
                you.gift_timeout = 0;
            }
            else if (you.gift_timeout > 1)
            {
                you.gift_timeout -= random2(2);
            }

            if (one_chance_in(20))
            {
                xom_acts(abs(you.piety - 100));
            }
            break;
        }

        // These gods like long-standing worshippers
        case GOD_ELYVILON:
            if (need_free_piety() && one_chance_in(20))
                gain_piety(1);
            break;

        case GOD_SHINING_ONE:
            if (need_free_piety() && one_chance_in(15))
                gain_piety(1);
            break;

        case GOD_ZIN:
            if (need_free_piety() && one_chance_in(12))
                gain_piety(1);
            break;

        case GOD_YREDELEMNUL:
        case GOD_KIKUBAAQUDGHA:
        case GOD_VEHUMET:
            if (one_chance_in(17))
                lose_piety(1);
            if (you.piety < 1)
                excommunication();
            break;

        case GOD_OKAWARU: // These gods accept corpses, so they time-out faster
        case GOD_TROG:
        case GOD_BEOGH:
            if (one_chance_in(14))
                lose_piety(1);
            if (you.piety < 1)
                excommunication();
            break;

        case GOD_MAKHLEB:
        case GOD_LUGONU:
            if (one_chance_in(16))
                lose_piety(1);
            if (you.piety < 1)
                excommunication();
            break;

        case GOD_SIF_MUNA:
            // [dshaligram] Sif Muna is now very patient - has to be
            // to make up for the new spell training requirements, else
            // it's practically impossible to get Master of Arcane status.
            if (one_chance_in(100))
                lose_piety(1);
            if (you.piety < 1)
                excommunication();
            break;

        case GOD_NEMELEX_XOBEH: // relatively patient
            if (one_chance_in(35))
                lose_piety(1);
            if (you.attribute[ATTR_CARD_COUNTDOWN] > 0 && coinflip())
                you.attribute[ATTR_CARD_COUNTDOWN]--;
            if (you.piety < 1)
                excommunication();
            break;

        default:
            DEBUGSTR("Bad god, no bishop!");
        }
    }
}                               // end handle_god_time()

// yet another wrapper for mpr() {dlb}:
void simple_god_message(const char *event, god_type which_deity)
{
    if (which_deity == GOD_NO_GOD)
        which_deity = you.religion;

    std::string msg = god_name(which_deity);
    msg += event;
    god_speaks( which_deity, msg.c_str() );
}

int god_colour( god_type god ) //mv - added
{
    switch (god)
    {
    case GOD_SHINING_ONE:
    case GOD_ZIN:
    case GOD_ELYVILON:
    case GOD_OKAWARU:
        return(CYAN);

    case GOD_YREDELEMNUL:
    case GOD_KIKUBAAQUDGHA:
    case GOD_MAKHLEB:
    case GOD_VEHUMET:
    case GOD_TROG:
    case GOD_BEOGH:
    case GOD_LUGONU:
        return(LIGHTRED);

    case GOD_XOM:
        return(YELLOW);

    case GOD_NEMELEX_XOBEH:
        return(LIGHTMAGENTA);

    case GOD_SIF_MUNA:
        return(LIGHTBLUE);

    case GOD_NO_GOD:
    default:
        break;
    }

    return(YELLOW);
}

int piety_rank( int piety )
{
    const int breakpoints[] = { 161, 120, 100, 75, 50, 30, 6 };
    const int numbreakpoints = sizeof(breakpoints) / sizeof(int);
    if ( piety < 0 )
        piety = you.piety;
    for ( int i = 0; i < numbreakpoints; ++i )
        if ( piety >= breakpoints[i] )
            return numbreakpoints - i;
    return 0;
}

int piety_breakpoint(int i)
{
    int breakpoints[MAX_GOD_ABILITIES] = { 30, 50, 75, 100, 120 };
    if ( i >= MAX_GOD_ABILITIES || i < 0 )
        return 255;
    else
        return breakpoints[i];
}

// Returns true if The Shining One doesn't mind you stabbing this creature.
bool tso_stab_safe_monster(const actor *act)
{
    const mon_holy_type holy = act->holiness();
    return (holy != MH_NATURAL && holy != MH_HOLY);
}

/////////////////////////////////////////////////////////////////////
// god_conduct_trigger

god_conduct_trigger::god_conduct_trigger(
    conduct_type c, int pg, bool kn, const monsters *vict)
  : conduct(c), pgain(pg), known(kn), enabled(true), victim(NULL)
{
    if (vict)
    {
        victim.reset(new monsters);
        *(victim.get()) = *vict;
    }
}

void god_conduct_trigger::set(conduct_type c, int pg, bool kn,
                              const monsters *vict)
{
    conduct = c;
    pgain = pg;
    known = kn;
    victim.reset(NULL);
    if (vict)
    {
        victim.reset(new monsters);
        *victim.get() = *vict;
    }
}

god_conduct_trigger::~god_conduct_trigger()
{
    if (enabled && conduct != NUM_CONDUCTS)
        did_god_conduct(conduct, pgain, known, victim.get());
}
