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

#ifdef DOS
#include <dos.h>
#endif

#include "externs.h"

#include "abl-show.h"
#include "beam.h"
#include "chardump.h"
#include "debug.h"
#include "decks.h"
#include "describe.h"
#include "effects.h"
#include "food.h"
#include "invent.h"
#include "it_use2.h"
#include "itemname.h"
#include "itemprop.h"
#include "item_use.h"
#include "items.h"
#include "makeitem.h"
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
#include "spl-cast.h"
#include "stuff.h"
#include "tutorial.h"
#include "view.h"

// Item offer messages for the gods:
// & is replaced by "is" or "are" as appropriate for the item.
// % is replaced by "s" or "" as appropriate.
const char *sacrifice[] =
{
    // No god
    " & eaten by a ravening swarm of bugs.",
    // Zin
    " glow% silver and disappear%.",
    // TSO
    " glow% a brilliant golden colour and disappear%.",
    // Kikubaaqudgha
    " rot% away in an instant.",
    // Yredelemnul
    " crumble% to dust.",
    // Xom (no sacrifices)
    " & eaten by a bug.",
    // Vehumet
    " explode% into nothingness.",
    // Okawaru
    " & consumed in a burst of flame.",
    // Makhleb
    " & consumed in a roaring column of flame.",
    // Sif Muna
    " glow% faintly for a moment, and & gone.",
    // Trog
    " & consumed in a roaring column of flame.",
    // Nemelex
    " glow% with a rainbow of weird colours and disappear%.",
    // Elyvilon
    " evaporate%.",
    // Lugonu
    " & consumed by the void.",
    // Beogh
    " crumble% into the ground."
};

