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

#include <iostream>

#include "externs.h"

#include "beam.h"
#include "dungeon.h"
#include "effects.h"
#include "food.h"
#include "invent.h"
#include "it_use2.h"
#include "item_use.h"
#include "itemprop.h"
#include "items.h"
#include "maps.h"
#include "message.h"
#include "misc.h"
#include "monplace.h"
#include "monstuff.h"
#include "mutation.h"
#include "ouch.h"
#include "player.h"
#include "religion.h"
#include "spells1.h"
#include "spells2.h"
#include "spells3.h"
#include "spells4.h"
#include "spl-cast.h"
#include "spl-util.h"
#include "stuff.h"
#include "terrain.h"
#include "transfor.h"
#include "traps.h"
#include "view.h"

#define VECFROM(x) (x), (x) + ARRAYSIZE(x)
#define DEFVEC(Z) static std::vector<card_type> Z(VECFROM(a_##Z))

static card_type a_deck_of_transport[] = {
    CARD_PORTAL, CARD_WARP, CARD_SWAP, CARD_VELOCITY
};

DEFVEC(deck_of_transport);

static card_type a_deck_of_emergency[] = {
    CARD_TOMB, CARD_BANSHEE, CARD_DAMNATION, CARD_SOLITUDE, CARD_WARPWRIGHT
};

DEFVEC(deck_of_emergency);

static card_type a_deck_of_destruction[] = {
    CARD_VITRIOL, CARD_FLAME, CARD_FROST, CARD_VENOM, CARD_HAMMER,
    CARD_PAIN, CARD_TORMENT
};

DEFVEC(deck_of_destruction);

static card_type a_deck_of_battle[] = {
    CARD_ELIXIR, CARD_BATTLELUST, CARD_METAMORPHOSIS,
    CARD_HELM, CARD_BLADE, CARD_SHADOW
};

DEFVEC(deck_of_battle);

static card_type a_deck_of_enchantments[] = {
    CARD_ELIXIR
};

DEFVEC(deck_of_enchantments);

static card_type a_deck_of_summoning[] = {
    CARD_SUMMON_ANIMAL, CARD_SUMMON_DEMON, CARD_SUMMON_WEAPON
};

DEFVEC(deck_of_summoning);

static card_type a_deck_of_wonders[] = {
    CARD_POTION, CARD_FOCUS, CARD_SHUFFLE,
    CARD_EXPERIENCE, CARD_WILD_MAGIC, CARD_HELIX
};

DEFVEC(deck_of_wonders);

static card_type a_deck_of_dungeons[] = {
    CARD_MAP, CARD_DOWSING, CARD_SPADE, CARD_TROWEL, CARD_MINEFIELD
};

DEFVEC(deck_of_dungeons);

static card_type a_deck_of_oddities[] = {
    CARD_GENIE, CARD_BARGAIN, CARD_WRATH, CARD_XOM,
    CARD_FEAST, CARD_FAMINE, CARD_CURSE
};

DEFVEC(deck_of_oddities);

static card_type a_deck_of_punishment[] = {
    CARD_WRAITH, CARD_WILD_MAGIC, CARD_WRATH,
    CARD_XOM, CARD_FAMINE, CARD_CURSE, CARD_TOMB,
    CARD_DAMNATION, CARD_PORTAL, CARD_MINEFIELD
};

DEFVEC(deck_of_punishment);

#undef DEFVEC
#undef VECFROM

const char* card_name(card_type card)
{
    switch (card)
    {
    case CARD_BLANK: return "blank card";
    case CARD_PORTAL: return "the Portal";
    case CARD_WARP: return "the Warp";
    case CARD_SWAP: return "Swap";
    case CARD_VELOCITY: return "Velocity";
    case CARD_DAMNATION: return "Damnation";
    case CARD_SOLITUDE: return "Solitude";
    case CARD_ELIXIR: return "the Elixir";
    case CARD_BATTLELUST: return "Battlelust";
    case CARD_METAMORPHOSIS: return "Metamorphosis";
    case CARD_HELM: return "the Helm";
    case CARD_BLADE: return "the Blade";
    case CARD_SHADOW: return "the Shadow";
    case CARD_POTION: return "the Potion";
    case CARD_FOCUS: return "Focus";
    case CARD_SHUFFLE: return "Shuffle";
    case CARD_EXPERIENCE: return "Experience";
    case CARD_HELIX: return "the Helix";
    case CARD_DOWSING: return "Dowsing";
    case CARD_TROWEL: return "the Trowel";
    case CARD_MINEFIELD: return "the Minefield";
    case CARD_GENIE: return "the Genie";
    case CARD_TOMB: return "the Tomb";
    case CARD_MAP: return "the Map";
    case CARD_BANSHEE: return "the Banshee";
    case CARD_WILD_MAGIC: return "Wild Magic";
    case CARD_SUMMON_ANIMAL: return "the Herd";
    case CARD_SUMMON_DEMON: return "the Pentagram";
    case CARD_SUMMON_WEAPON: return "the Dance";
    case CARD_SUMMON_ANY: return "Summoning";
    case CARD_XOM: return "Xom";
    case CARD_FAMINE: return "Famine";
    case CARD_FEAST: return "the Feast";
    case CARD_WARPWRIGHT: return "Warpwright";
    case CARD_VITRIOL: return "Vitriol";
    case CARD_FLAME: return "Flame";
    case CARD_FROST: return "Frost";
    case CARD_VENOM: return "Venom";
    case CARD_HAMMER: return "the Hammer";
    case CARD_PAIN: return "Pain";
    case CARD_TORMENT: return "Torment";
    case CARD_SPADE: return "the Spade";
    case CARD_BARGAIN: return "the Bargain";
    case CARD_WRATH: return "Wrath";
    case CARD_WRAITH: return "the Wraith";
    case CARD_CURSE: return "the Curse";
    case NUM_CARDS: return "a buggy card";
    }
    return "a very buggy card";
}

