/*
 *  File:       decks.cc
 *  Summary:    Functions with decks of cards.
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 *
 *  Change History (most recent first):
 *
 *               <1>     -/--/--        LRH             Created
 */

#include "AppHdr.h"
#include "decks.h"

#include <string.h>
#include <iostream>

#include "externs.h"

#include "effects.h"
#include "food.h"
#include "it_use2.h"
#include "items.h"
#include "misc.h"
#include "monplace.h"
#include "mutation.h"
#include "ouch.h"
#include "player.h"
#include "religion.h"
#include "spells1.h"
#include "spells3.h"
#include "spl-cast.h"
#include "stuff.h"

enum card_type
{
    CARD_BLANK = 0,             //    0
    CARD_BUTTERFLY,
    CARD_WRAITH,
    CARD_EXPERIENCE,
    CARD_WEALTH,
    CARD_INTELLIGENCE,          //    5
    CARD_STRENGTH,
    CARD_QUICKSILVER,
    CARD_STUPIDITY,
    CARD_WEAKNESS,
    CARD_SLOTH,                 //   10
    CARD_SHUFFLE,
    CARD_FREAK,
    CARD_DEATH,
    CARD_NORMALITY,
    CARD_SHADOW,                //   15
    CARD_GATE,
    CARD_STATUE,
    CARD_ACQUISITION,
    CARD_HASTEN,
    CARD_DEMON_LESSER,          //   20
    CARD_DEMON_COMMON,
    CARD_DEMON_GREATER,
    CARD_DEMON_SWARM,
    CARD_YAK,
    CARD_FIEND,                 //   25
    CARD_DRAGON,
    CARD_GOLEM,
    CARD_THING_FUGLY,
    CARD_LICH,
    CARD_HORROR_UNSEEN,         //   30
    CARD_BLINK,
    CARD_TELEPORT,
    CARD_TELEPORT_NOW,
    CARD_RAGE,
    CARD_LEVITY,                //   35
    CARD_VENOM,
    CARD_XOM,
    CARD_SLOW,
    CARD_DECAY,
    CARD_HEALING,               //   40
    CARD_HEAL_WOUNDS,
    CARD_TORMENT,
    CARD_FOUNTAIN,
    CARD_ALTAR,
    CARD_FAMINE,                //   45
    CARD_FEAST,
    CARD_WILD_MAGIC,
    CARD_VIOLENCE,
    CARD_PROTECTION,
    CARD_KNOWLEDGE,             //   50
    CARD_MAZE,
    CARD_PANDEMONIUM,
    CARD_IMPRISONMENT,
    CARD_RULES_FOR_BRIDGE,      //   54
    NUM_CARDS,                  // must remain last regular member {dlb}
    CARD_RANDOM = 255           // must remain final member {dlb}
};

static card_type deck_of_wonders[] = 
{
    CARD_BLANK,
    CARD_BUTTERFLY,
    CARD_WRAITH,
    CARD_EXPERIENCE,
    CARD_WEALTH,
    CARD_INTELLIGENCE,
    CARD_STRENGTH,
    CARD_QUICKSILVER,
    CARD_STUPIDITY,
    CARD_WEAKNESS,
    CARD_SLOTH,
    CARD_SHUFFLE,
    CARD_FREAK,
    CARD_DEATH,
    CARD_NORMALITY,
    CARD_SHADOW,
    CARD_GATE,
    CARD_STATUE,
    CARD_ACQUISITION,
    CARD_HASTEN,
    CARD_LICH,
    CARD_XOM,
    CARD_DECAY,
    CARD_ALTAR,
    CARD_FOUNTAIN,
    CARD_MAZE,
    CARD_PANDEMONIUM
};

static card_type deck_of_summoning[] = 
{
    CARD_STATUE,
    CARD_DEMON_LESSER,
    CARD_DEMON_COMMON,
    CARD_DEMON_GREATER,
    CARD_DEMON_SWARM,
    CARD_YAK,
    CARD_FIEND,
    CARD_DRAGON,
    CARD_GOLEM,
    CARD_THING_FUGLY,
    CARD_HORROR_UNSEEN
};