const char* god_gain_power_messages[MAX_NUM_GODS][MAX_GOD_ABILITIES] =
{
    // no god
    { "", "", "", "", "" },
    // Zin
    { "repel the undead",
      "call upon Zin for minor healing",
      "call down a plague",
      "utter a Holy Word",      
      "summon a guardian angel" },
    // TSO
    { "repel the undead",
      "smite your foes",
      "dispel the undead",
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
    { "gain power from killing in Vehumet's name",
      "You can call upon Vehumet to aid your destructive magics with prayer.",
      "During prayer you have some protection from summoned creatures.",
      "", "" },
    // Okawaru
    { "give your body great, but temporary strength",
      "call upon Okawaru for minor healing",
      "",
      "",
      "haste yourself" },
    // Makhleb
    { "gain power from killing in Makhleb's name",
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
      "give your body great, but temporary, strength",
      "",
      "haste yourself",
      "" },
    // Nemelex
    { "peek at the first card of a deck",
      "draw cards with careful consideration",
      "",
      "",
      "stack decks" },
    // Elyvilon
    { "call upon Elyvilon for minor healing",
      "call upon Elyvilon for purification",
      "call upon Elyvilon for moderate healing",
      "call upon Elyvilon to restore your abilities",
      "call upon Elyvilon for incredible healing" },
    // Lugonu
    { "depart the Abyss - at a permanent cost",
      "bend space around yourself",
      "summon the demons of the Abyss to your aid",
      "",
      "gate yourself to the Abyss" },
    // Beogh
    { "Beogh supports the use of orcish gear.",
      "smite your foes",
      "gain orcish followers",
      "recall your orcish followers",
      "walk on water" }
};

const char* god_lose_power_messages[MAX_NUM_GODS][MAX_GOD_ABILITIES] =
{
    // no god
    { "", "", "", "", "" },
    // Zin
    { "repel the undead",
      "call upon Zin for minor healing",
      "call down a plague",
      "utter a Holy Word",      
      "summon a guardian angel" },
    // TSO
    { "repel the undead",
      "smite your foes",
      "dispel the undead",
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
    { "gain power from killing in Vehumet's name",
      "Vehumet will no longer aid your destructive magics.",
      "Vehumet will no longer shield you from summoned creatures.",
      "",
      "" },
    // Okawaru
    { "give your body great, but temporary strength",
      "call upon Okawaru for minor healing",
      "",
      "",
      "haste yourself" },
    // Makhleb
    { "gain power from killing in Makhleb's name",
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
      "give your body great, but temporary, strength",
      "",
      "haste yourself",
      "" },
    // Nemelex
    { "peek at the first card of a deck",
      "draw cards with careful consideration",
      "",
      "",
      "stack decks" },
    // Elyvilon
    { "call upon Elyvilon for minor healing",
      "call upon Elyvilon for purification",
      "call upon Elyvilon for moderate healing",
      "call upon Elyvilon to restore your abilities",
      "call upon Elyvilon for incredible healing" },
    // Lugonu
    { "depart the Abyss at will",
      "bend space around yourself",
      "summon the demons of the Abyss to your aid",
      "",
      "gate yourself to the Abyss" },
    // Beogh
    { "Beogh no longer supports the use of orcish gear.",
      "smite your foes",
      "gain orcish followers",
      "recall your orcish followers",
      "walk on water" }
};


void altar_prayer(void);
void dec_penance(int god, int val);
void inc_penance(int god, int val);
void inc_penance(int val);
int followers_abandon_you(void); // Beogh

static bool is_evil_god(god_type god)
{
    return
        god == GOD_KIKUBAAQUDGHA ||
        god == GOD_MAKHLEB ||
        god == GOD_YREDELEMNUL ||
        god == GOD_VEHUMET ||
        god == GOD_BEOGH ||
        god == GOD_LUGONU;
}

static bool is_good_god(god_type god)
{
    return
        god == GOD_SHINING_ONE ||
        god == GOD_ZIN ||
        god == GOD_ELYVILON;
}

bool is_priest_god(god_type god)
{
    switch (god)
    {
    case GOD_ZIN:
    case GOD_YREDELEMNUL:
    case GOD_BEOGH:
        return (true);
    default:
        return (false);
    }
}

void dec_penance(god_type god, int val)
{
    if (you.penance[god] > 0)
    {
        if (you.penance[god] <= val)
        {
            simple_god_message(" seems mollified.", god);
            take_note(Note(NOTE_MOLLIFY_GOD, god));
            you.penance[god] = 0;

            // bonuses now once more effective
            if ( god == GOD_BEOGH && you.religion == GOD_BEOGH)
            {
                 you.redraw_armour_class = 1;
            }
        }
        else
            you.penance[god] -= val;
    }
}                               // end dec_penance()

void dec_penance(int val)
{
    dec_penance(you.religion, val);
}                               // end dec_penance()

void inc_penance(int god, int val)
{
    // orcish bonuses don't apply under penance
    if ( god == GOD_BEOGH && you.penance[god] == 0)
    {
         you.redraw_armour_class = 1;
    }
    
    if (you.penance[god] + val > 200)
        you.penance[god] = 200;
    else
        you.penance[god] += val;
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

static int random_undead_servant(int religion)
{
    // error trapping {dlb}
    int thing_called = MONS_PROGRAM_BUG;
    int temp_rand = random2(100);
    thing_called = ((temp_rand > 66) ? MONS_WRAITH :            // 33%
                    (temp_rand > 52) ? MONS_WIGHT :             // 12%
                    (temp_rand > 40) ? MONS_SPECTRAL_WARRIOR :  // 16%
                    (temp_rand > 31) ? MONS_ROTTING_HULK :      //  9%
                    (temp_rand > 23) ? MONS_SKELETAL_WARRIOR :  //  8%
                    (temp_rand > 16) ? MONS_VAMPIRE :           //  7%
                    (temp_rand > 10) ? MONS_GHOUL :             //  6%
                    (temp_rand >  4) ? MONS_MUMMY               //  6%
                                     : MONS_FLAYED_GHOST);      //  5%
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
    const int best_missile_skill = best_skill(SK_SLINGS, SK_RANGED_COMBAT);
    const item_def *launcher = find_missile_launcher(best_missile_skill);
    return (you.piety > 80
            && random2( you.piety ) > 70
            && !grid_destroys_items( grd[you.x_pos][you.y_pos] )
            && one_chance_in(8)
            && you.skills[ best_missile_skill ] >= 8
            && (launcher || best_missile_skill == SK_DARTS)
            && ammo_count(launcher) < 20 + random2(35));
}

static void do_god_gift()
{
    // Consider a gift if we don't have a timeout and weren't
    // already praying when we prayed.
    if (!you.penance[you.religion]
        && (!you.gift_timeout || you.religion == GOD_ZIN))
    {
        bool success = false;
        
        //   Remember to check for water/lava
        switch (you.religion)
        {
        default:
            break;

        case GOD_ZIN:
            //jmf: "good" god will feed you (a la Nethack)
            if (you.hunger_state == HS_STARVING
                && you.piety >= 30)
            {
                god_speaks(you.religion, "Your stomach feels content.");
                set_hunger(6000, true);
                lose_piety(5 + random2avg(10, 2) + (you.gift_timeout? 5 : 0));
                inc_gift_timeout(30 + random2avg(10, 2));
            }
            break;

        case GOD_NEMELEX_XOBEH:
            if (random2(200) <= you.piety
                && one_chance_in(3)
                && !you.attribute[ATTR_CARD_COUNTDOWN]
                && !grid_destroys_items(grd[you.x_pos][you.y_pos]))
            {
                misc_item_type gift_type;
                if ( random2(200) <= you.piety )
                {
                    // make a pure deck
                    const misc_item_type pure_decks[] = {
                        MISC_DECK_OF_ESCAPE,
                        MISC_DECK_OF_DESTRUCTION,
                        MISC_DECK_OF_DUNGEONS,
                        MISC_DECK_OF_SUMMONING,
                        MISC_DECK_OF_WONDERS
                    };
                    gift_type = RANDOM_ELEMENT(pure_decks);
                }
                else
                {
                    // make a mixed deck
                    const misc_item_type mixed_decks[] = {
                        MISC_DECK_OF_WAR,
                        MISC_DECK_OF_CHANGES,
                        MISC_DECK_OF_DEFENSE
                    };
                    gift_type = RANDOM_ELEMENT(mixed_decks);
                }

                int thing_created = items( 1, OBJ_MISCELLANY, gift_type, 
                                           true, 1, MAKE_ITEM_RANDOM_RACE );

                if (thing_created != NON_ITEM)
                {
                    move_item_to_grid( &thing_created, you.x_pos, you.y_pos );
                    origin_acquired(mitm[thing_created], you.religion);
                    
                    simple_god_message(" grants you a gift!");
                    more();
                    canned_msg(MSG_SOMETHING_APPEARS);

                    you.attribute[ATTR_CARD_COUNTDOWN] = 10;
                    inc_gift_timeout(5 + random2avg(9, 2));
                    you.num_gifts[you.religion]++;
                    take_note(Note(NOTE_GOD_GIFT, you.religion));
                }
            }
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
                int thing_called = random_undead_servant(GOD_YREDELEMNUL);
                if (create_monster( thing_called, 0, BEH_FRIENDLY, 
                                    you.x_pos, you.y_pos, 
                                    you.pet_target, MAKE_ITEM_RANDOM_RACE ) != -1)
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
        } 
    }                           // end of gift giving
}

static bool is_risky_sacrifice(const item_def& item)
{
    return item.base_type == OBJ_ORBS || is_rune(item);
}

static bool confirm_pray_sacrifice()
{
    for ( int i = igrd[you.x_pos][you.y_pos]; i != NON_ITEM;
          i = mitm[i].link )
    {
        const item_def& item = mitm[i];
        if ( is_risky_sacrifice(item) ||
             has_warning_inscription(item, OPER_PRAY) )
        {
            std::string prompt = "Really sacrifice ";
            prompt += item.name(DESC_NOCAP_A);
            prompt += '?';
            if ( !yesno(prompt.c_str(), false, 'n') )
                return false;
        }
    }
    return true;
}

std::string god_prayer_reaction()
{
    std::string result;
    result += god_name(you.religion);
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
    const bool was_praying = (you.duration[DUR_PRAYER] != 0);

    if (silenced(you.x_pos, you.y_pos))
    {
        mpr("You are unable to make a sound!");
        return;
    }

    // almost all prayers take time
    you.turn_is_over = true;

    const god_type altar_god = grid_altar_god(grd[you.x_pos][you.y_pos]);
    if (you.religion != GOD_NO_GOD && altar_god == you.religion)
    {
        altar_prayer();
    }
    else if (altar_god != GOD_NO_GOD)
    {
        if (you.species == SP_DEMIGOD)
        {
            mpr("Sorry, a being of your status cannot worship here.");
            return;
        }
        god_pitch( grid_altar_god(grd[you.x_pos][you.y_pos]) );
        return;
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
        return;

    mprf(MSGCH_PRAY, "You offer a prayer to %s.", god_name(you.religion));

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

#if DEBUG_DIAGNOSTICS
    mprf(MSGCH_DIAGNOSTICS, "piety: %d", you.piety );
#endif

    if (!was_praying)
        do_god_gift();
}                               // end pray()

const char *god_name( god_type which_god, bool long_name ) // mv - rewritten
{
    static char godname_buff[80];

    switch (which_god)
    {
    case GOD_NO_GOD:
        sprintf(godname_buff, "No God");
        break;
    case GOD_RANDOM:
        sprintf(godname_buff, "random");
        break;
    case GOD_ZIN:
        sprintf(godname_buff, "Zin%s", long_name ? " the Law-Giver" : "");
        break;
    case GOD_SHINING_ONE:
        strcpy(godname_buff, "The Shining One");
        break;
    case GOD_KIKUBAAQUDGHA:
        strcpy(godname_buff, "Kikubaaqudgha");
        break;
    case GOD_YREDELEMNUL:
        sprintf(godname_buff, "Yredelemnul%s", long_name ? " the Dark" : "");
        break;
    case GOD_XOM:
        strcpy(godname_buff, "Xom");
        if (long_name) 
        {
            strcat(godname_buff, " ");
            switch(random2(30))
            {
            default:
                strcat(godname_buff, "of Chaos"); 
                break;
            case 1:
                strcat(godname_buff, "the Random");
                if (coinflip())
                    strcat(godname_buff, coinflip()?"master":" Number God");
                break;
            case 2:
                strcat(godname_buff, "the Tricky"); 
                break;
            case 3:
                sprintf( godname_buff, "Xom the %sredictable", coinflip() ? "Less-P" 
                                                                    : "Unp" );
                break;
            case 4:
                strcat(godname_buff, "of Many Doors"); 
                break;
            case 5:
                strcat(godname_buff, "the Capricious"); 
                break;
            case 6:
                strcat(godname_buff, "of ");
                strcat(godname_buff, coinflip() ? "Bloodstained" : "Enforced");
                strcat(godname_buff, " Whimsey");
                break;
            case 7:
                strcat(godname_buff, "\"What was your username?\" *clickity-click*");
                break;
            case 8:
                strcat(godname_buff, "of Bone-Dry Humour"); 
                break;
            case 9:
                strcat(godname_buff, "of ");
                strcat(godname_buff, coinflip() ? "Malevolent" : "Malicious");
                strcat(godname_buff, " Giggling");
                break;
            case 10:
                strcat(godname_buff, "the Psycho");
                strcat(godname_buff, coinflip() ? "tic" : "path");
                break;
            case 11:
                strcat(godname_buff, "of ");
                switch(random2(5)) 
                {
                case 0: strcat(godname_buff, "Gnomic"); break;
                case 1: strcat(godname_buff, "Ineffable"); break;
                case 2: strcat(godname_buff, "Fickle"); break;
                case 3: strcat(godname_buff, "Swiftly Tilting"); break;
                case 4: strcat(godname_buff, "Unknown"); break;
                }
                strcat(godname_buff, " Intent");
                if (coinflip()) 
                    strcat(godname_buff, "ion");
                break;
            case 12:
                sprintf(godname_buff, "The Xom-Meister");
                if (coinflip())
                    strcat(godname_buff, ", Xom-a-lom-a-ding-dong");
                else if (coinflip())
                    strcat(godname_buff, ", Xom-o-Rama");
                else if (coinflip())
                    strcat(godname_buff, ", Xom-Xom-bo-Bom, Banana-Fana-fo-Fom");
                break;
            case 13:
                strcat(godname_buff, "the Begetter of ");
                strcat(godname_buff, coinflip() ? "Turbulence" : "Discontinuities");
                break;
            }
        }
        break;
    case GOD_VEHUMET:
        strcpy(godname_buff, "Vehumet");
        break;
    case GOD_OKAWARU:
        sprintf(godname_buff, "%sOkawaru", long_name ? "Warmaster " : "");
        break;
    case GOD_MAKHLEB:
        sprintf(godname_buff, "Makhleb%s", long_name ? " the Destroyer" : "");
        break;
    case GOD_SIF_MUNA:
        sprintf(godname_buff, "Sif Muna%s", long_name ? " the Loreminder" : "");
        break;
    case GOD_TROG:
        sprintf(godname_buff, "Trog%s", long_name ? " the Wrathful" : "");
        break;
    case GOD_NEMELEX_XOBEH:
        strcpy(godname_buff, "Nemelex Xobeh");
        break;
    case GOD_ELYVILON:
        sprintf(godname_buff, "Elyvilon%s", long_name ? " the Healer" : "");
        break;
    case GOD_LUGONU:
        sprintf(godname_buff, "Lugonu%s", long_name? " the Unformed" : "");
        break;
    case GOD_BEOGH:
        sprintf(godname_buff, "Beogh%s", long_name? " the Brigand" : "");
        break;
    default:
        sprintf(godname_buff, "The Buggy One (%d)", which_god);
    }

    return (godname_buff);
}                               // end god_name()

void god_speaks( god_type god, const char *mesg )
{
    mpr( mesg, MSGCH_GOD, god );
}                               // end god_speaks()

// This function is the merger of done_good() and naughty().
// Returns true if god was interested (good or bad) in conduct.
bool did_god_conduct( conduct_type thing_done, int level )
{
    bool ret = false;
    int piety_change = 0;
    int penance = 0;

    if (you.religion == GOD_NO_GOD || you.religion == GOD_XOM)
        return (false);

    switch (thing_done)
    {
    case DID_NECROMANCY:
    case DID_UNHOLY:
    case DID_ATTACK_HOLY:
        switch (you.religion)
        {
        case GOD_ZIN:
        case GOD_SHINING_ONE:
        case GOD_ELYVILON:
            piety_change = -level;
            penance = level * ((you.religion == GOD_ZIN) ? 2 : 1);
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
        switch (you.religion)
        {
        case GOD_ZIN:
        case GOD_SHINING_ONE:
        case GOD_ELYVILON:
        case GOD_OKAWARU:
        case GOD_BEOGH: // added penance to avoid killings for loot
                        // deliberately no extra punishment for killing
            piety_change = -level;
            penance = level * 3;
            ret = true;
            break;
        default:
            break;
        }
        break;

    case DID_FRIEND_DIES:
        switch (you.religion)
        {
        case GOD_ELYVILON:
            penance = level;    // healer god cares more about this
            // fall through
        case GOD_ZIN:
        case GOD_SHINING_ONE:
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

    case DID_DEDICATED_KILL_LIVING:
        switch (you.religion)
        {
        case GOD_ELYVILON:
            simple_god_message(" did not appreciate that!");
            ret = true;
            piety_change = -level;
            penance = level * 2;
            break;

        case GOD_KIKUBAAQUDGHA:
        case GOD_YREDELEMNUL:
        case GOD_OKAWARU:
        case GOD_VEHUMET:
        case GOD_MAKHLEB:
        case GOD_TROG:
        case GOD_BEOGH:
        case GOD_LUGONU:
            simple_god_message(" accepts your kill.");
            ret = true;
            if (random2(level + 18 - you.experience_level / 2) > 5)
                piety_change = 1;
            break;

        default:
            break;
        }
        break;

    case DID_DEDICATED_KILL_UNDEAD:
        switch (you.religion)
        {
        case GOD_ZIN:
        case GOD_SHINING_ONE:
        case GOD_OKAWARU:
        case GOD_VEHUMET:
        case GOD_LUGONU:
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

    case DID_DEDICATED_KILL_DEMON:  
        switch (you.religion)
        {
        case GOD_ZIN:
        case GOD_SHINING_ONE:
        case GOD_OKAWARU:
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

    case DID_DEDICATED_KILL_PRIEST:
        if (you.religion == GOD_BEOGH)
        {
            simple_god_message(" appreciates your killing of a "
                               "heretic priest.");
            ret = true;
            if (random2(level + 10) > 5)
                piety_change = 1;
        }
        break;
        
    case DID_DEDICATED_KILL_WIZARD:
        if (you.religion == GOD_TROG) 
        {
            // hooking this up, but is it too good?  
            // enjoy it while you can -- bwr
            simple_god_message(" appreciates your killing of a magic user.");
            ret = true;
            if (random2(level + 10) > 5)
                piety_change = 1;
        }
        break;

    // Note that Angel deaths are special, they are always noticed... 
    // if you or any friendly kills one you'll get the credit or the blame.
    case DID_ANGEL_KILLED_BY_SERVANT:
    case DID_KILL_ANGEL:
        switch (you.religion)
        {
        case GOD_ZIN:
        case GOD_SHINING_ONE:
        case GOD_ELYVILON:
            level *= 3;
            piety_change = -level;
            penance = level * ((you.religion == GOD_ZIN) ? 2 : 1);
            ret = true;
            break;

        case GOD_KIKUBAAQUDGHA:
        case GOD_YREDELEMNUL:
        case GOD_MAKHLEB:
        case GOD_LUGONU:
            snprintf( info, INFO_SIZE, " accepts your %skill.",
                      (thing_done == DID_KILL_ANGEL) ? "" : "collateral " );

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
        case GOD_ZIN:
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
        case GOD_ZIN:
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
        }
        break;

    case DID_STIMULANTS:                        // unused
    case DID_EAT_MEAT:                          // unused
    case DID_CREATED_LIFE:                      // unused
    case DID_DEDICATED_KILL_NATURAL_EVIL:       // unused
    case DID_NATURAL_EVIL_KILLED_BY_SERVANT:    // unused
    case DID_SPELL_NONUTILITY:                  // unused
    case NUM_CONDUCTS:
        break;
    }

    if (piety_change > 0)
        gain_piety( piety_change );
    else 
    {
        const int piety_loss = -piety_change;

        if (piety_loss)
        {
            // output guilt message:
            mprf( "You feel%sguilty.",
                    (piety_loss == 1) ? " a little " :
                    (piety_loss <  5) ? " " :
                    (piety_loss < 10) ? " very "
                                      : " extremely " );

            lose_piety( piety_loss );
        }

        if (you.piety < 1)
            excommunication();
        else if (penance)       // only if still in religion
        {
            god_speaks( you.religion, 
                        "\"You will pay for your transgression, mortal!\"" );

            inc_penance( penance );
        }
    }

#if DEBUG_DIAGNOSTICS
    if (ret)
    {
        static const char *conducts[] = 
        {
          "",
          "Necromancy", "Unholy", "Attack Holy", "Attack Friend",
          "Friend Died", "Stab", "Poison", "Field Sacrifice",
          "Kill Living", "Kill Undead", "Kill Demon", "Kill Natural Evil",
          "Kill Wizard",
          "Kill Priest", "Kill Angel", "Undead Slave Kill Living", 
          "Servant Kill Living", "Servant Kill Undead", 
          "Servant Kill Demon", "Servant Kill Natural Evil", 
          "Servant Kill Angel",
          "Spell Memorise", "Spell Cast", "Spell Practise", "Spell Nonutility",
          "Cards", "Stimulants", "Eat Meat", "Create Life" 
        };

        mprf( MSGCH_DIAGNOSTICS, 
             "conduct: %s; piety: %d (%+d); penance: %d (%+d)",
             conducts[thing_done], 
             you.piety, piety_change, you.penance[you.religion], penance );

    }
#endif

    return (ret);
}

void gain_piety(int pgn)
{
    // Xom uses piety differently...
    if (you.religion == GOD_XOM)
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
        if (!one_chance_in(8))
            return;
    }

    // slow down gain at upper levels of piety
    if (you.religion != GOD_SIF_MUNA)
    {
        if (you.piety > 199
            || (you.piety > 150 && one_chance_in(3))
            || (you.piety > 100 && one_chance_in(3)))
            return;
    }
    else
    {
        // Sif Muna has a gentler taper off because training becomes
        // naturally slower as the player gains in spell skills.
        if ((you.piety > 199) ||
            (you.piety > 150 && one_chance_in(5)))
        {
            do_god_gift();
            return;
        }
    }

    int old_piety = you.piety;

    you.piety += pgn;

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
            if (you.religion == GOD_BEOGH)
            {
                // every piety level change also affects AC from orcish gear
                you.redraw_armour_class = 1;
            }
        }
    }

    if ( you.piety > 160 && old_piety <= 160 &&
         (you.religion == GOD_SHINING_ONE || you.religion == GOD_ZIN ||
          you.religion == GOD_LUGONU) && you.num_gifts[you.religion] == 0 )
        simple_god_message( " will now bless your weapon at an altar...once.");

    if (you.religion == GOD_SIF_MUNA)
        do_god_gift();
}

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
        !player_is_levitating() && you.species != SP_MERFOLK &&
        grd[you.x_pos][you.y_pos] == DNGN_DEEP_WATER;
}

void lose_piety(int pgn)
{
    const int old_piety = you.piety;

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
            (you.religion == GOD_SHINING_ONE || you.religion == GOD_ZIN ||
             you.religion == GOD_LUGONU) && you.num_gifts[you.religion] == 0)
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

                if ( need_water_walking() && !beogh_water_walk() )
                {
                    fall_into_a_pool( you.x_pos, you.y_pos, true,
                                      grd[you.x_pos][you.y_pos] );
                }

                if ( you.religion == GOD_BEOGH )
                {
                    // every piety level change also affects AC from
                    // orcish gear
                    you.redraw_armour_class = 1;
                }
            }
        }
    }
}