static card_type choose_one_card(const item_def& item, bool message)
{
    std::vector<card_type> *pdeck = NULL;
    switch ( item.sub_type )
    {
    case MISC_DECK_OF_ESCAPE:
        pdeck = (coinflip() ? &deck_of_transport : &deck_of_emergency);
        break;
    case MISC_DECK_OF_DESTRUCTION: pdeck = &deck_of_destruction; break;
    case MISC_DECK_OF_DUNGEONS:    pdeck = &deck_of_dungeons;    break;
    case MISC_DECK_OF_SUMMONING:   pdeck = &deck_of_summoning;   break;
    case MISC_DECK_OF_WONDERS:     pdeck = &deck_of_wonders;     break;
    case MISC_DECK_OF_PUNISHMENT:  pdeck = &deck_of_punishment;  break;
    case MISC_DECK_OF_WAR:
        switch ( random2(6) )
        {
        case 0: pdeck = &deck_of_destruction;  break;
        case 1: pdeck = &deck_of_enchantments; break;
        case 2: pdeck = &deck_of_battle;       break;
        case 3: pdeck = &deck_of_summoning;    break;
        case 4: pdeck = &deck_of_transport;    break;
        case 5: pdeck = &deck_of_emergency;    break;
        }
        break;
    case MISC_DECK_OF_CHANGES:
        switch ( random2(3) )
        {
        case 0: pdeck = &deck_of_battle;       break;
        case 1: pdeck = &deck_of_dungeons;     break;
        case 2: pdeck = &deck_of_wonders;      break;
        }
        break;
    case MISC_DECK_OF_DEFENSE:
        pdeck = (coinflip() ? &deck_of_emergency : &deck_of_battle);
        break;
    }

    ASSERT( pdeck );
    
    if ( one_chance_in(100) )
    {
        if ( message )
            mpr("This card doesn't seem to belong here.");
        pdeck = &deck_of_oddities;
    }

    card_type chosen = (*pdeck)[random2(pdeck->size())];

    // High Evocations gives you another shot (but not at being punished...)
    if (pdeck != &deck_of_punishment && chosen == CARD_BLANK &&
        you.skills[SK_EVOCATIONS] > random2(30))
        chosen = (*pdeck)[random2(pdeck->size())];

    return chosen;
}

static bool wielding_deck()
{
    if ( you.equip[EQ_WEAPON] == -1 )
        return false;
    return is_deck(you.inv[you.equip[EQ_WEAPON]]);
}

// Select a deck from inventory and draw a card from it.
bool choose_deck_and_draw()
{
    int slot = prompt_invent_item( "Draw from which deck?",
                                   MT_INVLIST, OBJ_MISCELLANY,
                                   true, true, true, 0, NULL,
                                   OPER_EVOKE );
    if ( slot == PROMPT_ABORT )
    {
        canned_msg(MSG_OK);
        return false;
    }

    item_def& deck(you.inv[slot]);
    if ( !is_deck(deck) )
    {
        mpr("That isn't a deck!");
        return false;
    }
    evoke_deck(deck);
    return true;
}

// Peek at a deck (show what the next card will be.)
// Return false if the operation was failed/aborted along the way.
bool deck_peek()
{
    if ( !wielding_deck() )
    {
        mpr("You aren't wielding a deck!");
        return false;
    }
    item_def& item(you.inv[you.equip[EQ_WEAPON]]);
    if ( item.plus2 != 0 )
    {
        mpr("You already know what the next card will be.");
        return false;
    }

    const card_type chosen = choose_one_card(item, false);

    msg::stream << "You see " << card_name(chosen) << '.' << std::endl;
    item.plus2 = chosen + 1;
    you.wield_change = true;

    // You lose 1d2 cards when peeking.
    if ( item.plus > 1 )
    {
        mpr("Some cards drop out of the deck.");
        if ( item.plus > 2 && coinflip() )
            item.plus -= 2;
        else
            item.plus -= 1;
    }

    return true;
}

// Stack a deck: look at the next five cards, put them back in any
// order, discard the rest of the deck.
// Return false if the operation was failed/aborted along the way.
bool deck_stack()
{
    if ( !wielding_deck() )
    {
        mpr("You aren't wielding a deck!");
        return false;
    }
    item_def& item(you.inv[you.equip[EQ_WEAPON]]);
    if ( item.plus2 != 0 )
    {
        mpr("You can't stack a marked deck.");
        return false;
    }
    const int num_to_stack = (item.plus < 5 ? item.plus : 5);

    std::vector<card_type> draws;
    for ( int i = 0; i < num_to_stack; ++i )
        draws.push_back(choose_one_card(item, false));

    if ( draws.size() == 1 )
        mpr("There's only one card left!");
    else
        mpr("Order the cards (bottom to top)...", MSGCH_PROMPT);

    item.special = 0;

    while ( draws.size() > 1 )
    {
        for ( unsigned int i = 0; i < draws.size(); ++i )
        {
            msg::stream << (static_cast<char>(i + 'a')) << " - "
                        << card_name(draws[i]) << std::endl;
        }

        int selected = -1;
        while ( true )
        {
            const int keyin = tolower(get_ch());
            if (keyin >= 'a' && keyin < 'a' + static_cast<int>(draws.size()))
            {
                selected = keyin - 'a';
                break;
            }
            else
            {
                canned_msg(MSG_HUH);
            }
        }
        item.special <<= 8;
        item.special += draws[selected] + 1;
        draws.erase(draws.begin() + selected);
        mpr("Next card?", MSGCH_PROMPT);
    }
    item.plus2 = draws[0] + 1;
    item.plus = num_to_stack;   // no more deck after the stack
    you.wield_change = true;
    return true;
}