static card_type deck_of_tricks[] = 
{
    CARD_BLANK,
    CARD_BUTTERFLY,
    CARD_BLINK,
    CARD_TELEPORT,
    CARD_TELEPORT_NOW,
    CARD_RAGE,
    CARD_LEVITY,
    CARD_HEALING,
    CARD_WILD_MAGIC,
    CARD_DEMON_LESSER,
    CARD_HASTEN
};

static card_type deck_of_power[] = 
{
    CARD_BLANK,
    CARD_DEMON_COMMON,
    CARD_DEMON_GREATER,
    CARD_TELEPORT_NOW,
    CARD_VENOM,
    CARD_XOM,
    CARD_HEAL_WOUNDS,
    CARD_FAMINE,
    CARD_FEAST,
    CARD_WILD_MAGIC,
    CARD_VIOLENCE,
    CARD_PROTECTION,
    CARD_KNOWLEDGE,
    CARD_HASTEN,
    CARD_TORMENT,
    CARD_DEMON_SWARM,
    CARD_SLOW
};

// Supposed to be bad, small chance of OK... Nemelex wouldn't like a game 
// that didn't have some chance of "losing".
static card_type deck_of_punishment[] = 
{
    CARD_BLANK,
    CARD_BUTTERFLY,
    CARD_WRAITH,
    CARD_WEALTH,
    CARD_STUPIDITY,
    CARD_WEAKNESS,
    CARD_SLOTH,
    CARD_SHUFFLE,
    CARD_FREAK,
    CARD_DEATH,
    CARD_NORMALITY,
    CARD_SHADOW,
    CARD_GATE,
    CARD_DEMON_SWARM,
    CARD_RAGE,
    CARD_VENOM,
    CARD_SLOW,
    CARD_DECAY,
    CARD_TORMENT,
    CARD_FAMINE,
    CARD_WILD_MAGIC,
    CARD_MAZE,
    CARD_PANDEMONIUM
};

#define ARRAYSIZE(x) (sizeof(x) / sizeof(x[0]))
#define DECK_WONDERS_SIZE ARRAYSIZE(deck_of_wonders)
#define DECK_SUMMONING_SIZE ARRAYSIZE(deck_of_summoning)
#define DECK_TRICKS_SIZE ARRAYSIZE(deck_of_tricks)
#define DECK_POWER_SIZE ARRAYSIZE(deck_of_power)
#define DECK_PUNISHMENT_SIZE ARRAYSIZE(deck_of_punishment)

static const char* card_name(card_type card)
{
    switch (card)
    {
    case CARD_BLANK: return "a blank card";
    case CARD_BUTTERFLY: return "Butterfly";
    case CARD_WRAITH: return "the Wraith";
    case CARD_EXPERIENCE: return "Experience";
    case CARD_WEALTH: return "Wealth";
    case CARD_INTELLIGENCE: return "the Brain";
    case CARD_STRENGTH: return "Strength";
    case CARD_QUICKSILVER: return "Quicksilver";
    case CARD_STUPIDITY: return "Stupidity";
    case CARD_WEAKNESS: return "Weakness";
    case CARD_SLOTH: return "the Slug";
    case CARD_SHUFFLE: return "Shuffle";
    case CARD_FREAK: return "the Freak";
    case CARD_DEATH: return "Death";
    case CARD_NORMALITY: return "Normality";
    case CARD_SHADOW: return "the Shadow";
    case CARD_GATE: return "the Gate";
    case CARD_STATUE: return "the Crystal Statue";
    case CARD_ACQUISITION: return "Acquisition";
    case CARD_HASTEN: return "Haste";
    case CARD_DEMON_LESSER: return "a little demon";
    case CARD_DEMON_COMMON: return "a demon";
    case CARD_DEMON_GREATER: return "a huge demon";
    case CARD_DEMON_SWARM: return "a swarm of little demons";
    case CARD_YAK: return "a huge, shaggy yak";
    case CARD_FIEND: return "a huge, scaly devil";
    case CARD_DRAGON: return "a huge, scaly dragon";
    case CARD_GOLEM: return "a statue";
    case CARD_THING_FUGLY: return "a very ugly thing";
    case CARD_LICH: return "a very irritated-looking skeletal thing";
    case CARD_HORROR_UNSEEN:
        return player_see_invis() ? "a hideous abomination" : "a blank card";
    case CARD_BLINK: return "Blink";
    case CARD_TELEPORT: return "the Portal of Delayed Transposition";
    case CARD_TELEPORT_NOW: return "the Portal of Instantaneous Transposition";
    case CARD_RAGE: return "Rage";
    case CARD_LEVITY: return "Levity";
    case CARD_VENOM: return "Venom";
    case CARD_XOM: return "the card of Xom";
    case CARD_SLOW: return "Slowness";
    case CARD_DECAY: return "Decay";
    case CARD_HEALING: return "the Elixir of Health";
    case CARD_HEAL_WOUNDS: return "the Symbol of Immediate Regeneration";
    case CARD_TORMENT: return "the Symbol of Torment";
    case CARD_FOUNTAIN: return "the Fountain";
    case CARD_ALTAR: return "the Altar";
    case CARD_FAMINE: return "Famine";
    case CARD_FEAST: return "the Feast";
    case CARD_WILD_MAGIC: return "Wild Magic";
    case CARD_VIOLENCE: return "Violence";
    case CARD_PROTECTION: return "Protection";
    case CARD_KNOWLEDGE: return "Knowledge";
    case CARD_MAZE: return "the Maze";
    case CARD_PANDEMONIUM: return "Pandemonium";
    case CARD_IMPRISONMENT: return "the Prison";
    case CARD_RULES_FOR_BRIDGE: return "the rules for contract bridge";
    case NUM_CARDS: case CARD_RANDOM: return "a buggy card";
    }
    return "a very buggy card";
}