static bool tso_retribution()
{
    const god_type god = GOD_SHINING_ONE;
    // daeva/smiting theme

    // Doesn't care unless you've gone over to evil/destructive gods
    if ( !is_evil_god(you.religion) )
        return false;

    if (coinflip())
    {
        bool success = false;
        int how_many = 1 + random2(you.experience_level / 5) + random2(3);
        
        for (int i = 0; i < how_many; i++)
            if (create_monster( MONS_DAEVA, 0, BEH_HOSTILE,
                                you.x_pos, you.y_pos, MHITYOU, 250) != -1)
                success = true;

        simple_god_message( success ?
                            " sends the divine host to punish you "
                            "for your evil ways!" :
                            "'s divine host fails to appear.",
                            god );
    }
    else
    {
        if (!player_under_penance() && you.piety > random2(400))
        {
            god_speaks(you.religion,
                       "Mortal, I have averted the wrath of "
                       "the Shining One... this time.");
        }
        else
        {
            int divine_hurt = 10 + random2(10);            
            for (int i = 0; i < 5; i++)
                divine_hurt += random2( you.experience_level );

            simple_god_message( " smites you!", god );
            ouch( divine_hurt, 0, KILLED_BY_TSO_SMITING );
            dec_penance( god, 1 );
        }
    }

    return false;
}