// Draw the next three cards, discard two and pick one.
bool deck_triple_draw()
{
    if ( !wielding_deck() )
    {
        mpr("You aren't wielding a deck!");
        return false;
    }

    const int slot = you.equip[EQ_WEAPON];
    item_def& item(you.inv[slot]);

    if ( item.plus2 != 0 )
    {
        mpr("You can't triple draw from a marked deck.");
        return false;
    }

    if (item.plus == 1)
    {
        // only one card to draw, so just draw it
        evoke_deck(item);
        return true;
    }

    const int num_to_draw = (item.plus < 3 ? item.plus : 3);
    std::vector<card_type> draws;
    for ( int i = 0; i < num_to_draw; ++i )
        draws.push_back(choose_one_card(item, false));

    mpr("You draw... (choose one card)");
    for ( int i = 0; i < num_to_draw; ++i )
        msg::streams(MSGCH_PROMPT) << (static_cast<char>(i + 'a')) << " - "
                                   << card_name(draws[i]) << std::endl;
    int selected = -1;
    while ( 1 )
    {
        const int keyin = tolower(get_ch());
        if (keyin >= 'a' && keyin < 'a' + num_to_draw)
        {
            selected = keyin - 'a';
            break;
        }
        else
            canned_msg(MSG_HUH);
    }

    // Note that card_effect() might cause you to unwield the deck.
    card_effect(draws[selected], deck_rarity(item));

    // remove the cards from the deck
    item.plus -= num_to_draw;
    if (item.plus <= 0)
    {
        mpr("The deck of cards disappears in a puff of smoke.");
        if ( slot == you.equip[EQ_WEAPON] )
            unwield_item();

        dec_inv_item_quantity( slot, 1 );
    }
    you.wield_change = true;
    return true;
}

// This is Nemelex retribution.
void draw_from_deck_of_punishment()
{
    item_def deck;
    deck.plus = 10;             // don't let it puff away
    deck.plus2 = 0;
    deck.colour = BLACK;        // for rarity
    deck.base_type = OBJ_MISCELLANY;
    deck.sub_type = MISC_DECK_OF_PUNISHMENT;
    evoke_deck(deck);
}

// In general, if the next cards in a deck are known, they will
// be stored in plus2 (the next card) and special (up to 4 cards
// after that, bitpacked.)
// Hidden assumption: no more than 126 cards, otherwise the 5th
// card could clobber the sign bit in special.
void evoke_deck( item_def& deck )
{
    int brownie_points = 0;
    mpr("You draw a card...");
    bool allow_id = in_inventory(deck) && !item_ident(deck, ISFLAG_KNOW_TYPE);
    bool fake_draw = false;
    
    // If the deck wasn't marked, draw a fair card.
    if ( deck.plus2 == 0 )
    {
        fake_draw =
            !card_effect( choose_one_card(deck, true), deck_rarity(deck) );

        if ( deck.sub_type != MISC_DECK_OF_PUNISHMENT )
        {
            // Nemelex likes gamblers.
            brownie_points = 1;
            if (one_chance_in(3))
                brownie_points++;
        }
    }
    else
    {
        // You can't ID off a marked card
        allow_id = false;

        // draw the marked card
        fake_draw = !card_effect(static_cast<card_type>(deck.plus2 - 1),
                                 deck_rarity(deck));

        // If there are more marked cards, shift them up
        if ( deck.special )
        {
            const short next_card = (deck.special & 0xFF);
            deck.special >>= 8;
            deck.plus2 = next_card;
        }
        else
        {
            deck.plus2 = 0;
        }
        you.wield_change = true;
    }
    deck.plus--;
    
    if ( deck.plus == 0 )
    {
        mpr("The deck of cards disappears in a puff of smoke.");
        dec_inv_item_quantity( deck.link, 1 );
        // Finishing the deck will earn a point, even if it
        // was marked or stacked.
        brownie_points++;
    }
    else if (allow_id && (you.skills[SK_EVOCATIONS] > 5 + random2(35)))
    {
        mpr("Your skill with magical items lets you identify the deck.");
        set_ident_flags( deck, ISFLAG_KNOW_TYPE );
        msg::streams(MSGCH_EQUIPMENT) << deck.name(DESC_INVENTORY)
                                      << std::endl;
        you.wield_change = true;
    }

    if (!fake_draw)
        did_god_conduct(DID_CARDS, brownie_points);
}

int get_power_level(int power, deck_rarity_type rarity)
{
    int power_level = 0;
    switch ( rarity )
    {
    case DECK_RARITY_COMMON:
        break;
    case DECK_RARITY_LEGENDARY:
        if ( random2(500) < power )
            ++power_level;
        // deliberate fall-through
    case DECK_RARITY_RARE:
        if ( random2(700) < power )
            ++power_level;
        break;
    }
    return power_level;
}

/* Actual card implementations follow. */
static void portal_card(int power, deck_rarity_type rarity)
{
    const int control_level = get_power_level(power, rarity);
    bool instant = false;
    bool controlled = false;
    if ( control_level >= 2 )
    {
        instant = true;
        controlled = true;
    }
    else if ( control_level == 1 )
    {
        if ( coinflip() )
            instant = true;
        else
            controlled = true;
    }

    const bool was_controlled = player_control_teleport();
    if ( controlled && !was_controlled )
        you.duration[DUR_CONTROL_TELEPORT] = 6; // long enough to kick in

    if ( instant )
        you_teleport_now( true );
    else
        you_teleport();
}

static void warp_card(int power, deck_rarity_type rarity)
{
    const int control_level = get_power_level(power, rarity);
    if ( control_level >= 2 )
        blink(1000, false);
    else if ( control_level == 1 )
        cast_semi_controlled_blink(power / 4);
    else
        random_blink(false);
}