static card_type choose_one_card(deck_type which_deck, bool message)
{
    card_type *deck = deck_of_wonders;
    int max_card = 0;

    switch (which_deck)
    {
    case DECK_OF_WONDERS:
        deck = deck_of_wonders;
        max_card = DECK_WONDERS_SIZE;
        break;
    case DECK_OF_SUMMONING:
        deck = deck_of_summoning;
        max_card = DECK_SUMMONING_SIZE;
        break;
    case DECK_OF_TRICKS:
        deck = deck_of_tricks;
        max_card = DECK_TRICKS_SIZE;
        break;
    case DECK_OF_POWER:
        deck = deck_of_power;
        max_card = DECK_POWER_SIZE;
        break;
    case DECK_OF_PUNISHMENT:
        deck = deck_of_punishment;
        max_card = DECK_PUNISHMENT_SIZE;
        break;
    }

    card_type chosen = deck[random2(max_card)];

    if (one_chance_in(250))
    {
        if ( message )
            mpr("This card doesn't seem to belong here.");
        chosen = static_cast<card_type>(random2(NUM_CARDS));
    }

    // High Evocations gives you another shot (but not at being punished...)
    if (which_deck != DECK_OF_PUNISHMENT && chosen == CARD_BLANK &&
        you.skills[SK_EVOCATIONS] > random2(30))
        chosen = deck[random2(max_card)];
    return chosen;
}
static void cards(card_type which_card);

// returns the deck type, of DECK_OF_PUNISHMENT if none
deck_type subtype_to_decktype(int subtype)
{
    switch ( subtype )
    {
    case MISC_DECK_OF_WONDERS:
        return DECK_OF_WONDERS;
    case MISC_DECK_OF_POWER:
        return DECK_OF_POWER;
    case MISC_DECK_OF_SUMMONINGS:
        return DECK_OF_SUMMONING;
    case MISC_DECK_OF_TRICKS:
        return DECK_OF_TRICKS;
    default:                    // sentinel
        return DECK_OF_PUNISHMENT;
    }
}