static bool zin_retribution()
{
    const god_type god = GOD_ZIN;

    // angels/creeping doom theme:
    // Doesn't care unless you've gone over to evil
    if (!is_evil_god(you.religion))
        return false;

    if (random2(you.experience_level) > 7 && !one_chance_in(5))
    {
        bool success = false;
        const int how_many = 1 + (you.experience_level / 10) + random2(3);

        for (int i = 0; i < how_many; i++)
            if (create_monster(MONS_ANGEL, 0, BEH_HOSTILE,
                               you.x_pos, you.y_pos, MHITYOU, 250) != -1)
                success = true;

        simple_god_message( success ?
                            " sends the divine host to punish you "
                            "for your evil ways!" :
                            "'s divine host fails to appear.",
                            god);
    }
    else
    {
        // god_gift == false gives unfriendly
        summon_swarm( you.experience_level * 20, true, false );
        simple_god_message(" sends a plague down upon you!", god);
    }

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
                                            MHITYOU, 250) != -1;
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
                               you.x_pos, you.y_pos, MHITYOU, 250) != -1)
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
                               you.y_pos, MHITYOU, 250) != -1)
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
        bool success = false;
        int how_many = 1 + random2(1 + (you.experience_level / 5));

        for (int i = 0; i < how_many; i++)
        {
            const int temp_rand = random2(100);

            const monster_type punisher =
                ((temp_rand > 66) ? MONS_WRAITH :            // 33%
                 (temp_rand > 52) ? MONS_WIGHT :             // 12%
                 (temp_rand > 40) ? MONS_SPECTRAL_WARRIOR :  // 16%
                 (temp_rand > 31) ? MONS_ROTTING_HULK :      //  9%
                 (temp_rand > 23) ? MONS_SKELETAL_WARRIOR :  //  8%
                 (temp_rand > 16) ? MONS_VAMPIRE :           //  7%
                 (temp_rand > 10) ? MONS_GHOUL :             //  6%
                 (temp_rand >  4) ? MONS_MUMMY               //  6%
                 : MONS_FLAYED_GHOST);      //  5%

            if (create_monster( punisher, 0, BEH_HOSTILE, 
                                you.x_pos, you.y_pos, MHITYOU, 250 ) != -1)
                success = true;

        }

        simple_god_message(success ?
                           " sends a servant to punish you." :
                           "'s servant fails to arrive.",
                           god);
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
        // Would be better if berserking monsters were available,
        // we just send some big bruisers for now.
        bool success = false;
        int points = 3 + you.experience_level * 3;

        while (points > 0)
        {
            monster_type punisher = MONS_PROGRAM_BUG;

            if (points > 20 && coinflip())
            {
                // quick reduction for large values
                punisher = MONS_DEEP_TROLL;
                points -= 15;
                continue;
            }
            else
            {
                const monster_type punishers[10] = {
                    MONS_IRON_TROLL, MONS_ROCK_TROLL, MONS_TROLL,
                    MONS_MINOTAUR, MONS_MINOTAUR, MONS_TWO_HEADED_OGRE, 
                    MONS_TWO_HEADED_OGRE, MONS_OGRE, MONS_OGRE, MONS_OGRE
                };
                const int costs[10] = { 10, 10, 6, 3, 3, 4, 4, 3, 3, 3 };
                const int idx = random2(10);
                
                punisher = punishers[idx];
                points -= costs[idx];
            }

            if (create_monster(punisher, 0, BEH_HOSTILE, you.x_pos,
                               you.y_pos, MHITYOU, 250) != -1)
                success = true;
        }
        
        simple_god_message(success ?
                           " sends monsters to punish you." :
                           " has no time to punish you...now.",
                           god);
    }
    else if ( !one_chance_in(3) )
    {
        simple_god_message("'s voice booms out, \"Feel my wrath!\"", god );

        // A collection of physical effects that might be better
        // suited to Trog than wild fire magic... messages could
        // be better here... something more along the lines of apathy
        // or loss of rage to go with the anti-berzerk effect-- bwr
        switch (random2(6))
        {
        case 0:
            potion_effect( POT_DECAY, 100 );
            break;

        case 1:
        case 2:
            lose_stat(STAT_STRENGTH, 1 + random2(you.strength / 5), true);
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
    {
        int divine_hurt = 10 + random2(10);
            
        for (int i = 0; i < 5; i++)
            divine_hurt += random2( you.experience_level );
            
        if (!player_under_penance() && you.piety > random2(400))
        {
            snprintf(info, INFO_SIZE, "Mortal, I have averted the wrath "
                     "of %s... this time.", god_name(god));
            god_speaks(you.religion, info);
        }
        else
        {
            simple_god_message( " smites you!", god );
            ouch( divine_hurt, 0, KILLED_BY_BEOGH_SMITING );
            dec_penance( god, 1 );
        }
        break;
    }

    // taken from makeitem.cc and spells3.cc:
    case 2: // send out one or two dancing weapons of orc slaying (12.5%)
    {
        int num_created = 0;
        int num_to_create = random2(2) + 1;
        for (int i = 0; i < num_to_create; i++)
        {
            bool created = false;

            // first create item
            const int it = get_item_slot();
            if (it != NON_ITEM)
            {
                item_def &item = mitm[it];
                    
                item.quantity = 1;
                item.base_type = OBJ_WEAPONS;
                // any melee weapon
                item.sub_type = WPN_CLUB + random2(13); 
                    
                set_item_ego_type( item, OBJ_WEAPONS, SPWPN_ORC_SLAYING );
                // just how good should this weapon be?
                item.plus = random2(3);
                item.plus2 = random2(3);
                      
                if (coinflip())
                    item.flags |= ISFLAG_CURSED;

                set_ident_type( item.base_type, item.sub_type,
                                ID_KNOWN_TYPE );
                    
                // for debugging + makes things more interesting
                // (doesn't seem to have any effect, though)
                set_ident_flags( item, ISFLAG_KNOW_PLUSES );
                set_ident_flags( item, ISFLAG_IDENT_MASK );

                // now create monster
                int mons =
                    create_monster( MONS_DANCING_WEAPON, 0,
                                    BEH_HOSTILE, you.x_pos, you.y_pos,
                                    MHITYOU, 250 );
                                       
                // hand item information over to monster
                if (mons != -1 && mons != NON_MONSTER)
                {
                    mitm[it] = item;
                    mitm[it].quantity = 1;
                    mitm[it].x = 0;
                    mitm[it].y = 0;
                    mitm[it].link = NON_ITEM;
                    menv[mons].inv[MSLOT_WEAPON] = it;
                    created = true;
                    num_created++;

                    // 50% chance of weapon disappearing on "death"
                    if (coinflip())
                        menv[mons].flags |= MF_HARD_RESET;
                }
            }
            if (!created) // didn't work out! delete item
            {
                mitm[it].base_type = OBJ_UNASSIGNED;
                mitm[it].quantity = 0;
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
        if (followers_abandon_you()) 
            break;
        // else fall through
    default: // send orcs after you (3/8 to 5/8)
    {
        int points = you.experience_level + 3
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

        bool success = (create_monster(punisher, 0, BEH_HOSTILE, you.x_pos,
                                       you.y_pos, MHITYOU, 250, true) != -1);

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
        const int temp_rand = random2(100);

        monster_type punisher = ((temp_rand > 84) ? MONS_ORC_WARRIOR :
                                 (temp_rand > 69) ? MONS_ORC_KNIGHT :
                                 (temp_rand > 59) ? MONS_NAGA_WARRIOR :
                                 (temp_rand > 49) ? MONS_CENTAUR_WARRIOR :
                                 (temp_rand > 39) ? MONS_STONE_GIANT :
                                 (temp_rand > 29) ? MONS_FIRE_GIANT :
                                 (temp_rand > 19) ? MONS_FROST_GIANT :
                                 (temp_rand >  9) ? MONS_CYCLOPS :
                                 (temp_rand >  4) ? MONS_HILL_GIANT 
                                 : MONS_TITAN);

        if (create_monster(punisher, 0, BEH_HOSTILE,
                           you.x_pos, you.y_pos, MHITYOU, 250) != -1)
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
        lose_stat(STAT_INTELLIGENCE, 1 + random2( you.intel / 5 ), true);
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
                           MHITYOU, 250) != -1)
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
                               you.x_pos, you.y_pos, MHITYOU, 250) != -1)
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

void divine_retribution( god_type god )
{
    ASSERT(god != GOD_NO_GOD);

    // Good gods don't use divine retribution on their followers, they
    // will consider it for those who have gone astray however.
    if (god == you.religion && is_good_god(god) )
        return;

    // Just the thought of retribution (getting this far) mollifies
    // the god by at least a point... the punishment might reduce
    // penance further.
    dec_penance( god, 1 + random2(3) );

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

    case GOD_ELYVILON:  // Elyvilon doesn't seek revenge
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
}

// upon excommunication, (now ex) Beogh adepts lose their orcish followers
int followers_abandon_you()
{
     int ystart = you.y_pos - 9, xstart = you.x_pos - 9;
     int yend = you.y_pos + 9, xend = you.x_pos + 9;
     if ( xstart < 0 ) xstart = 0;
     if ( ystart < 0 ) ystart = 0;
     if ( xend >= GXM ) xend = GXM;
     if ( ystart >= GYM ) yend = GYM;
     
     bool reconvert = false;
     int num_reconvert = 0;
     int num_followers = 0;

     std::vector<const monsters *> mons;
     // monster check
     for ( int y = ystart; y < yend; ++y )
     {
         for ( int x = xstart; x < xend; ++x )
         {

             const unsigned char targ_monst = mgrd[x][y];
             if ( targ_monst != NON_MONSTER )
             {
                 struct monsters *monster = &menv[targ_monst];
                 if ( mons_species(monster->type) == MONS_ORC
                      && monster->attitude == ATT_FRIENDLY
                      && (monster->flags & MF_CONVERT_ATTEMPT))
                 {

                     num_followers++;
                     
                     if (mons_player_visible(monster)
                         && !mons_is_confused(monster)
                         && !mons_is_paralysed(monster))
                     {
                        monster->attitude = ATT_HOSTILE;
                        monster->behaviour = BEH_HOSTILE;
                        // for now CREATED_FRIENDLY stays

                        if (player_monster_visible(monster))
                        {
                           num_reconvert++; // only visible ones
                        }
                        reconvert = true;
                     }
                 }
             }
         }
     }
     if (reconvert) // maybe all of them invisible
     {
        snprintf(info, INFO_SIZE, "%s booms out: \"Who do you think you are?\"",
                   god_name(GOD_BEOGH));
        god_speaks(GOD_BEOGH, info);

        if (num_reconvert > 0)
        {
           std::string how_many;
           if (num_reconvert == 1 and num_followers > 1)
              how_many = "One of your";
           else if (num_reconvert == num_followers)
              how_many = "Your";
           else
              how_many = "Some of your";
              
           snprintf(info, INFO_SIZE, "%s follower%s decide%s to abandon you.",
                how_many.c_str(),
                (num_reconvert > 1) ? "s" : "",
                (num_reconvert > 1) ? "" : "s");
           mpr(info, MSGCH_MONSTER_ENCHANT);
           return 1;
        }
     }
     return 0;
}

void excommunication(void)
{
    const god_type old_god = you.religion;

    take_note(Note(NOTE_LOSE_GOD, old_god));

    you.duration[DUR_PRAYER] = 0;
    you.religion = GOD_NO_GOD;
    you.piety = 0;
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

        // Penance has to come before retribution to prevent "mollify"
        inc_penance( old_god, 50 );
        divine_retribution( old_god );
        break;

    case GOD_BEOGH:
        simple_god_message( " does not appreciate desertion!", old_god );
        followers_abandon_you(); // check if friendly orcs around -> hostile

        // You might have lost water walking at a bad time...
        if ( need_water_walking() )
            fall_into_a_pool( you.x_pos, you.y_pos, true,
                              grd[you.x_pos][you.y_pos] );

        // Penance has to come before retribution to prevent "mollify"
        inc_penance( old_god, 50 );
        divine_retribution( old_god );
        break;

    // these like to haunt players for a bit more than the standard
    case GOD_NEMELEX_XOBEH:
    case GOD_SIF_MUNA:
        inc_penance( old_god, 50 );
        break;

    case GOD_LUGONU:
        if ( you.level_type == LEVEL_DUNGEON )
        {
            simple_god_message(" casts you back into the Abyss!", old_god);
            banished(DNGN_ENTER_ABYSS, "Lugonu's wrath");
        }
        inc_penance(old_god, 50);
        break;

    case GOD_ELYVILON:  // never seeks revenge
        break;

    default:
        inc_penance( old_god, 25 );
        break;
    }
}                               // end excommunication()

static bool bless_weapon( int god, int brand, int colour )
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

        if ( god != GOD_LUGONU )
            holy_word( 100, true );

        delay(1000);

        return (true);
    }

    return (false);
}