// Find a nearby monster to banish and return its index,
// including you as a possibility with probability weight.
static int choose_random_nearby_monster(int weight)
{
    int mons_count = weight;
    int result = NON_MONSTER;

    int ystart = you.y_pos - 9, xstart = you.x_pos - 9;
    int yend = you.y_pos + 9, xend = you.x_pos + 9;
    if ( xstart < 0 ) xstart = 0;
    if ( ystart < 0 ) ystart = 0;
    if ( xend >= GXM ) xend = GXM;
    if ( ystart >= GYM ) yend = GYM;

    /* monster check */
    for ( int y = ystart; y < yend; ++y )
        for ( int x = xstart; x < xend; ++x )
            if ( see_grid(x,y) && mgrd[x][y] != NON_MONSTER )
                if ( one_chance_in(++mons_count) )
                    result = mgrd[x][y];

    return result;
}

static void swap_monster_card(int power, deck_rarity_type rarity)
{
    // Swap between you and another monster.
    // Don't choose yourself unless there are no other monsters
    // nearby.
    const int mon_to_swap = choose_random_nearby_monster(0);
    if ( mon_to_swap == NON_MONSTER )
    {
        mpr("You spin around.");
    }
    else
    {
        monsters& mon = menv[mon_to_swap];
        const coord_def newpos = menv[mon_to_swap].pos();

        // pick the monster up
        mgrd[mon.x][mon.y] = NON_MONSTER;

        mon.x = you.x_pos;
        mon.y = you.y_pos;

        // plunk it down
        mgrd[mon.x][mon.y] = mon_to_swap;

        // move you to its previous location
        you.moveto(newpos);
    }
}

static void velocity_card(int power, deck_rarity_type rarity)
{
    const int power_level = get_power_level(power, rarity);
    if ( power_level >= 2 )
    {
        potion_effect( POT_SPEED, random2(power / 4) );
    }
    else if ( power_level == 1 )
    {
        cast_fly( random2(power/4) );
        cast_swiftness( random2(power/4) );
    }
    else
    {
        cast_swiftness( random2(power/4) );
    }
}

static void damnation_card(int power, deck_rarity_type rarity)
{
    if ( you.level_type != LEVEL_DUNGEON )
    {
        canned_msg(MSG_NOTHING_HAPPENS);
        return;
    }

    // Calculate how many extra banishments you get.
    const int power_level = get_power_level(power, rarity);
    int nemelex_bonus = 0;
    if ( you.religion == GOD_NEMELEX_XOBEH && !player_under_penance() )
        nemelex_bonus = you.piety / 20;    
    int extra_targets = power_level + random2(you.skills[SK_EVOCATIONS] +
                                              nemelex_bonus) / 12;
    
    for ( int i = 0; i < 1 + extra_targets; ++i )
    {
        // pick a random monster nearby to banish (or yourself)
        const int mon_to_banish = choose_random_nearby_monster(1);

        // bonus banishments only banish monsters
        if ( i != 0 && mon_to_banish == NON_MONSTER )
            continue;

        if ( mon_to_banish == NON_MONSTER ) // banish yourself!
        {
            banished(DNGN_ENTER_ABYSS, "drawing a card");
            break;              // don't banish anything else
        }
        else
            menv[mon_to_banish].banish();
    }

}

static void warpwright_card(int power, deck_rarity_type rarity)
{
    if ( you.level_type == LEVEL_ABYSS )
    {
        mpr("The power of the Abyss blocks your magic.");
        return;
    }

    int count = 0;
    int fx = -1, fy = -1;
    for ( int dx = -1; dx <= 1; ++dx )
    {
        for ( int dy = -1; dy <= 1; ++dy )
        {
            if ( dx == 0 && dy == 0 )
                continue;
            const int rx = you.x_pos + dx;
            const int ry = you.y_pos + dy;
            if ( grd[rx][ry] == DNGN_FLOOR && trap_at_xy(rx,ry) == -1 )
            {
                if ( one_chance_in(++count) )
                {
                    fx = rx;
                    fy = ry;
                }
            }
        }
    }

    if ( fx >= 0 )              // found a spot
    {
        if ( place_specific_trap(fx, fy, TRAP_TELEPORT) )
        {
            // mark it discovered if enough power
            if ( get_power_level(power, rarity) >= 1 )
            {
                const int i = trap_at_xy(fx, fy);
                if (i != -1)    // should always happen
                    grd[fx][fy] = trap_category(env.trap[i].type);
            }
        }
    }
}

static void minefield_card(int power, deck_rarity_type rarity)
{
    const int power_level = get_power_level(power, rarity);
    const int radius = power_level * 2 + 2;
    for ( int dx = -radius; dx <= radius; ++dx )
    {
        for ( int dy = -radius; dy <= radius; ++dy )
        {
            if ( dx*dx + dy*dy > radius*radius + 1 )
                continue;
            if ( dx == 0 && dy == 0 )
                continue;

            const int rx = you.x_pos + dx;
            const int ry = you.y_pos + dy;
            if ( !in_bounds(rx, ry) )
                continue;
            if ( grd[rx][ry] == DNGN_FLOOR && trap_at_xy(rx,ry) == -1 &&
                 one_chance_in(4 - power_level) )
            {
                if ( you.level_type == LEVEL_ABYSS )
                    grd[rx][ry] = coinflip() ? DNGN_DEEP_WATER : DNGN_LAVA;
                else
                    place_specific_trap(rx, ry, TRAP_RANDOM);
            }
        }
    }
}