bool deck_triple_draw()
{
    if (you.equip[EQ_WEAPON] == -1)
    {
        mpr("You aren't wielding a deck!");
        return false;
    }
    item_def& item(you.inv[you.equip[EQ_WEAPON]]);

    if ( item.base_type != OBJ_MISCELLANY ||
         subtype_to_decktype(item.sub_type) == DECK_OF_PUNISHMENT )
    {
        mpr("You aren't wielding a deck!");
        return false;
    }

    const deck_type dtype = subtype_to_decktype(item.sub_type);
    
    if (item.plus == 1)
    {
        // only one card to draw, so just draw it
        deck_of_cards(dtype);
        return true;
    }

    const int num_to_draw = (item.plus < 3 ? item.plus : 3);
    std::vector<card_type> draws;
    for ( int i = 0; i < num_to_draw; ++i )
        draws.push_back(choose_one_card(dtype, false));

    mpr("You draw... (choose one card)");
    for ( int i = 0; i < num_to_draw; ++i )
        msg::streams(MSGCH_PROMPT) << (static_cast<char>(i + 'a')) << " - "
                                   << card_name(draws[i]) << std::endl;
    int selected = -1;
    while ( 1 )
    {
        int keyin = get_ch();
        if ( isalpha(keyin) )
            keyin = tolower(keyin);
        if (keyin >= 'a' && keyin < 'a' + num_to_draw)
        {
            selected = keyin - 'a';
            break;
        }
        else
            canned_msg(MSG_HUH);
    }
    cards(draws[selected]);

    // remove the cards from the deck
    item.plus -= num_to_draw;
    if (item.plus <= 0)
    {
        mpr("The deck of cards disappears in a puff of smoke.");
        unwield_item(you.equip[EQ_WEAPON]);
        dec_inv_item_quantity( you.equip[EQ_WEAPON], 1 );
    }

    return true;
}

void deck_of_cards(deck_type which_deck)
{
    int brownie_points = 0;     // for passing to did_god_conduct() {dlb}

    mpr("You draw a card...");
    
    const card_type chosen = choose_one_card(which_deck, true);

    cards(chosen);

    // Decks of punishment aren't objects in the game,
    // its just Nemelex's form of punishment -- bwr
    if (which_deck != DECK_OF_PUNISHMENT)
    {
        you.inv[you.equip[EQ_WEAPON]].plus--;

        if (you.inv[you.equip[EQ_WEAPON]].plus == 0)
        {
            mpr("The deck of cards disappears in a puff of smoke.");

            unwield_item(you.equip[EQ_WEAPON]);

            dec_inv_item_quantity( you.equip[EQ_WEAPON], 1 );

            // these bonuses happen only when the deck expires {dlb}:
            brownie_points = (coinflip()? 2 : 1);

            if (which_deck == DECK_OF_WONDERS)
                brownie_points += 2;
            else if (which_deck == DECK_OF_POWER)
                brownie_points++;
        }

        // this bonus happens with every use {dlb}:
        if (which_deck == DECK_OF_WONDERS || one_chance_in(3))
            brownie_points++;

        did_god_conduct(DID_CARDS, brownie_points);
    }

    return;
}                               // end deck_of_cards()