static std::string sacrifice_message(god_type god, const item_def &item)
{
    const char *msg = sacrifice[god];
    const char *be = item.quantity == 1? "is" : "are";

    // "The bread ration evaporates", "The arrows evaporate" - the s suffix
    // is for verbs.
    const char *ssuffix = item.quantity == 1? "s" : "";

    return (replace_all(replace_all(msg, "%", ssuffix), "&", be));
}

void altar_prayer(void)
{
    mpr( "You kneel at the altar and pray." );

    if (you.religion == GOD_XOM)
        return;

    // TSO blesses long swords with holy wrath
    if (you.religion == GOD_SHINING_ONE
        && !you.num_gifts[GOD_SHINING_ONE]
        && !player_under_penance()
        && you.piety > 160)
    {
        const int wpn = get_player_wielded_weapon();

        if (wpn != -1 
            && weapon_skill( you.inv[wpn] ) == SK_LONG_SWORDS
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

    // Zin blesses maces with disruption
    if (you.religion == GOD_ZIN
        && !you.num_gifts[GOD_ZIN]
        && !player_under_penance()
        && you.piety > 160)
    {
        const int wpn = get_player_wielded_weapon();

        if (wpn != -1 
            && (you.inv[wpn].base_type == OBJ_WEAPONS
                && (you.inv[wpn].sub_type == WPN_MACE
                    || you.inv[wpn].sub_type == WPN_GREAT_MACE))
            && get_weapon_brand( you.inv[wpn] ) != SPWPN_DISRUPTION)
        {
            bless_weapon( GOD_ZIN, SPWPN_DISRUPTION, WHITE );
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

void offer_items()
{
    if (you.religion == GOD_NO_GOD)
        return;
      
    int i = igrd[you.x_pos][you.y_pos];
    while (i != NON_ITEM)
    {
        if (one_chance_in(1000))
            break;

        const int next = mitm[i].link;  // in case we can't get it later.

        const int value = item_value( mitm[i], true );

        switch (you.religion)
        {
        case GOD_ZIN:
        case GOD_OKAWARU:
        case GOD_MAKHLEB:
        case GOD_NEMELEX_XOBEH:
            mprf(MSGCH_GOD, "%s%s", mitm[i].name(DESC_CAP_THE).c_str(),
                 sacrifice_message(you.religion, mitm[i]).c_str());

#ifdef DEBUG_DIAGNOSTICS
            mprf(MSGCH_DIAGNOSTICS, "Sacrifice item value: %d", value);
#endif
            if (mitm[i].base_type == OBJ_CORPSES 
                || random2(value) >= 50
                || (you.religion == GOD_NEMELEX_XOBEH && one_chance_in(50))
                || player_under_penance())
            {
                gain_piety(1);
            }

            destroy_item(i);
            break;

        case GOD_SIF_MUNA:
            mprf(MSGCH_GOD, "%s%s", mitm[i].name(DESC_CAP_THE).c_str(),
                 sacrifice_message(you.religion, mitm[i]).c_str());

            if (value >= 150)
                gain_piety(1 + random2(3));

            destroy_item(i);
            break;

        case GOD_KIKUBAAQUDGHA:
        case GOD_TROG:
            if (mitm[i].base_type != OBJ_CORPSES)
                break;

            mprf(MSGCH_GOD, "%s%s", mitm[i].name(DESC_CAP_THE).c_str(),
                 sacrifice_message(you.religion, mitm[i]).c_str());

            gain_piety(1);
            destroy_item(i);
            break;

        case GOD_ELYVILON:
            if (mitm[i].base_type != OBJ_WEAPONS
                && mitm[i].base_type != OBJ_MISSILES)
            {
                break;
            }

            mprf(MSGCH_GOD, "%s%s", mitm[i].name(DESC_CAP_THE).c_str(),
                 sacrifice_message(you.religion, mitm[i]).c_str());

            if (random2(value) >= random2(50) 
                || (mitm[i].base_type == OBJ_WEAPONS 
                    && (you.piety < 30 || player_under_penance())))
            {
                gain_piety(1);
            }

            destroy_item(i);
            break;

        default:
            break;
        }

        i = next;
    }
}

void god_pitch(god_type which_god)
{
    mprf("You kneel at the altar of %s.", god_name(which_god));
    more();

    // Note: using worship we could make some gods not allow followers to
    // return, or not allow worshippers from other religions.  -- bwr

    if ((you.is_undead || you.species == SP_DEMONSPAWN)
        && (which_god == GOD_ZIN || which_god == GOD_SHINING_ONE
            || which_god == GOD_ELYVILON) ||
          which_god == GOD_BEOGH && you.species != SP_HILL_ORC)
    {
        simple_god_message(" does not accept worship from those such as you!",
                           which_god);
        return;
    }

    if (which_god == GOD_LUGONU && you.penance[GOD_LUGONU])
    {
        simple_god_message(" is most displeased with you!", which_god);
        lugonu_retribution();
        return;
    }

    describe_god( which_god, false );

    snprintf( info, INFO_SIZE, "Do you wish to %sjoin this religion?", 
              (you.worshipped[which_god]) ? "re" : "" );

    if (!yesno( info ))
    {
        redraw_screen();
        return;
    }

    if (!yesno("Are you sure?"))
    {
        redraw_screen();
        return;
    }

    redraw_screen();
    if (you.religion != GOD_NO_GOD)
        excommunication();

    //jmf: moved up so god_speaks gives right colour
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

    if (you.worshipped[you.religion] < 100)
        you.worshipped[you.religion]++;

    take_note(Note(NOTE_GET_GOD, you.religion));

    // Currently penance is just zeroed, this could be much more interesting.
    you.penance[you.religion] = 0;

    if (is_evil_god(you.religion))
    {
        // Note:  Using worshipped[] we could make this sort of grudge
        // permanent instead of based off of penance. -- bwr
        if (you.penance[GOD_SHINING_ONE] > 0)
        {
            inc_penance(GOD_SHINING_ONE, 30);
            god_speaks(GOD_SHINING_ONE, "\"You will pay for your evil ways, mortal!\"");
        }
    }

    if ( you.religion == GOD_LUGONU )
        gain_piety(20);         // allow instant access to first power

    redraw_skill( you.your_name, player_title() );
}                               // end god_pitch()

bool god_likes_butchery(god_type god)
{
    return (you.religion == GOD_OKAWARU || you.religion == GOD_MAKHLEB ||
            you.religion == GOD_TROG || you.religion == GOD_BEOGH ||
            you.religion == GOD_LUGONU);
}

bool god_hates_butchery(god_type god)
{
    return (god == GOD_ELYVILON);
}

void offer_corpse(int corpse)
{
    mprf(MSGCH_GOD, "%s%s", mitm[corpse].name(DESC_CAP_THE).c_str(),
         sacrifice_message(you.religion, mitm[corpse]).c_str());

    did_god_conduct(DID_DEDICATED_BUTCHERY, 10);
}                               // end offer_corpse()

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
            if (you.penance[i])
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
                mpr("Xom is getting BORED.");
                you.gift_timeout = 0;
            }
            else if (you.gift_timeout > 1)
            {
                you.gift_timeout -= random2(2);
            }

            if (one_chance_in(20))
            {
                // If you.gift_timeout was == 0, then Xom was BORED.
                // He HATES that.
                xom_acts(you.gift_timeout > 0 && you.piety > 100,
                         abs(you.piety - 100));
            }
            break;
        }

        case GOD_ZIN:           // These gods like long-standing worshippers
        case GOD_ELYVILON:
            if (need_free_piety() && one_chance_in(20))
                gain_piety(1);
            break;

        case GOD_SHINING_ONE:
            if (need_free_piety() && one_chance_in(15))
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
    char buff[ INFO_SIZE ];

    if (which_deity == GOD_NO_GOD)
        which_deity = you.religion;

    snprintf( buff, sizeof(buff), "%s%s", god_name( which_deity ), event );

    god_speaks( which_deity, buff );
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