// Return true if it was a "genuine" draw, i.e., there was a monster
// to target. This is still exploitable by finding popcorn monsters.
static bool damaging_card(card_type card, int power, deck_rarity_type rarity)
{
    extern bool there_are_monsters_nearby();
    bool rc = there_are_monsters_nearby();
    
    const int power_level = get_power_level(power, rarity);

    dist target;
    bolt beam;
    zap_type ztype = ZAP_DEBUGGING_RAY;
    const zap_type firezaps[3] = { ZAP_FLAME, ZAP_STICKY_FLAME, ZAP_FIRE };
    const zap_type frostzaps[3] = { ZAP_FROST, ZAP_ICE_BOLT, ZAP_COLD };
    const zap_type hammerzaps[3] = { ZAP_STRIKING, ZAP_STONE_ARROW,
                                     ZAP_CRYSTAL_SPEAR };
    const zap_type venomzaps[3] = { ZAP_STING, ZAP_VENOM_BOLT,
                                    ZAP_POISON_ARROW };

    switch ( card )
    {
    case CARD_VITRIOL:
        ztype = (one_chance_in(3) ? ZAP_DEGENERATION : ZAP_BREATHE_ACID);
        break;

    case CARD_FLAME:
        ztype = (coinflip() ? ZAP_FIREBALL : firezaps[power_level]);
        break;

    case CARD_FROST:
        ztype = frostzaps[power_level];
        break;

    case CARD_HAMMER:
        ztype = hammerzaps[power_level];
        break;

    case CARD_VENOM:
        ztype = venomzaps[power_level];
        break;

    case CARD_PAIN:
        ztype = ZAP_AGONY;
        break;

    default:
        break;
    }

    if ( spell_direction( target, beam ) )
        zapping(ztype, random2(power/4), beam);
    else
        rc = false;

    return rc;
}

static void elixir_card(int power, deck_rarity_type rarity)
{
    int power_level = get_power_level(power, rarity);

    if ( power_level == 1 && you.hp * 2 > you.hp_max )
        power_level = 0;

    if ( power_level == 0 )
    {
        if ( coinflip() )
            potion_effect( POT_HEAL_WOUNDS, 40 ); // power doesn't matter
        else
            cast_regen( random2(power / 4) );
    }
    else if ( power_level == 1 )
    {
        you.hp = you.hp_max;
        you.magic_points = 0;
    }
    else if ( power_level >= 2 )
    {
        you.hp = you.hp_max;
        you.magic_points = you.max_magic_points;
    }
    you.redraw_hit_points = 1;
    you.redraw_magic_points = 1;
}

static void battle_lust_card(int power, deck_rarity_type rarity)
{
    const int power_level = get_power_level(power, rarity);
    if ( power_level >= 2 )
    {
        you.duration[DUR_SLAYING] = random2(power/6) + 1;
        mpr("You feel deadly.");
    }
    else if ( power_level == 1 )
    {
        you.duration[DUR_BUILDING_RAGE] = 1;
        mpr("You feel your rage building.");
    }
    else if ( power_level == 0 )
        potion_effect(POT_MIGHT, random2(power/4));
}

static void metamorphosis_card(int power, deck_rarity_type rarity)
{
    const int power_level = get_power_level(power, rarity);
    transformation_type trans;
    if ( power_level >= 2 )
        trans = (coinflip() ? TRAN_DRAGON : TRAN_LICH);
    else if ( power_level == 1 )
    {
        trans = (one_chance_in(3) ? TRAN_STATUE :
                 (coinflip() ? TRAN_ICE_BEAST : TRAN_BLADE_HANDS));
    }
    else
        trans = (coinflip() ? TRAN_SPIDER : TRAN_BAT);
    transform(random2(power/4), trans);
}

static void helm_card(int power, deck_rarity_type rarity)
{
    const int power_level = get_power_level(power, rarity);
    bool do_forescry = false;
    bool do_stoneskin = false;
    bool do_shield = false;
    int num_resists = 0;
    if ( power_level >= 2 )
    {
        if ( coinflip() ) do_forescry = true;
        if ( coinflip() ) do_stoneskin = true;
        if ( coinflip() ) do_shield = true;
        num_resists = random2(4);
    }
    if ( power_level >= 1 )
    {
        if ( coinflip() ) do_forescry = true;
        if ( coinflip() ) do_stoneskin = true;
        if ( coinflip() ) do_shield = true;
    }
    if ( power_level >= 0 )
    {
        if ( coinflip() )
            do_forescry = true;
        else
            do_stoneskin = true;
    }

    if ( do_forescry )
        cast_forescry( random2(power/4) );
    if ( do_stoneskin )
        cast_stoneskin( random2(power/4) );
    if ( num_resists )
    {
        const duration_type possible_resists[4] = {
            DUR_RESIST_POISON, DUR_INSULATION,
            DUR_RESIST_FIRE, DUR_RESIST_COLD
        };
        const char* resist_names[4] = {
            "poison", "electricity", "fire", "cold"
        };
        for ( int i = 0; i < 4 && num_resists; ++i )
        {
            // if there are n left, of which we need to choose
            // k, we have chance k/n of selecting the next item.
            if ( random2(4-i) < num_resists )
            {
                // Add a temporary resist
                you.duration[possible_resists[i]] += random2(power/7) + 1;
                msg::stream << "You feel resistant to " << resist_names[i]
                            << '.' << std::endl;
                --num_resists;
            }
        }
    }

    if ( do_shield )
    {
        if ( you.duration[DUR_MAGIC_SHIELD] == 0 )
            mpr("A magical shield forms in front of you.");
        you.duration[DUR_MAGIC_SHIELD] += random2(power/6) + 1;
    }
}

static void blade_card(int power, deck_rarity_type rarity)
{
    // Pause before jumping to the list.
    if (Options.auto_list)
        more();

    wield_weapon(false);

    const int power_level = get_power_level(power, rarity);
    if ( power_level >= 2 )
    {
        dancing_weapon( random2(power/4), false );
    }
    else if ( power_level == 1 )
    {
        cast_sure_blade( random2(power/4) );
    }
    else
    {
        brand_weapon(SPWPN_VORPAL, random2(power/4)); // maybe other brands?
    }
}