static void cards(card_type which_card)
{
    FixedVector < int, 5 > dvar;
    FixedVector < int, 5 > mvar;
    int dvar1 = 0;
    int loopy = 0;              // general purpose loop variable {dlb}
    bool success = false;       // for summoning messages {dlb}
    bool failMsg = true;
    int summ_dur, summ_beh, summ_num; 

    if (which_card == CARD_BLANK && one_chance_in(10))
        which_card = CARD_RULES_FOR_BRIDGE;

    switch (which_card)
    {
    case CARD_BLANK:
        mpr("It is blank.");
        break;

    case CARD_BUTTERFLY:
        mpr("You have drawn the Butterfly.");

        summ_dur = 1 + random2(3) + you.skills[SK_EVOCATIONS] / 2;
        if (summ_dur > 6)
            summ_dur = 6;

        if (create_monster( MONS_BUTTERFLY, summ_dur, BEH_FRIENDLY, 
                            you.x_pos, you.y_pos, MHITYOU, 250 ) != -1)
        {
            mpr("A brightly coloured insect flies from the card!");
        }
        break;

    case CARD_WRAITH:
        mpr("You have drawn the Wraith.");

        lose_level();
        drain_exp();
        break;

    case CARD_EXPERIENCE:
        mpr( "You have drawn Experience." );
        potion_effect( POT_EXPERIENCE, 0 );
        break;

    case CARD_WEALTH:
        mpr("You have drawn Wealth.");

        you.gold += roll_dice( 2, 20 * you.skills[SK_EVOCATIONS] );
        you.redraw_gold = 1;
        break;

    case CARD_INTELLIGENCE:
        mpr("You have drawn the Brain!");

        you.intel += 1 + random2( you.skills[SK_EVOCATIONS] ) / 7;

        if (you.max_intel < you.intel)
            you.max_intel = you.intel;

        you.redraw_intelligence = 1;
        break;

    case CARD_STRENGTH:
        mpr("You have drawn Strength!");

        you.strength += 1 + random2( you.skills[SK_EVOCATIONS] ) / 7;

        if (you.max_strength < you.strength)
            you.max_strength = you.strength;

        you.redraw_strength = 1;
        break;

    case CARD_QUICKSILVER:
        mpr("You have drawn the Quicksilver card.");

        you.dex += 1 + random2( you.skills[SK_EVOCATIONS] ) / 7;

        if (you.max_dex < you.dex)
            you.max_dex = you.dex;

        you.redraw_dexterity = 1;
        break;

    case CARD_STUPIDITY:
        mpr("You have drawn Stupidity!");

        you.intel -= (2 + random2avg(3, 2));
        if (you.intel < 4)
            you.intel = 0;

        if (you.skills[SK_EVOCATIONS] < random2(30))
            you.max_intel--;

        you.redraw_intelligence = 1;
        break;

    case CARD_WEAKNESS:
        mpr("You have drawn Weakness.");

        you.strength -= (2 + random2avg(3, 2));
        if (you.strength < 4)
            you.strength = 0;

        if (you.skills[SK_EVOCATIONS] < random2(30))
            you.max_strength--;

        you.redraw_strength = 1;
        break;

    case CARD_SLOTH:
        mpr("You have drawn the Slug.");

        you.dex -= (2 + random2avg(3, 2));
        if (you.dex < 4)
            you.dex = 0;

        if (you.skills[SK_EVOCATIONS] < random2(30))
            you.max_dex--;

        you.redraw_dexterity = 1;
        break;

    case CARD_SHUFFLE:          // shuffle stats
        mpr("You have drawn the Shuffle card!");

        dvar[STAT_STRENGTH] = you.strength;
        dvar[STAT_DEXTERITY] = you.dex;
        dvar[STAT_INTELLIGENCE] = you.intel;

        mvar[STAT_STRENGTH] = you.max_strength;
        mvar[STAT_DEXTERITY] = you.max_dex;
        mvar[STAT_INTELLIGENCE] = you.max_intel;

        you.strength = 101;
        you.intel = 101;
        you.dex = 101;

        do
        {
            dvar1 = random2(NUM_STATS);

            if (dvar[dvar1] == 101)
                continue;

            if (you.strength == 101)
            {
                you.strength = dvar[dvar1];
                you.max_strength = mvar[dvar1];
            }
            else if (you.intel == 101)
            {
                you.intel = dvar[dvar1];
                you.max_intel = mvar[dvar1];
            }
            else if (you.dex == 101)
            {
                you.dex = dvar[dvar1];
                you.max_dex = mvar[dvar1];
            }

            dvar[dvar1] = 101;
        }
        while (dvar[STAT_STRENGTH] != 101 || dvar[STAT_DEXTERITY] != 101
                                           || dvar[STAT_INTELLIGENCE] != 101);

        you.redraw_strength = 1;
        you.redraw_intelligence = 1;
        you.redraw_dexterity = 1;
        burden_change();
        break;

    case CARD_FREAK:
        mpr("You have drawn the Freak!");
        for (loopy = 0; loopy < 6; loopy++)
        {
            if (!mutate(100, failMsg))
                failMsg = false;
        }
        break;

    case CARD_DEATH:
        mpr("Oh no! You have drawn the Death card!");

        if (you.duration[DUR_TELEPORT])
            you_teleport();

        for (loopy = 0; loopy < 5; loopy++)
        {
            create_monster( MONS_REAPER, 0, BEH_HOSTILE, you.x_pos, you.y_pos, 
                            MHITYOU, 250 );
        }
        break;

    case CARD_NORMALITY:
        mpr("You have drawn Normalisation.");
        for (loopy = 0; loopy < 6; loopy++)
        {
            delete_mutation(100);
        }
        break;

    case CARD_SHADOW:
        mpr("You have drawn the Shadow.");
        create_monster( MONS_SOUL_EATER, 0, BEH_HOSTILE, you.x_pos, you.y_pos, 
                        MHITYOU, 250 );
        break;

    case CARD_GATE:
        mpr("You have drawn the Gate!");

        if (you.level_type == LEVEL_ABYSS)
            banished(DNGN_EXIT_ABYSS, "drew the Gate");
        else if (you.level_type == LEVEL_LABYRINTH)
            canned_msg(MSG_NOTHING_HAPPENS);
        else
        {
            mpr("You are cast into the Abyss!");
            banished(DNGN_ENTER_ABYSS, "drew the Gate");
        }
        break;

    case CARD_STATUE:
        mpr("You have drawn the Crystal Statue.");
        create_monster( MONS_CRYSTAL_GOLEM, 0, BEH_FRIENDLY, 
                        you.x_pos, you.y_pos, you.pet_target, 250 );
        break;

    case CARD_ACQUISITION:
        mpr( "You have drawn Acquisition!" );
        mpr( "The card unfolds to form a scroll of paper." );
        acquirement( OBJ_RANDOM, AQ_CARD_ACQUISITION );
        break;

    case CARD_HASTEN:
        mpr("You have drawn Haste.");
        potion_effect( POT_SPEED, 5 * you.skills[SK_EVOCATIONS] );
        break;

    case CARD_DEMON_LESSER:
        mpr("On the card is a picture of a little demon.");

        summ_dur = cap_int(1 + random2(3) + you.skills[SK_EVOCATIONS] / 3, 6);

        if (create_monster( summon_any_demon( DEMON_LESSER ), summ_dur, 
                            BEH_FRIENDLY, you.x_pos, you.y_pos, you.pet_target,
                            250 ) != -1)
        {
            mpr("The picture comes to life!");
        }
        break;

    case CARD_DEMON_COMMON:
        mpr("On the card is a picture of a demon.");

        summ_dur = cap_int(1 + random2(3) + you.skills[SK_EVOCATIONS] / 4, 6);

        if (create_monster( summon_any_demon( DEMON_COMMON ), summ_dur, 
                            BEH_FRIENDLY, you.x_pos, you.y_pos, you.pet_target,
                            250 ) != -1)
        {
            mpr("The picture comes to life!");
        }
        break;

    case CARD_DEMON_GREATER:
        mpr("On the card is a picture of a huge demon.");

        summ_beh = (you.skills[SK_EVOCATIONS] > random2(30)) ? BEH_FRIENDLY 
                                                             : BEH_CHARMED;

        if (summ_beh == BEH_CHARMED)
            mpr( "You don't feel so good about this..." );

        if (create_monster( summon_any_demon( DEMON_GREATER ), 5,
                            summ_beh, you.x_pos, you.y_pos, 
                            you.pet_target, 250 ) != -1)
        {
            mpr("The picture comes to life!");
        }
        break;

    case CARD_DEMON_SWARM:
        mpr("On the card is a picture of a swarm of little demons.");

        success = false;

        summ_num = 7 + random2(6);

        for (loopy = 0; loopy < summ_num; loopy++)
        {
            if (create_monster( summon_any_demon( DEMON_LESSER ), 6, 
                                BEH_HOSTILE, you.x_pos, you.y_pos, 
                                MHITYOU, 250 ) != -1) 
            {
                 success = true;
            }
        }

        if (success)
            mpr("The picture comes to life!");
        break;

    case CARD_YAK:
        mpr("On the card is a picture of a huge shaggy yak.");

        summ_dur = cap_int(2 + you.skills[SK_EVOCATIONS] / 2, 6);
        if (create_monster( MONS_DEATH_YAK, summ_dur, BEH_FRIENDLY,
                            you.x_pos, you.y_pos, you.pet_target, 250 ) != -1)
        {
            mpr("The picture comes to life!");
        }
        break;

    case CARD_FIEND:
        mpr("On the card is a picture of a huge scaly devil.");

        summ_dur = cap_int(2 + you.skills[SK_EVOCATIONS] / 6, 6);
        if (create_monster( MONS_FIEND, summ_dur, BEH_FRIENDLY,
                            you.x_pos, you.y_pos, you.pet_target, 250 ) != -1)
        {
            mpr("The picture comes to life!");
        }
        break;

    case CARD_DRAGON:
        mpr("On the card is a picture of a huge scaly dragon.");

        summ_dur = cap_int(3 + you.skills[SK_EVOCATIONS] / 6, 6);
        if (create_monster( (coinflip() ? MONS_DRAGON : MONS_ICE_DRAGON),
                            summ_dur, BEH_FRIENDLY, you.x_pos, you.y_pos, 
                            you.pet_target, 250 ) != -1)
        {
            mpr("The picture comes to life!");
        }
        break;

    case CARD_GOLEM:
        mpr("On the card is a picture of a statue.");

        summ_dur = cap_int(2 + you.skills[SK_EVOCATIONS] / 4, 6);
        if (create_monster( MONS_CLAY_GOLEM + random2(6), summ_dur, 
                            BEH_FRIENDLY, you.x_pos, you.y_pos, 
                            you.pet_target, 250 ) != -1)
        {
            mpr("The picture comes to life!");
        }
        break;

    case CARD_THING_FUGLY:
        mpr("On the card is a picture of a very ugly thing.");

        summ_dur = cap_int(2 + you.skills[SK_EVOCATIONS] / 4, 6);
        if (create_monster( MONS_VERY_UGLY_THING, summ_dur, BEH_FRIENDLY,
                            you.x_pos, you.y_pos, you.pet_target, 250 ) != -1)
        {
            mpr("The picture comes to life!");
        }
        break;

    case CARD_LICH:
        mpr( "On the card is a picture of a very irritated-looking "
             "skeletal thing." );

        if (create_monster( MONS_LICH, 0, BEH_HOSTILE, you.x_pos, you.y_pos, 
                            MHITYOU, 250) != -1)
        {
            mpr("The picture comes to life!");
        }
        break;

    case CARD_HORROR_UNSEEN:
        if (!player_see_invis())
            mpr("It is blank!");
        else
            mpr("On the card is a picture of a hideous abomination.");

        summ_dur = cap_int(2 + you.skills[SK_EVOCATIONS] / 4, 6);
        if (create_monster( MONS_UNSEEN_HORROR, summ_dur, BEH_FRIENDLY,
                            you.x_pos, you.y_pos, you.pet_target, 250 ) != -1)
        {
            if (player_see_invis())
            {
                mpr("The picture comes to life!");
            }
        }
        break;

    case CARD_BLINK:
        mpr("You have drawn Blink.");
        blink();
        // random_blink(true);
        break;

    case CARD_TELEPORT:
        mpr("You have drawn the Portal of Delayed Transposition.");
        you_teleport();
        break;

    case CARD_TELEPORT_NOW:
        mpr( "You have drawn the Portal of Instantaneous Transposition." );
        you_teleport2( true, true );  // in abyss, always to new area
        break;

    case CARD_RAGE:
        mpr("You have drawn Rage.");

        if (!go_berserk(false))
            canned_msg(MSG_NOTHING_HAPPENS);
        else
            you.berserk_penalty = NO_BERSERK_PENALTY;
        break;

    case CARD_LEVITY:
        mpr("You have drawn Levity.");
        potion_effect( POT_LEVITATION, 5 * you.skills[SK_EVOCATIONS] );
        break;

    case CARD_VENOM:
        mpr("You have drawn Venom.");
        poison_player( 2 + random2( 7 - you.skills[SK_EVOCATIONS] / 5 ) );
        break;

    case CARD_XOM:
        mpr("You have drawn the card of Xom!");
        Xom_acts( true, 5 + random2( you.skills[SK_EVOCATIONS] ), true );
        break;

    case CARD_SLOW:
        mpr("You have drawn Slowness.");
        potion_effect( POT_SLOWING, 100 - 2 * you.skills[SK_EVOCATIONS] );
        break;

    case CARD_DECAY:
        mpr("You have drawn Decay.");

        if (you.is_undead)
            mpr("You feel terrible.");
        else
            rot_player( 2 + random2( 7 - you.skills[SK_EVOCATIONS] / 4 ) );
        break;

    case CARD_HEALING:
        mpr("You have drawn the Elixir of Health.");
        potion_effect( POT_HEALING, 5 * you.skills[SK_EVOCATIONS] );
        break;

    case CARD_HEAL_WOUNDS:
        mpr("You have drawn the Symbol of Immediate Regeneration.");
        potion_effect( POT_HEAL_WOUNDS, 5 * you.skills[SK_EVOCATIONS] );
        break;

    case CARD_TORMENT:
        mpr("You have drawn the Symbol of Torment.");
        torment( TORMENT_CARDS, you.x_pos, you.y_pos );
        break;
        
    case CARD_FOUNTAIN:
        // what about checking whether there are items there, too? {dlb}
        mpr("You have drawn the Fountain.");

        if (grd[you.x_pos][you.y_pos] == DNGN_FLOOR)
        {
            mprf("A beautiful fountain of clear blue water grows from the "
                 "floor %s!",
                 (you.species == SP_NAGA || you.species == SP_CENTAUR) ?
                 "before you!" : "at your feet!" );
            grd[you.x_pos][you.y_pos] = DNGN_BLUE_FOUNTAIN;
        }
        else
        {
            canned_msg(MSG_NOTHING_HAPPENS);
        }
        break;

    case CARD_ALTAR:
        mpr("You have drawn the Altar.");

        if (you.religion == GOD_NO_GOD ||
            grd[you.x_pos][you.y_pos] != DNGN_FLOOR)
        {
            canned_msg(MSG_NOTHING_HAPPENS);
        }
        else
        {
            dvar1 = 179 + you.religion;
            
            mprf("An altar grows from the floor %s!",
                 (you.species == SP_NAGA || you.species == SP_CENTAUR)
                 ? "before you" : "at your feet");
            grd[you.x_pos][you.y_pos] = dvar1;
        }
        break;

    case CARD_FAMINE:
        mpr("You have drawn Famine.");

        if (you.is_undead == US_UNDEAD)
            mpr("You feel rather smug.");
        else
            set_hunger(500, true);
        break;

    case CARD_FEAST:
        mpr("You have drawn the Feast.");

        if (you.is_undead == US_UNDEAD)
            mpr("You feel a horrible emptiness.");
        else
            set_hunger(12000, true);
        break;

    case CARD_WILD_MAGIC:
        mpr( "You have drawn Wild Magic." );
        miscast_effect( SPTYP_RANDOM, random2(15) + 5, random2(250), 0 );
        break;

    case CARD_VIOLENCE:
        mpr("You have drawn Violence.");
        acquirement( OBJ_WEAPONS, AQ_CARD_VIOLENCE );
        break;

    case CARD_PROTECTION:
        mpr("You have drawn Protection.");
        acquirement( OBJ_ARMOUR, AQ_CARD_PROTECTION );
        break;

    case CARD_KNOWLEDGE:
        mpr("You have drawn Knowledge.");
        acquirement( OBJ_BOOKS, AQ_CARD_KNOWLEDGE );
        break;

    case CARD_MAZE:
        mpr("You have drawn the Maze!");
        more();

        if (you.level_type == LEVEL_DUNGEON)
            banished( DNGN_ENTER_LABYRINTH );
        break;

    case CARD_PANDEMONIUM:
        mpr("You have drawn the Pandemonium card!");
        more();

        if (you.level_type == LEVEL_PANDEMONIUM)
            banished(DNGN_EXIT_PANDEMONIUM);
        else if (you.level_type == LEVEL_LABYRINTH)
            canned_msg(MSG_NOTHING_HAPPENS);
        else
            banished(DNGN_ENTER_PANDEMONIUM);
        break;

    case CARD_RULES_FOR_BRIDGE:
        mpr("You have drawn the rules for contract bridge.");
        mpr("How intriguing!");
        break;

    case CARD_IMPRISONMENT:
        mpr("You have drawn the Prison!");
        entomb();
        break;

    case NUM_CARDS:
    case CARD_RANDOM:
        mpr("You have drawn a buggy card!");
        break;
    }

    return;
}                               // end cards()