static void shadow_card(int power, deck_rarity_type rarity)
{
    const int power_level = get_power_level(power, rarity);

    if ( power_level >= 1 )
    {
        mpr( you.duration[DUR_STEALTH] ? "You feel more catlike." :
             "You feel stealthy.");
        you.duration[DUR_STEALTH] += random2(power/4) + 1;
    }

    potion_effect(POT_INVISIBILITY, random2(power/4));
}

static void potion_card(int power, deck_rarity_type rarity)
{
    const int power_level = get_power_level(power, rarity);
    potion_type pot_effects[] = {
        POT_HEAL_WOUNDS, POT_HEAL_WOUNDS, POT_HEAL_WOUNDS,
        POT_HEALING, POT_HEALING, POT_HEALING,
        POT_RESTORE_ABILITIES, POT_RESTORE_ABILITIES,
        POT_POISON, POT_CONFUSION, POT_DEGENERATION
    };

    potion_type pot = RANDOM_ELEMENT(pot_effects);

    if ( power_level >= 1 && coinflip() )
        pot = (coinflip() ? POT_CURE_MUTATION : POT_MUTATION);

    if ( power_level >= 2 && one_chance_in(5) )
        pot = POT_MAGIC;

    potion_effect(pot, random2(power/4));
}

static void focus_card(int power, deck_rarity_type rarity)
{
    char* max_statp[] = { &you.max_strength, &you.max_intel, &you.max_dex };
    char* base_statp[] = { &you.strength, &you.intel, &you.dex };
    int best_stat = 0;
    int worst_stat = 0;

    for ( int i = 1; i < 3; ++i )
    {
        const int best_diff = *max_statp[i] - *max_statp[best_stat];
        if ( best_diff > 0 || (best_diff == 0 && coinflip()) )
            best_stat = i;
        const int worst_diff = *max_statp[i] - *max_statp[worst_stat];
        if ( worst_diff < 0 || (worst_diff == 0 && coinflip()) )
            worst_stat = i;
    }

    while ( best_stat == worst_stat )
    {
        best_stat = random2(3);
        worst_stat = random2(3);
    }

    (*max_statp[best_stat])++;
    (*max_statp[worst_stat])--;
    (*base_statp[best_stat])++;
    (*base_statp[worst_stat])--;
    you.redraw_strength = true;
    you.redraw_intelligence = true;
    you.redraw_dexterity = true;
}

static void shuffle_card(int power, deck_rarity_type rarity)
{
    stat_type stats[3] = {STAT_STRENGTH, STAT_DEXTERITY, STAT_INTELLIGENCE};
    int old_base[3] = { you.strength, you.dex, you.intel};
    int old_max[3]  = {you.max_strength, you.max_dex, you.max_intel};
    int modifiers[3];

    int perm[3] = { 0, 1, 2 };

    for ( int i = 0; i < 3; ++i )
        modifiers[i] = stat_modifier(stats[i]);

    std::random_shuffle( perm, perm + 3 );
    
    int new_base[3];
    int new_max[3];
    for ( int i = 0; i < 3; ++i )
    {
        new_base[perm[i]] = old_base[i] - modifiers[i] + modifiers[perm[i]];
        new_max[perm[i]] = old_max[i] - modifiers[i] + modifiers[perm[i]];
    }
    
    // Sometimes you just long for Python.
    you.strength = new_base[0];
    you.dex = new_base[1];
    you.intel = new_base[2];

    you.max_strength = new_max[0];
    you.max_dex = new_max[1];
    you.max_intel = new_max[2];

    you.redraw_strength = true;
    you.redraw_intelligence = true;
    you.redraw_dexterity = true;
    you.redraw_evasion = true;
}

static void helix_card(int power, deck_rarity_type rarity)
{
    mutation_type bad_mutations[] = {
        MUT_FAST_METABOLISM, MUT_WEAK, MUT_DOPEY, MUT_CLUMSY,
        MUT_TELEPORT, MUT_DEFORMED, MUT_LOST, MUT_DETERIORATION,
        MUT_BLURRY_VISION, MUT_FRAIL
    };

    mutation_type which_mut = NUM_MUTATIONS;
    int numfound = 0;
    for ( unsigned int i = 0; i < ARRAYSIZE(bad_mutations); ++i )
    {
        if (you.mutation[bad_mutations[i]] > you.demon_pow[bad_mutations[i]])
            if ( one_chance_in(++numfound) )
                which_mut = bad_mutations[i];
    }
    if ( numfound )
        delete_mutation(which_mut);
}

static void dowsing_card(int power, deck_rarity_type rarity)
{
    const int power_level = get_power_level(power, rarity);
    bool things_to_do[3] = { false, false, false };
    things_to_do[random2(3)] = true;

    if ( power_level == 1 )
        things_to_do[random2(3)] = true;

    if ( power_level >= 2 )
        for ( int i = 0; i < 3; ++i )
            things_to_do[i] = true;

    if ( things_to_do[0] )
        cast_detect_secret_doors( random2(power/4) );
    if ( things_to_do[1] )
        detect_traps( random2(power/4) );
    if ( things_to_do[2] )
        detect_creatures( random2(power/4) );
}

static void trowel_card(int power, deck_rarity_type rarity)
{
    const int power_level = get_power_level(power, rarity);
    bool done_stuff = false;
    if ( power_level >= 2 )
    {
        // generate a portal to something
        int mapidx = -1;
        if ( coinflip() )
        {
            // generate a bazaar portal
            mapidx = find_map_by_name("bzr_entry_dummy");
        }
        else
        {
            mapidx = find_map_by_name("lab_entry_generic");
        }

        if ( mapidx == -1 )
        {
            mpr("A buggy portal flickers into view, then vanishes.");
        }
        else
        {
            {
                no_messages n;
                dgn_place_map(mapidx, false, true, true, you.pos());
            }
            mpr("A mystic portal forms.");
        }
        done_stuff = true;
    }
    else if ( power_level == 1 )
    {
        if (coinflip())
        {
            // Create a random bad statue
            // This could be really bad, because it's placed adjacent
            // to you...
            const monster_type statues[] = {
                MONS_ORANGE_STATUE, MONS_SILVER_STATUE, MONS_ICE_STATUE
            };
            const monster_type mons = RANDOM_ELEMENT(statues);
            if ( create_monster(mons, 0, BEH_HOSTILE, you.x_pos, you.y_pos,
                                MHITYOU, 250) != -1 )
            {
                mpr("A menacing statue appears!");
                done_stuff = true;
            }
        }
        else
        {
            coord_def pos = pick_adjacent_free_square(you.x_pos, you.y_pos);
            if ( pos.x >= 0 && pos.y >= 0 )
            {
                const dungeon_feature_type statfeat[] = {
                    DNGN_GRANITE_STATUE, DNGN_ORCISH_IDOL
                };
                // We leave the items on the square
                grd[pos.x][pos.y] = RANDOM_ELEMENT(statfeat);
                mpr("A statue takes form beside you.");
                done_stuff = true;
            }
        }
    }
    else
    {
        // generate an altar
        if ( grd[you.x_pos][you.y_pos] == DNGN_FLOOR )
        {
            // might get GOD_NO_GOD and no altar
            grd[you.x_pos][you.y_pos] =
                altar_for_god(static_cast<god_type>(random2(NUM_GODS)));

            if ( grd[you.x_pos][you.y_pos] != DNGN_FLOOR )
            {
                done_stuff = true;
                mpr("An altar grows from the floor before you!");
            }
        }
    }

    if ( !done_stuff )
        mpr("Nothing appears to happen.");

    return;
}

static void genie_card(int power, deck_rarity_type rarity)
{
    if ( coinflip() )
        acquirement( OBJ_RANDOM, AQ_CARD_GENIE );
    else
        potion_effect( coinflip() ? POT_DEGENERATION : POT_DECAY, 40 );
}

static void godly_wrath()
{
    divine_retribution(static_cast<god_type>(random2(NUM_GODS-1) + 1));
}

static void curse_card(int power, deck_rarity_type rarity)
{
    const int power_level = get_power_level(power, rarity);

    if ( power_level >= 2 )
    {
        // curse (almost) everything + decay
        while ( curse_an_item(true) && !one_chance_in(1000) )
            ;
    }
    else if ( power_level == 1 )
    {
        do                      // curse an average of four items
        {
            curse_an_item(false);
        } while ( !one_chance_in(4) );
    }
    else
    {
        curse_an_item(false);   // curse 1.5 items
        if ( coinflip() )
            curse_an_item(false);
    }
}

static void summon_demon_card(int power, deck_rarity_type rarity)
{
    const int power_level = get_power_level(power, rarity);
    demon_class_type dct;
    if ( power_level >= 2 )
        dct = DEMON_GREATER;
    else if ( power_level == 1 )
        dct = DEMON_COMMON;
    else
        dct = DEMON_LESSER;
    create_monster( summon_any_demon(dct), std::min(power/50,6),
                    BEH_FRIENDLY, you.x_pos, you.y_pos, MHITYOU, 250 );
}

static void summon_any_monster(int power, deck_rarity_type rarity)
{
    const int power_level = get_power_level(power, rarity);
    monster_type mon_chosen = NUM_MONSTERS;
    int chosen_x = 0, chosen_y = 0;
    int num_tries;

    if ( power_level == 0 )
        num_tries = 1;
    else if ( power_level == 1 )
        num_tries = 4;
    else
        num_tries = 18;

    for ( int i = 0; i < num_tries; ++i ) {
        int dx, dy;
        do {
            dx = random2(3) - 1;
            dy = random2(3) - 1;
        } while ( dx == 0 && dy == 0 );
        monster_type cur_try;
        do {
            cur_try = random_monster_at_grid(you.x_pos + dx, you.y_pos + dy);
        } while ( mons_is_unique(cur_try) );

        if ( mon_chosen == NUM_MONSTERS ||
             mons_power(mon_chosen) < mons_power(cur_try) )
        {
            mon_chosen = cur_try;
            chosen_x = you.x_pos;
            chosen_y = you.y_pos;
        }
    }

    if ( mon_chosen == NUM_MONSTERS ) // should never happen
        return;
    
    if ( power_level == 0 && one_chance_in(4) )
        create_monster( mon_chosen, 3, BEH_HOSTILE,
                        chosen_x, chosen_y, MHITYOU, 250 );
    else
        create_monster( mon_chosen, 3, BEH_FRIENDLY,
                        chosen_x, chosen_y, you.pet_target, 250 );
}

static void summon_dancing_weapon(int power, deck_rarity_type rarity)
{
    const int power_level = get_power_level(power, rarity);
    const bool friendly = (power_level > 0 || !one_chance_in(4));
    const int mon = create_monster( MONS_DANCING_WEAPON, power_level + 3,
                                    friendly ? BEH_FRIENDLY : BEH_HOSTILE,
                                    you.x_pos, you.y_pos,
                                    friendly ? you.pet_target : MHITYOU,
                                    250 );
    
    // Given the abundance of Nemelex decks, not setting hard reset
    // leaves a trail of weapons behind, most of which just get
    // offered to Nemelex again, adding an unnecessary source of
    // piety.
    if (mon != -1)
        menv[mon].flags |= MF_HARD_RESET;
}

static int card_power(deck_rarity_type rarity)
{
    int result = 0;

    if ( you.penance[GOD_NEMELEX_XOBEH] )
    {
        result -= you.penance[GOD_NEMELEX_XOBEH];
    }
    else if ( you.religion == GOD_NEMELEX_XOBEH )
    {
        result = you.piety;
        result *= (you.skills[SK_EVOCATIONS] + 25);
        result /= 27;
    }

    result += you.skills[SK_EVOCATIONS] * 9;
    if ( rarity == DECK_RARITY_RARE )
        result += random2(result / 2);
    if ( rarity == DECK_RARITY_LEGENDARY )
        result += random2(result);

    return result;
}

bool card_effect(card_type which_card, deck_rarity_type rarity)
{
    bool rc = true;
    const int power = card_power(rarity);
#ifdef DEBUG_DIAGNOSTICS
    msg::streams(MSGCH_DIAGNOSTICS) << "Card power: " << power
                                    << ", rarity: " << static_cast<int>(rarity)
                                    << std::endl;
#endif

    msg::stream << "You have drawn " << card_name( which_card )
                << '.' << std::endl;

    switch (which_card)
    {
    case CARD_BLANK: break;
    case CARD_PORTAL:           portal_card(power, rarity); break;
    case CARD_WARP:             warp_card(power, rarity); break;
    case CARD_SWAP:             swap_monster_card(power, rarity); break;
    case CARD_VELOCITY:         velocity_card(power, rarity); break;
    case CARD_DAMNATION:        damnation_card(power, rarity); break;
    case CARD_SOLITUDE:         cast_dispersal(power/4); break;
    case CARD_ELIXIR:           elixir_card(power, rarity); break;
    case CARD_BATTLELUST:       battle_lust_card(power, rarity); break;
    case CARD_METAMORPHOSIS:    metamorphosis_card(power, rarity); break;
    case CARD_HELM:             helm_card(power, rarity); break;
    case CARD_BLADE:            blade_card(power, rarity); break;
    case CARD_SHADOW:           shadow_card(power, rarity); break;
    case CARD_POTION:           potion_card(power, rarity); break;
    case CARD_FOCUS:            focus_card(power, rarity); break;
    case CARD_SHUFFLE:          shuffle_card(power, rarity); break;
    case CARD_EXPERIENCE:       potion_effect(POT_EXPERIENCE, power/4); break;
    case CARD_HELIX:            helix_card(power, rarity); break;
    case CARD_DOWSING:          dowsing_card(power, rarity); break;
    case CARD_MINEFIELD:        minefield_card(power, rarity); break;
    case CARD_GENIE:            genie_card(power, rarity); break;
    case CARD_CURSE:            curse_card(power, rarity); break;
    case CARD_WARPWRIGHT:       warpwright_card(power, rarity); break;
    case CARD_TOMB:             entomb(power/2); break;
    case CARD_WRAITH:           drain_exp(false); lose_level(); break;
    case CARD_WRATH:            godly_wrath(); break;
    case CARD_SUMMON_DEMON:     summon_demon_card(power, rarity); break;
    case CARD_SUMMON_ANIMAL:    summon_animals(random2(power/3)); break;
    case CARD_SUMMON_ANY:       summon_any_monster(power, rarity); break;
    case CARD_XOM:              xom_acts(5 + random2(power/10)); break;
    case CARD_SUMMON_WEAPON:    summon_dancing_weapon(power, rarity); break;
    case CARD_TROWEL:           trowel_card(power, rarity); break;
    case CARD_SPADE: your_spells(SPELL_DIG, random2(power/4), false); break;
    case CARD_BANSHEE: mass_enchantment(ENCH_FEAR, power, MHITYOU); break;
    case CARD_TORMENT: torment(TORMENT_CARDS, you.x_pos, you.y_pos); break;

    case CARD_VENOM:
        if ( coinflip() )
            your_spells(SPELL_OLGREBS_TOXIC_RADIANCE,random2(power/4), false);
        else
            rc = damaging_card(which_card, power, rarity);
        break;

    case CARD_VITRIOL: case CARD_FLAME: case CARD_FROST: case CARD_HAMMER:
    case CARD_PAIN:
        rc = damaging_card(which_card, power, rarity);
        break;

    case CARD_BARGAIN:
        you.duration[DUR_BARGAIN] += random2(power) + random2(power) + 2;
        break;

    case CARD_MAP:
        if (!magic_mapping( random2(power/10) + 15, random2(power), true))
            mpr("The map is blank.");
        break;

    case CARD_WILD_MAGIC:
        // yes, high power is bad here
        miscast_effect( SPTYP_RANDOM, random2(power/15) + 5,
                        random2(power), 0 );
        break;


    case CARD_FAMINE:
        if (you.is_undead == US_UNDEAD)
            mpr("You feel rather smug.");
        else
            set_hunger(500, true);
        break;

    case CARD_FEAST:
        if (you.is_undead == US_UNDEAD)
            mpr("You feel a horrible emptiness.");
        else
            set_hunger(12000, true);
        break;

    case NUM_CARDS:
        mpr("You have drawn a buggy card!");
        break;
    }

    if (you.religion == GOD_NEMELEX_XOBEH && !rc)
        simple_god_message(" seems disappointed in you.");
    
    return rc;
}

bool is_deck(const item_def &item)
{
    return item.base_type == OBJ_MISCELLANY
        && (item.sub_type >= MISC_DECK_OF_ESCAPE &&
            item.sub_type <= MISC_DECK_OF_DEFENSE);
}

deck_rarity_type deck_rarity(const item_def &item)
{
    ASSERT( is_deck(item) );
    switch (item.colour)
    {
    case BLACK: case BLUE: case GREEN: case CYAN: case RED:
    default:
        return DECK_RARITY_COMMON;
    case MAGENTA: case BROWN:
        return DECK_RARITY_RARE;
    case LIGHTMAGENTA:
        return DECK_RARITY_LEGENDARY;
    }
}
