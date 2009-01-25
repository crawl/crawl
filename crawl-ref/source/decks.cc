/*
 *  File:       decks.cc
 *  Summary:    Functions with decks of cards.
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 */

#include "AppHdr.h"
REVISION("$Rev$");

#include "decks.h"

#include <iostream>
#include <algorithm>

#include "externs.h"

#include "beam.h"
#include "cio.h"
#include "dungeon.h"
#include "effects.h"
#include "files.h"
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
#include "output.h"
#include "player.h"
#include "religion.h"
#include "skills2.h"
#include "spells1.h"
#include "spells2.h"
#include "spells3.h"
#include "spells4.h"
#include "spl-cast.h"
#include "spl-mis.h"
#include "spl-util.h"
#include "state.h"
#include "stuff.h"
#include "terrain.h"
#include "transfor.h"
#include "traps.h"
#include "view.h"
#include "xom.h"

// DECK STRUCTURE: deck.plus is the number of cards the deck *started*
// with, deck.plus2 is the number of cards drawn, deck.special is the
// deck rarity, deck.props["cards"] holds the list of cards (with the
// highest index card being the top card, and index 0 being the bottom
// card), deck.props["drawn_cards"] holds the list of drawn cards
// (with index 0 being the first drawn), deck.props["card_flags"]
// holds the flags for each card, deck.props["num_marked"] is the
// number of marked cards left in the deck, and
// deck.props["non_brownie_draws"] is the number of non-marked draws
// you have to make from that deck before earning brownie points from
// it again.
//
// The card type and per-card flags are each stored as unsigned bytes,
// for a maximum of 256 different kinds of cards and 8 bits of flags.

static void _deck_ident(item_def& deck);

struct card_with_weights
{
    card_type card;
    int weight[3];
};

typedef card_with_weights deck_archetype;

#define END_OF_DECK {NUM_CARDS, {0,0,0}}

const deck_archetype deck_of_transport[] = {
    { CARD_PORTAL,   {5, 5, 5} },
    { CARD_WARP,     {5, 5, 5} },
    { CARD_SWAP,     {5, 5, 5} },
    { CARD_VELOCITY, {5, 5, 5} },
    END_OF_DECK
};

const deck_archetype deck_of_emergency[] = {
    { CARD_TOMB,       {5, 5, 5} },
    { CARD_BANSHEE,    {5, 5, 5} },
    { CARD_DAMNATION,  {0, 1, 2} },
    { CARD_SOLITUDE,   {5, 5, 5} },
    { CARD_WARPWRIGHT, {5, 5, 5} },
    { CARD_FLIGHT,     {5, 5, 5} },
    END_OF_DECK
};

const deck_archetype deck_of_destruction[] = {
    { CARD_VITRIOL, {5, 5, 5} },
    { CARD_FLAME,   {5, 5, 5} },
    { CARD_FROST,   {5, 5, 5} },
    { CARD_VENOM,   {5, 5, 5} },
    { CARD_HAMMER,  {5, 5, 5} },
    { CARD_SPARK,   {5, 5, 5} },
    { CARD_PAIN,    {5, 5, 5} },
    { CARD_TORMENT, {5, 5, 5} },
    END_OF_DECK
};

const deck_archetype deck_of_battle[] = {
    { CARD_ELIXIR,        {5, 5, 5} },
    { CARD_BATTLELUST,    {5, 5, 5} },
    { CARD_METAMORPHOSIS, {5, 5, 5} },
    { CARD_HELM,          {5, 5, 5} },
    { CARD_BLADE,         {5, 5, 5} },
    { CARD_SHADOW,        {5, 5, 5} },
    END_OF_DECK
};

const deck_archetype deck_of_enchantments[] = {
    { CARD_ELIXIR, {5, 5, 5} },
    END_OF_DECK
};

const deck_archetype deck_of_summoning[] = {
    { CARD_CRUSADE,         {5, 5, 5} },
    { CARD_SUMMON_ANIMAL,   {5, 5, 5} },
    { CARD_SUMMON_DEMON,    {5, 5, 5} },
    { CARD_SUMMON_WEAPON,   {5, 5, 5} },
    { CARD_SUMMON_FLYING,   {5, 5, 5} },
    { CARD_SUMMON_SKELETON, {5, 5, 5} },
    { CARD_SUMMON_UGLY,     {5, 5, 5} },
    END_OF_DECK
};

const deck_archetype deck_of_wonders[] = {
    { CARD_POTION,     {5, 5, 5} },
    { CARD_FOCUS,      {1, 1, 2} },
    { CARD_SHUFFLE,    {0, 1, 2} },
    { CARD_EXPERIENCE, {5, 5, 5} },
    { CARD_WILD_MAGIC, {5, 5, 5} },
    { CARD_HELIX,      {5, 5, 5} },
    { CARD_SAGE,       {5, 5, 5} },
    END_OF_DECK
};

const deck_archetype deck_of_dungeons[] = {
    { CARD_WATER,     {5, 5, 5} },
    { CARD_GLASS,     {5, 5, 5} },
    { CARD_MAP,       {5, 5, 5} },
    { CARD_DOWSING,   {5, 5, 5} },
    { CARD_SPADE,     {5, 5, 5} },
    { CARD_TROWEL,    {5, 5, 5} },
    { CARD_MINEFIELD, {5, 5, 5} },
    END_OF_DECK
};

const deck_archetype deck_of_oddities[] = {
    { CARD_GENIE,   {5, 5, 5} },
    { CARD_BARGAIN, {5, 5, 5} },
    { CARD_WRATH,   {5, 5, 5} },
    { CARD_XOM,     {5, 5, 5} },
    { CARD_FEAST,   {5, 5, 5} },
    { CARD_FAMINE,  {5, 5, 5} },
    { CARD_CURSE,   {5, 5, 5} },
    END_OF_DECK
};

const deck_archetype deck_of_punishment[] = {
    { CARD_WRAITH,     {5, 5, 5} },
    { CARD_WILD_MAGIC, {5, 5, 5} },
    { CARD_WRATH,      {5, 5, 5} },
    { CARD_XOM,        {5, 5, 5} },
    { CARD_FAMINE,     {5, 5, 5} },
    { CARD_CURSE,      {5, 5, 5} },
    { CARD_TOMB,       {5, 5, 5} },
    { CARD_DAMNATION,  {5, 5, 5} },
    { CARD_PORTAL,     {5, 5, 5} },
    { CARD_MINEFIELD,  {5, 5, 5} },
    END_OF_DECK
};

static void _check_odd_card(unsigned char flags)
{
    if ((flags & CFLAG_ODDITY) && !(flags & CFLAG_SEEN))
        mpr("This card doesn't seem to belong here.");
}

int cards_in_deck(const item_def &deck)
{
    ASSERT(is_deck(deck));

    const CrawlHashTable &props = deck.props;
    ASSERT(props.exists("cards"));

    return (props["cards"].get_vector().size());
}

static void _shuffle_deck(item_def &deck)
{
    ASSERT(is_deck(deck));

    CrawlHashTable &props = deck.props;
    ASSERT(props.exists("cards"));

    CrawlVector &cards = props["cards"];

    CrawlVector &flags = props["card_flags"];
    ASSERT(flags.size() == cards.size());

    // Don't use std::shuffle(), since we want to apply exactly the
    // same shuffling to both the cards vector and the flags vector.
    std::vector<vec_size> pos;
    for (unsigned long i = 0; i < cards.size(); i++)
        pos.push_back(random2(cards.size()));

    for (vec_size i = 0; i < pos.size(); i++)
    {
        std::swap(cards[i], cards[pos[i]]);
        std::swap(flags[i], flags[pos[i]]);
    }
}

card_type get_card_and_flags(const item_def& deck, int idx,
                             unsigned char& _flags)
{
    const CrawlHashTable &props = deck.props;
    const CrawlVector    &cards = props["cards"].get_vector();
    const CrawlVector    &flags = props["card_flags"].get_vector();

    // Negative idx means read from the end.
    if (idx < 0)
        idx += static_cast<int>(cards.size());

    _flags = (unsigned char) flags[idx].get_byte();

    return static_cast<card_type>(cards[idx].get_byte());
}

static void _set_card_and_flags(item_def& deck, int idx, card_type card,
                                unsigned char _flags)
{
    CrawlHashTable &props = deck.props;
    CrawlVector    &cards = props["cards"];
    CrawlVector    &flags = props["card_flags"];

    if (idx == -1)
        idx = static_cast<int>(cards.size()) - 1;

    cards[idx] = (char) card;
    flags[idx] = (char) _flags;
}

const char* card_name(card_type card)
{
    switch (card)
    {
    case CARD_PORTAL:          return "the Portal";
    case CARD_WARP:            return "the Warp";
    case CARD_SWAP:            return "Swap";
    case CARD_VELOCITY:        return "Velocity";
    case CARD_DAMNATION:       return "Damnation";
    case CARD_SOLITUDE:        return "Solitude";
    case CARD_ELIXIR:          return "the Elixir";
    case CARD_BATTLELUST:      return "Battlelust";
    case CARD_METAMORPHOSIS:   return "Metamorphosis";
    case CARD_HELM:            return "the Helm";
    case CARD_BLADE:           return "the Blade";
    case CARD_SHADOW:          return "the Shadow";
    case CARD_POTION:          return "the Potion";
    case CARD_FOCUS:           return "Focus";
    case CARD_SHUFFLE:         return "Shuffle";
    case CARD_EXPERIENCE:      return "Experience";
    case CARD_HELIX:           return "the Helix";
    case CARD_SAGE:            return "the Sage";
    case CARD_DOWSING:         return "Dowsing";
    case CARD_TROWEL:          return "the Trowel";
    case CARD_MINEFIELD:       return "the Minefield";
    case CARD_STAIRS:          return "the Stairs";
    case CARD_GENIE:           return "the Genie";
    case CARD_TOMB:            return "the Tomb";
    case CARD_WATER:           return "Water";
    case CARD_GLASS:           return "Vitrification";
    case CARD_MAP:             return "the Map";
    case CARD_BANSHEE:         return "the Banshee";
    case CARD_WILD_MAGIC:      return "Wild Magic";
    case CARD_CRUSADE:         return "the Crusade";
    case CARD_SUMMON_ANIMAL:   return "the Herd";
    case CARD_SUMMON_DEMON:    return "the Pentagram";
    case CARD_SUMMON_WEAPON:   return "the Dance";
    case CARD_SUMMON_FLYING:   return "Foxfire";
    case CARD_SUMMON_SKELETON: return "the Bones";
    case CARD_SUMMON_UGLY:     return "Repulsiveness";
    case CARD_SUMMON_ANY:      return "Summoning";
    case CARD_XOM:             return "Xom";
    case CARD_FAMINE:          return "Famine";
    case CARD_FEAST:           return "the Feast";
    case CARD_WARPWRIGHT:      return "Warpwright";
    case CARD_FLIGHT:          return "Flight";
    case CARD_VITRIOL:         return "Vitriol";
    case CARD_FLAME:           return "Flame";
    case CARD_FROST:           return "Frost";
    case CARD_VENOM:           return "Venom";
    case CARD_SPARK:           return "the Spark";
    case CARD_HAMMER:          return "the Hammer";
    case CARD_PAIN:            return "Pain";
    case CARD_TORMENT:         return "Torment";
    case CARD_SPADE:           return "the Spade";
    case CARD_BARGAIN:         return "the Bargain";
    case CARD_WRATH:           return "Wrath";
    case CARD_WRAITH:          return "the Wraith";
    case CARD_CURSE:           return "the Curse";
    case NUM_CARDS:            return "a buggy card";
    }
    return "a very buggy card";
}

static const deck_archetype* _random_sub_deck(unsigned char deck_type)
{
    const deck_archetype *pdeck = NULL;
    switch (deck_type)
    {
    case MISC_DECK_OF_ESCAPE:
        pdeck = (coinflip() ? deck_of_transport : deck_of_emergency);
        break;
    case MISC_DECK_OF_DESTRUCTION: pdeck = deck_of_destruction; break;
    case MISC_DECK_OF_DUNGEONS:    pdeck = deck_of_dungeons;    break;
    case MISC_DECK_OF_SUMMONING:   pdeck = deck_of_summoning;   break;
    case MISC_DECK_OF_WONDERS:     pdeck = deck_of_wonders;     break;
    case MISC_DECK_OF_PUNISHMENT:  pdeck = deck_of_punishment;  break;
    case MISC_DECK_OF_WAR:
        switch (random2(6))
        {
        case 0: pdeck = deck_of_destruction;  break;
        case 1: pdeck = deck_of_enchantments; break;
        case 2: pdeck = deck_of_battle;       break;
        case 3: pdeck = deck_of_summoning;    break;
        case 4: pdeck = deck_of_transport;    break;
        case 5: pdeck = deck_of_emergency;    break;
        }
        break;
    case MISC_DECK_OF_CHANGES:
        switch (random2(3))
        {
        case 0: pdeck = deck_of_battle;       break;
        case 1: pdeck = deck_of_dungeons;     break;
        case 2: pdeck = deck_of_wonders;      break;
        }
        break;
    case MISC_DECK_OF_DEFENCE:
        pdeck = (coinflip() ? deck_of_emergency : deck_of_battle);
        break;
    }

    ASSERT(pdeck);

    return (pdeck);
}

static card_type _choose_from_archetype(const deck_archetype* pdeck,
                                        deck_rarity_type rarity)
{
    // We assume here that common == 0, rare == 1, legendary == 2.

    // FIXME: We should use one of the various choose_random_weighted
    // functions here, probably with an iterator, instead of
    // duplicating the implementation.

    int totalweight = 0;
    card_type result = NUM_CARDS;
    for (int i = 0; pdeck[i].card != NUM_CARDS; ++i)
    {
        const card_with_weights& cww = pdeck[i];
        totalweight += cww.weight[rarity];
        if (x_chance_in_y(cww.weight[rarity], totalweight))
            result = cww.card;
    }
    return result;
}

static card_type _random_card(unsigned char deck_type, deck_rarity_type rarity,
                              bool &was_oddity)
{
    const deck_archetype *pdeck = _random_sub_deck(deck_type);

    if (one_chance_in(100))
    {
        pdeck      = deck_of_oddities;
        was_oddity = true;
    }

    return _choose_from_archetype(pdeck, rarity);
}

static card_type _random_card(const item_def& item, bool &was_oddity)
{
    return _random_card(item.sub_type, deck_rarity(item), was_oddity);
}

static card_type _draw_top_card(item_def& deck, bool message,
                                unsigned char &_flags)
{
    CrawlHashTable &props = deck.props;
    CrawlVector    &cards = props["cards"].get_vector();
    CrawlVector    &flags = props["card_flags"].get_vector();

    int num_cards = cards.size();
    int idx       = num_cards - 1;

    ASSERT(num_cards > 0);

    card_type card = get_card_and_flags(deck, idx, _flags);
    cards.pop_back();
    flags.pop_back();

    if (message)
    {
        if (_flags & CFLAG_MARKED)
            mprf("You draw %s.", card_name(card));
        else
            mprf("You draw a card... It is %s.", card_name(card));

        _check_odd_card(_flags);
    }

    return card;
}

static void _push_top_card(item_def& deck, card_type card,
                           unsigned char _flags)
{
    CrawlHashTable &props = deck.props;
    CrawlVector    &cards = props["cards"].get_vector();
    CrawlVector    &flags = props["card_flags"].get_vector();

    cards.push_back((char) card);
    flags.push_back((char) _flags);
}

static bool _wielding_deck()
{
    return (you.weapon() && is_deck(*you.weapon()));
}

static void _remember_drawn_card(item_def& deck, card_type card, bool allow_id)
{
    ASSERT( is_deck(deck) );
    CrawlHashTable &props = deck.props;
    CrawlVector &drawn = props["drawn_cards"].get_vector();
    drawn.push_back( static_cast<char>(card) );

    // Once you've drawn two cards, you know the deck.
    if (allow_id &&
        (drawn.size() >= 2 || origin_is_god_gift(deck)))
    {
        _deck_ident(deck);
    }
}

const std::vector<card_type> get_drawn_cards(const item_def& deck)
{
    std::vector<card_type> result;
    if (is_deck(deck))
    {
        const CrawlHashTable &props = deck.props;
        const CrawlVector &drawn = props["drawn_cards"].get_vector();
        for (unsigned int i = 0; i < drawn.size(); ++i)
        {
            const char tmp = drawn[i];
            result.push_back(static_cast<card_type>(tmp));
        }
    }
    return result;
}

static bool _check_buggy_deck(item_def& deck)
{
    std::ostream& strm = msg::streams(MSGCH_DIAGNOSTICS);
    if (!is_deck(deck))
    {
        crawl_state.zero_turns_taken();
        strm << "This isn't a deck at all!" << std::endl;
        return (true);
    }

    CrawlHashTable &props = deck.props;

    if (!props.exists("cards")
        || props["cards"].get_type() != SV_VEC
        || props["cards"].get_vector().get_type() != SV_BYTE
        || cards_in_deck(deck) == 0)
    {
        crawl_state.zero_turns_taken();

        if (!props.exists("cards"))
            strm << "Seems this deck never had any cards in the first place!";
        else if (props["cards"].get_type() != SV_VEC)
            strm << "'cards' property isn't a vector.";
        else
        {
            if (props["cards"].get_vector().get_type() != SV_BYTE)
                strm << "'cards' vector doesn't contain bytes.";

            if (cards_in_deck(deck) == 0)
            {
                strm << "Strange, this deck is already empty.";

                int cards_left = 0;
                if (deck.plus2 >= 0)
                    cards_left = deck.plus - deck.plus2;
                else
                    cards_left = -deck.plus;

                if (cards_left != 0)
                {
                    strm << " But there should have been " <<  cards_left
                         << " cards left.";
                }
            }
        }
        strm << std::endl
             << "A swarm of software bugs snatches the deck from you "
            "and whisks it away."
             << std::endl;

        if (deck.link == you.equip[EQ_WEAPON])
            unwield_item();

        dec_inv_item_quantity( deck.link, 1 );
        did_god_conduct(DID_CARDS, 1);

        return (true);
    }

    bool problems = false;

    CrawlVector &cards = props["cards"].get_vector();
    CrawlVector &flags = props["card_flags"].get_vector();

    vec_size num_cards = cards.size();
    vec_size num_flags = flags.size();

    unsigned int num_buggy     = 0;
    unsigned int num_marked    = 0;

    for (vec_size i = 0; i < num_cards; i++)
    {
        unsigned char card   = cards[i].get_byte();
        unsigned char _flags = flags[i].get_byte();
        if (card >= NUM_CARDS)
        {
            cards.erase(i);
            flags.erase(i);
            i--;
            num_cards--;
            num_buggy++;
        }
        else
        {
            if (_flags & CFLAG_MARKED)
                num_marked++;
        }
    }

    if (num_buggy > 0)
    {
        strm << num_buggy << " buggy cards found in the deck, discarding them."
             << std::endl;

        deck.plus2 += num_buggy;

        num_cards = cards.size();
        num_flags = cards.size();

        problems = true;
    }

    if (num_cards == 0)
    {
        crawl_state.zero_turns_taken();

        strm << "Oops, all of the cards seem to be gone." << std::endl
             << "A swarm of software bugs snatches the deck from you "
             "and whisks it away." << std::endl;

        if (deck.link == you.equip[EQ_WEAPON])
            unwield_item();

        dec_inv_item_quantity( deck.link, 1 );
        did_god_conduct(DID_CARDS, 1);

        return (true);
    }

    if (static_cast<long>(num_cards) > deck.plus)
    {
        if (deck.plus == 0)
            strm << "Deck was created with zero cards???" << std::endl;
        else if (deck.plus < 0)
            strm << "Deck was created with *negative* cards?!" << std::endl;
        else
        {
            strm << "Deck has more cards than it was created with?"
                 << std::endl;
        }

        deck.plus = num_cards;
        problems  = true;
    }

    if (num_cards > num_flags)
    {
#ifdef WIZARD
        strm << (num_cards - num_flags) << " more cards than flags.";
#else
        strm << "More cards than flags.";
#endif
        strm << std::endl;
        for (unsigned int i = num_flags + 1; i <= num_cards; i++)
            flags[i] = static_cast<char>(0);

        problems = true;
    }
    else if (num_flags > num_cards)
    {
#ifdef WIZARD
        strm << (num_cards - num_flags) << " more cards than flags.";
#else
        strm << "More cards than flags.";
#endif
        strm << std::endl;

        for (unsigned int i = num_flags; i > num_cards; i--)
            flags.erase(i);

        problems = true;
    }

    if (props["num_marked"].get_byte() > static_cast<char>(num_cards))
    {
        strm << "More cards marked than in the deck?" << std::endl;
        props["num_marked"] = static_cast<char>(num_marked);
        problems = true;
    }
    else if (props["num_marked"].get_byte() != static_cast<char>(num_marked))
    {
#ifdef WIZARD

        strm << "Oops, counted " << static_cast<int>(num_marked)
             << " marked cards, but num_marked is "
             << (static_cast<int>(props["num_marked"].get_byte()));
#else
        strm << "Oops, book-keeping on marked cards is wrong.";
#endif
        strm << std::endl;

        props["num_marked"] = static_cast<char>(num_marked);
        problems = true;
    }

    if (deck.plus2 >= 0)
    {
        if (deck.plus != (deck.plus2 + static_cast<long>(num_cards)))
        {
#ifdef WIZARD
            strm << "Have you used " << deck.plus2 << " cards, or "
                 << (deck.plus - num_cards) << "? Oops.";
#else
            strm << "Oops, book-keeping on used cards is wrong.";
#endif
            strm << std::endl;
            deck.plus2 = deck.plus - num_cards;
            problems = true;
        }
    }
    else
    {
        if (-deck.plus2 != static_cast<long>(num_cards))
        {
#ifdef WIZARD
            strm << "There are " << num_cards << " cards left, not "
                 << (-deck.plus2) << ".  Oops.";
#else
            strm << "Oops, book-keeping on cards left is wrong.";
#endif
            strm << std::endl;
            deck.plus2 = -num_cards;
            problems = true;
        }
    }

    if (!problems)
        return (false);

    you.wield_change = true;

    if (!yesno("Problems might not have been completely fixed; "
               "still use deck?", true, 'n'))
    {
        crawl_state.zero_turns_taken();
        return (true);
    }
    return (false);
}

// Choose a deck from inventory and return its slot (or -1).
static int _choose_inventory_deck( const char* prompt )
{
    const int slot = prompt_invent_item( prompt,
                                         MT_INVLIST, OSEL_DRAW_DECK,
                                         true, true, true, 0, -1, NULL,
                                         OPER_EVOKE );

    if (prompt_failed(slot))
        return -1;

    if (!is_deck(you.inv[slot]))
    {
        mpr("That isn't a deck!");
        return -1;
    }

    return slot;
}

// Select a deck from inventory and draw a card from it.
bool choose_deck_and_draw()
{
    const int slot = _choose_inventory_deck( "Draw from which deck?" );

    if (slot == -1)
    {
        crawl_state.zero_turns_taken();
        return (false);
    }

    evoke_deck(you.inv[slot]);
    return (true);
}

static void _deck_ident(item_def& deck)
{
    if (in_inventory(deck) && !item_ident(deck, ISFLAG_KNOW_TYPE))
    {
        set_ident_flags(deck, ISFLAG_KNOW_TYPE);
        mprf("This is %s.", deck.name(DESC_NOCAP_A).c_str());
        you.wield_change = true;
    }
}

// This also shuffles the deck.
static void _deck_lose_card(item_def& deck)
{
    unsigned char flags = 0;
    // Seen cards are only half as likely to fall out,
    // marked cards only one-quarter as likely (note that marked
    // cards are also seen.)
    do
    {
        _shuffle_deck(deck);
        get_card_and_flags(deck, -1, flags);
    }
    while ( (flags & CFLAG_MARKED) && coinflip()
            || (flags & CFLAG_SEEN) && coinflip() );

    _draw_top_card(deck, false, flags);
    deck.plus2++;
}

// Peek at two cards in a deck, then shuffle them back in.
// Return false if the operation was failed/aborted along the way.
bool deck_peek()
{
    const int slot = _choose_inventory_deck( "Peek at which deck?" );
    if (slot == -1)
    {
        crawl_state.zero_turns_taken();
        return (false);
    }
    item_def& deck(you.inv[slot]);

    if (_check_buggy_deck(deck))
        return (false);

    if (cards_in_deck(deck) > 2)
    {
        _deck_lose_card(deck);
        mpr("A card falls out of the deck.");
    }

    CrawlVector &cards     = deck.props["cards"];
    const int    num_cards = cards.size();

    card_type card1, card2;
    unsigned char flags1, flags2;

    card1 = get_card_and_flags(deck, 0, flags1);

    if (num_cards == 1)
    {
        mpr("There's only one card in the deck!");

        _set_card_and_flags(deck, 0, card1, flags1 | CFLAG_SEEN | CFLAG_MARKED);
        deck.props["num_marked"]++;
        deck.plus2 = -1;
        you.wield_change = true;

        return (true);
    }

    card2 = get_card_and_flags(deck, 1, flags2);

    int already_seen = 0;
    if (flags1 & CFLAG_SEEN)
        already_seen++;
    if (flags2 & CFLAG_SEEN)
        already_seen++;

    // Always increase if seen 2, 50% increase if seen 1.
    if (already_seen && x_chance_in_y(already_seen, 2))
        deck.props["non_brownie_draws"]++;

    mprf("You draw two cards from the deck. They are: %s and %s.",
         card_name(card1), card_name(card2));

    _set_card_and_flags(deck, 0, card1, flags1 | CFLAG_SEEN);
    _set_card_and_flags(deck, 1, card2, flags2 | CFLAG_SEEN);

    mpr("You shuffle the cards back into the deck.");
    _shuffle_deck(deck);

    // Peeking identifies the deck.
    _deck_ident(deck);

    you.wield_change = true;
    return (true);
}

// Mark a deck: look at the next four cards, mark them, and shuffle
// them back into the deck. The player won't know what order they're
// in, and if the top card is non-marked then the player won't
// know what the next card is.  Return false if the operation was
// failed/aborted along the way.
bool deck_mark()
{
    const int slot = _choose_inventory_deck( "Mark which deck?" );
    if (slot == -1)
    {
        crawl_state.zero_turns_taken();
        return (false);
    }
    item_def& deck(you.inv[slot]);
    if (_check_buggy_deck(deck))
        return (false);

    CrawlHashTable &props = deck.props;
    if (props["num_marked"].get_byte() > 0)
    {
        mpr("The deck is already marked.");
        crawl_state.zero_turns_taken();
        return (false);
    }

    // Lose some cards, but keep at least two.
    if (cards_in_deck(deck) > 2)
    {
        const int num_lost = std::min(cards_in_deck(deck)-2, random2(3) + 1);
        for (int i = 0; i < num_lost; ++i)
            _deck_lose_card(deck);

        if (num_lost == 1)
            mpr("A card falls out of the deck.");
        else if (num_lost > 1)
            mpr("Some cards fall out of the deck.");
    }

    const int num_cards   = cards_in_deck(deck);
    const int num_to_mark = (num_cards < 4 ? num_cards : 4);

    if (num_cards == 1)
        mpr("There's only one card left!");
    else if (num_cards < 4)
        mprf("The deck only has %d cards.", num_cards);

    std::vector<std::string> names;
    for (int i = 0; i < num_to_mark; ++i)
    {
        unsigned char flags;
        card_type     card = get_card_and_flags(deck, i, flags);

        flags |= CFLAG_SEEN | CFLAG_MARKED;
        _set_card_and_flags(deck, i, card, flags);

        names.push_back(card_name(card));
    }
    mpr_comma_separated_list("You draw and mark ", names);
    props["num_marked"] = (char) num_to_mark;

    if (num_cards == 1)
        ;
    else if (num_cards < 4)
    {
        mprf("You shuffle the deck.");
        deck.plus2 = -num_cards;
    }
    else
        mprf("You shuffle the cards back into the deck.");

    _shuffle_deck(deck);
    _deck_ident(deck);
    you.wield_change = true;

    return (true);
}

static void _redraw_stacked_cards(const std::vector<card_type>& draws,
                                  unsigned int selected)
{
    for (unsigned int i = 0; i < draws.size(); ++i)
    {
        cgotoxy(1, i+2);
        textcolor(selected == i ? WHITE : LIGHTGREY);
        cprintf("%u - %s", i+1, card_name(draws[i]) );
        clear_to_end_of_line();
    }
}


// Stack a deck: look at the next five cards, put them back in any
// order, discard the rest of the deck.
// Return false if the operation was failed/aborted along the way.
bool deck_stack()
{
    cursor_control con(false);
    if (!_wielding_deck())
    {
        mpr("You aren't wielding a deck!");
        crawl_state.zero_turns_taken();
        return (false);
    }
    item_def& deck(*you.weapon());
    if (_check_buggy_deck(deck))
        return (false);

    CrawlHashTable &props = deck.props;
    if (props["num_marked"].get_byte() > 0)
    {
        mpr("You can't stack a marked deck.");
        crawl_state.zero_turns_taken();
        return (false);
    }

    const int num_cards    = cards_in_deck(deck);
    const int num_to_stack = (num_cards < 5 ? num_cards : 5);

    std::vector<card_type>     draws;
    std::vector<unsigned char> flags;
    for (int i = 0; i < num_cards; ++i)
    {
        unsigned char _flags;
        card_type     card = _draw_top_card(deck, false, _flags);

        if (i < num_to_stack)
        {
            draws.push_back(card);
            flags.push_back(_flags | CFLAG_SEEN | CFLAG_MARKED);
        }
        // Rest of deck is discarded.
    }

    if (num_cards == 1)
        mpr("There's only one card left!");
    else if (num_cards < 5)
        mprf("The deck only has %d cards.", num_to_stack);
    else if (num_cards == 5)
        mpr("The deck has exactly five cards.");
    else
    {
        mprf("You draw the first five cards out of %d and discard the rest.",
             num_cards);
    }
    more();

    if (draws.size() > 1)
    {
        unsigned int selected = draws.size();
        clrscr();
        cgotoxy(1,1);
        textcolor(WHITE);
        cprintf("Press a digit to select a card, "
                "then another digit to swap it.");
        cgotoxy(1,10);
        cprintf("Press Enter to accept.");

        _redraw_stacked_cards(draws, selected);

        // Hand-hacked implementation, instead of using Menu. Oh well.
        while (true)
        {
            const int c = getch();
            if (c == CK_ENTER)
            {
                cgotoxy(1,11);
                textcolor(LIGHTGREY);
                cprintf("Are you sure? (press y or Y to confirm)");
                if (toupper(getch()) == 'Y')
                    break;

                cgotoxy(1,11);
                clear_to_end_of_line();
                continue;
            }

            if (c >= '1' && c <= '0' + static_cast<int>(draws.size()))
            {
                const unsigned int new_selected = c - '1';
                if (selected < draws.size())
                {
                    std::swap(draws[selected], draws[new_selected]);
                    std::swap(flags[selected], flags[new_selected]);
                    selected = draws.size();
                }
                else
                    selected = new_selected;

                _redraw_stacked_cards(draws, selected);
            }
        }
        redraw_screen();
    }

    deck.plus2 = -num_to_stack;
    for (unsigned int i = 0; i < draws.size(); ++i)
    {
        _push_top_card(deck, draws[draws.size() - 1 - i],
                       flags[flags.size() - 1 - i]);
    }

    props["num_marked"] = static_cast<char>(num_to_stack);
    you.wield_change = true;

    _check_buggy_deck(deck);

    return (true);
}

// Draw the next three cards, discard two and pick one.
bool deck_triple_draw()
{
    const int slot = _choose_inventory_deck("Triple draw from which deck?");
    if (slot == -1)
    {
        crawl_state.zero_turns_taken();
        return (false);
    }

    item_def& deck(you.inv[slot]);

    if (_check_buggy_deck(deck))
        return (false);

    const int num_cards = cards_in_deck(deck);

    // We have to identify the deck before removing cards from it.
    // Otherwise, _remember_drawn_card() will implicitly call
    // _deck_ident() when the deck might have no cards left.
    _deck_ident(deck);

    if (num_cards == 1)
    {
        // Only one card to draw, so just draw it.
        evoke_deck(deck);
        return (true);
    }

    const int num_to_draw = (num_cards < 3 ? num_cards : 3);
    std::vector<card_type>     draws;
    std::vector<unsigned char> flags;

    for (int i = 0; i < num_to_draw; ++i)
    {
        unsigned char _flags;
        card_type     card = _draw_top_card(deck, false, _flags);

        draws.push_back(card);
        flags.push_back(_flags | CFLAG_SEEN | CFLAG_MARKED);
    }

    mpr("You draw... (choose one card)");
    for (int i = 0; i < num_to_draw; ++i)
    {
        msg::streams(MSGCH_PROMPT) << (static_cast<char>(i + 'a')) << " - "
                                   << card_name(draws[i]) << std::endl;
    }
    int selected = -1;
    while (true)
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

    // Note how many cards were removed from the deck.
    deck.plus2 += num_to_draw;
    for (int i = 0; i < num_to_draw; ++i)
        _remember_drawn_card(deck, draws[i], false);
    you.wield_change = true;

    // Make deck disappear *before* the card effect, since we
    // don't want to unwield an empty deck.
    deck_rarity_type rarity = deck_rarity(deck);
    if (cards_in_deck(deck) == 0)
    {
        mpr("The deck of cards disappears in a puff of smoke.");
        if (slot == you.equip[EQ_WEAPON])
            unwield_item();

        dec_inv_item_quantity( slot, 1 );
    }

    // Note that card_effect() might cause you to unwield the deck.
    card_effect(draws[selected], rarity, flags[selected], false);

    return (true);
}

// This is Nemelex retribution.
void draw_from_deck_of_punishment()
{
    bool oddity;
    card_type card = _random_card(MISC_DECK_OF_PUNISHMENT, DECK_RARITY_COMMON,
                                  oddity);

    mpr("You draw a card...");
    card_effect(card, DECK_RARITY_COMMON);
}

static int _xom_check_card(item_def &deck, card_type card,
                           unsigned char flags)
{
    int amusement = 64;

    if (!item_type_known(deck))
        amusement *= 2;
    // Expecting one type of card but got another, real funny.
    else if (flags & CFLAG_ODDITY)
        amusement = 255;

    if (player_in_a_dangerous_place())
        amusement *= 2;

    switch (card)
    {
    case CARD_XOM:
        // Handled elsewhere
        amusement = 0;
        break;

    case CARD_DAMNATION:
        // Nothing happened, boring.
        if (you.level_type != LEVEL_DUNGEON)
            amusement = 0;
        break;

    case CARD_MINEFIELD:
    case CARD_FAMINE:
    case CARD_CURSE:
        // Always hilarious.
        amusement = 255;

    default:
        break;
    }

    return amusement;
}

void evoke_deck( item_def& deck )
{
    if (_check_buggy_deck(deck))
        return;

    int brownie_points = 0;
    bool allow_id = in_inventory(deck) && !item_ident(deck, ISFLAG_KNOW_TYPE);

    const deck_rarity_type rarity = deck_rarity(deck);
    CrawlHashTable &props = deck.props;

    unsigned char flags = 0;
    card_type card = _draw_top_card(deck, true, flags);

    // Oddity cards don't give any information about the deck.
    if (flags & CFLAG_ODDITY)
        allow_id = false;

    // Passive Nemelex retribution: sometimes a card gets swapped out.
    // More likely to happen with marked decks.
    if (you.penance[GOD_NEMELEX_XOBEH])
    {
        int c = 1;
        if ((flags & (CFLAG_MARKED | CFLAG_SEEN))
            || props["num_marked"].get_byte() > 0)
        {
            c = 3;
        }

        if (x_chance_in_y(c * you.penance[GOD_NEMELEX_XOBEH], 3000))
        {
            card_type old_card = card;
            card = _choose_from_archetype(deck_of_punishment, rarity);
            if (card != old_card)
            {
                simple_god_message(" seems to have exchanged this card "
                                   "behind your back!", GOD_NEMELEX_XOBEH);
                mprf("It's actually %s.", card_name(card));
                // You never completely appease Nemelex, but the effects
                // get less frequent.
                you.penance[GOD_NEMELEX_XOBEH] -=
                    random2( (you.penance[GOD_NEMELEX_XOBEH]+18) / 10);
            }
        }
    }

    const int amusement   = _xom_check_card(deck, card, flags);
    const bool no_brownie = (props["non_brownie_draws"].get_byte() > 0);

    // Do these before the deck item_def object is gone.
    if (flags & CFLAG_MARKED)
        props["num_marked"]--;
    if (no_brownie)
        props["non_brownie_draws"]--;

    deck.plus2++;
    _remember_drawn_card(deck, card, allow_id);

    // Get rid of the deck *before* the card effect because a card
    // might cause a wielded deck to be swapped out for something else,
    // in which case we don't want an empty deck to go through the
    // swapping process.
    const bool deck_gone = (cards_in_deck(deck) == 0);
    if (deck_gone)
    {
        mpr("The deck of cards disappears in a puff of smoke.");
        dec_inv_item_quantity( deck.link, 1 );
        // Finishing the deck will earn a point, even if it
        // was marked or stacked.
        brownie_points++;
    }

    const bool fake_draw = !card_effect(card, rarity, flags, false);
    if (fake_draw && !deck_gone)
        props["non_brownie_draws"]++;

    if (!(flags & CFLAG_MARKED))
    {
        // Could a Xom worshipper ever get a stacked deck in the first
        // place?
        xom_is_stimulated(amusement);

        // Nemelex likes gamblers.
        if (!no_brownie)
        {
            brownie_points++;
            if (one_chance_in(3))
                brownie_points++;
        }

        // You can't ID off a marked card
        allow_id = false;
    }

    if (!deck_gone && allow_id
        && you.skills[SK_EVOCATIONS] > 5 + random2(35))
    {
        mpr("Your skill with magical items lets you identify the deck.");
        set_ident_flags( deck, ISFLAG_KNOW_TYPE );
        msg::streams(MSGCH_EQUIPMENT) << deck.name(DESC_INVENTORY)
                                      << std::endl;
    }

    if (!fake_draw)
        did_god_conduct(DID_CARDS, brownie_points);

    // Always wield change, since the number of cards used/left has
    // changed.
    you.wield_change = true;
}

int get_power_level(int power, deck_rarity_type rarity)
{
    int power_level = 0;
    switch (rarity)
    {
    case DECK_RARITY_COMMON:
        break;
    case DECK_RARITY_LEGENDARY:
        if (x_chance_in_y(power, 500))
            ++power_level;
        // deliberate fall-through
    case DECK_RARITY_RARE:
        if (x_chance_in_y(power, 700))
            ++power_level;
        break;
    }
    return power_level;
}

// Actual card implementations follow.
static void _portal_card(int power, deck_rarity_type rarity)
{
    const int control_level = get_power_level(power, rarity);
    bool instant = false;
    bool controlled = false;
    if (control_level >= 2)
    {
        instant = true;
        controlled = true;
    }
    else if (control_level == 1)
    {
        if (coinflip())
            instant = true;
        else
            controlled = true;
    }

    const bool was_controlled = player_control_teleport();
    const bool short_control = (you.duration[DUR_CONTROL_TELEPORT] > 0)
        && (you.duration[DUR_CONTROL_TELEPORT] < 6);

    if (controlled && (!was_controlled || short_control))
        you.duration[DUR_CONTROL_TELEPORT] = 6; // Long enough to kick in.

    if (instant)
        you_teleport_now( true );
    else
        you_teleport();
}

static void _warp_card(int power, deck_rarity_type rarity)
{
    const int control_level = get_power_level(power, rarity);
    if (control_level >= 2)
        blink(1000, false);
    else if (control_level == 1)
        cast_semi_controlled_blink(power / 4);
    else
        random_blink(false);
}

static void _swap_monster_card(int power, deck_rarity_type rarity)
{
    // Swap between you and another monster.
    // Don't choose yourself unless there are no monsters nearby.
    monsters *mon_to_swap = choose_random_nearby_monster(0);
    if (!mon_to_swap)
    {
        mpr("You spin around.");
    }
    else
    {
        monsters& mon(*mon_to_swap);
        const coord_def newpos = mon.pos();

        // Be nice: no swapping into uninhabitable environments.
        if (!you.is_habitable(newpos) || !mon.is_habitable(you.pos()))
        {
            mpr("You spin around.");
            return;
        }

        bool mon_caught = mons_is_caught(&mon);
        bool you_caught = you.attribute[ATTR_HELD];

        // Pick the monster up.
        mgrd(newpos) = NON_MONSTER;

        mon.moveto(you.pos());

        // Plunk it down.
        mgrd(mon.pos()) = monster_index(mon_to_swap);

        if (you_caught)
        {
            check_net_will_hold_monster(&mon);
            if (!mon_caught)
                you.attribute[ATTR_HELD] = 0;
        }

        // Move you to its previous location.
        // FIXME: this should also handle merfolk swimming, etc.
        you.moveto(newpos);

        if (mon_caught)
        {
            if (you.body_size(PSIZE_BODY) >= SIZE_GIANT)
            {
                mpr("The net rips apart!");
                you.attribute[ATTR_HELD] = 0;
                int net = get_trapping_net(you.pos());
                if (net != NON_ITEM)
                    destroy_item(net);
            }
            else
            {
                you.attribute[ATTR_HELD] = 10;
                mpr("You become entangled in the net!");

                // Xom thinks this is hilarious if you trap yourself this way.
                if (you_caught)
                    xom_is_stimulated(16);
                else
                    xom_is_stimulated(255);
            }

            if (!you_caught)
                mon.del_ench(ENCH_HELD, true);
        }
    }
}

static void _velocity_card(int power, deck_rarity_type rarity)
{
    const int power_level = get_power_level(power, rarity);
    if (power_level >= 2)
        potion_effect( POT_SPEED, random2(power / 4) );
    else if (power_level == 1)
    {
        cast_fly( random2(power/4) );
        cast_swiftness( random2(power/4) );
    }
    else
        cast_swiftness( random2(power/4) );
}

static void _damnation_card(int power, deck_rarity_type rarity)
{
    if (you.level_type != LEVEL_DUNGEON)
    {
        canned_msg(MSG_NOTHING_HAPPENS);
        return;
    }

    // Calculate how many extra banishments you get.
    const int power_level = get_power_level(power, rarity);
    int nemelex_bonus = 0;
    if (you.religion == GOD_NEMELEX_XOBEH && !player_under_penance())
        nemelex_bonus = you.piety / 20;

    int extra_targets = power_level + random2(you.skills[SK_EVOCATIONS]
                                              + nemelex_bonus) / 12;

    for (int i = 0; i < 1 + extra_targets; ++i)
    {
        // Pick a random monster nearby to banish (or yourself).
        monsters *mon_to_banish = choose_random_nearby_monster(1);

        // Bonus banishments only banish monsters.
        if (i != 0 && !mon_to_banish)
            continue;

        if (!mon_to_banish) // Banish yourself!
        {
            banished(DNGN_ENTER_ABYSS, "drawing a card");
            break;              // Don't banish anything else.
        }
        else
            mon_to_banish->banish();
    }

}

static void _warpwright_card(int power, deck_rarity_type rarity)
{
    if (you.level_type == LEVEL_ABYSS)
    {
        mpr("The power of the Abyss blocks your magic.");
        return;
    }

    int count = 0;
    coord_def f;
    for (adjacent_iterator ai; ai; ++ai)
        if (grd(*ai) == DNGN_FLOOR && find_trap(*ai) && one_chance_in(++count))
            f = *ai;

    if (count > 0)              // found a spot
    {
        if (place_specific_trap(f, TRAP_TELEPORT))
        {
            // Mark it discovered if enough power.
            if (get_power_level(power, rarity) >= 1)
                find_trap(f)->reveal();
        }
    }
}

static void _flight_card(int power, deck_rarity_type rarity)
{
    const int power_level = get_power_level(power, rarity);

    // Assume something _will_ happen.
    bool success = true;

    if (power_level == 0)
    {
        if (!transform(random2(power/4), coinflip() ? TRAN_SPIDER : TRAN_BAT,
                       true))
        {
            // Oops, something went wrong here (either because of cursed
            // equipment or the possibility of stat loss).
            success = false;
        }
    }
    else if (power_level >= 1)
    {
        cast_fly(random2(power/4));
        cast_swiftness(random2(power/4));
    }

    if (power_level == 2) // Stacks with the above.
    {
        if (is_valid_shaft_level() && grd(you.pos()) == DNGN_FLOOR)
        {
            if (place_specific_trap(you.pos(), TRAP_SHAFT))
            {
                find_trap(you.pos())->reveal();
                mpr("A shaft materialises beneath you!");
            }
        }
    }
    if (one_chance_in(4 - power_level))
        potion_effect(POT_INVISIBILITY, random2(power)/4);
    else if (!success)
        canned_msg(MSG_NOTHING_HAPPENS);
}

static void _minefield_card(int power, deck_rarity_type rarity)
{
    const int power_level = get_power_level(power, rarity);
    const int radius = power_level * 2 + 2;
    for (radius_iterator ri(you.pos(), radius, false, false, false); ri; ++ri)
    {
        if ( *ri == you.pos() )
            continue;

        if (grd(*ri) == DNGN_FLOOR && !find_trap(*ri)
            && one_chance_in(4 - power_level))
        {
            if (you.level_type == LEVEL_ABYSS)
                grd(*ri) = coinflip() ? DNGN_DEEP_WATER : DNGN_LAVA;
            else
                place_specific_trap(*ri, TRAP_RANDOM);
        }
    }
}

static int stair_draw_count = 0;

static void _move_stair(coord_def stair_pos, bool away)
{
    ASSERT(stair_pos != you.pos());

    dungeon_feature_type feat = grd(stair_pos);
    ASSERT(grid_stair_direction(feat) != CMD_NO_CMD);

    coord_def begin, towards;

    if (away)
    {
        begin   = you.pos();
        towards = stair_pos;
    }
    else
    {
        // Can't move towards player if it's already adjacent.
        if (adjacent(you.pos(), stair_pos))
            return;

        begin   = stair_pos;
        towards = you.pos();
    }

    ray_def ray;
    if (!find_ray(begin, towards, true, ray, 0, true))
    {
        mpr("Couldn't find ray between player and stairs.", MSGCH_ERROR);
        return;
    }

    // Don't start off under the player.
    if (away)
        ray.advance();

    bool found_stairs = false;
    int  past_stairs  = 0;
    while ( in_bounds(ray.pos()) && see_grid(ray.pos())
            && !grid_is_solid(ray.pos()) && ray.pos() != you.pos() )
    {
        if (ray.pos() == stair_pos)
            found_stairs = true;
        if (found_stairs)
            past_stairs++;
        ray.advance();
    }
    past_stairs--;

    if (!away && grid_is_solid(ray.pos()))
        // Transparent wall between stair and player.
        return;

    if (away && !found_stairs)
    {
        if (grid_is_solid(ray.pos()))
            // Transparent wall between stair and player.
            return;

        mpr("Ray didn't cross stairs.", MSGCH_ERROR);
    }

    if (away && past_stairs <= 0)
        // Stairs already at edge, can't move further away.
        return;

    if ( !in_bounds(ray.pos()) || ray.pos() == you.pos() )
        ray.regress();

    while (!see_grid(ray.pos()) || grd(ray.pos()) != DNGN_FLOOR)
    {
        ray.regress();
        if (!in_bounds(ray.pos()) || ray.pos() == you.pos()
            || ray.pos() == stair_pos)
        {
            // No squares in path are a plain floor.
            return;
        }
    }

    ASSERT(stair_pos != ray.pos());

    std::string stair_str =
        feature_description(stair_pos, false, DESC_CAP_THE, false);

    mprf("%s slides %s you!", stair_str.c_str(),
         away ? "away from" : "towards");

    // Animate stair moving.
    const feature_def &feat_def = get_feature_def(feat);

    bolt beam;

    beam.range   = INFINITE_DISTANCE;
    beam.flavour = BEAM_VISUAL;
    beam.type    = feat_def.symbol;
    beam.colour  = feat_def.colour;
    beam.source  = stair_pos;
    beam.target  = ray.pos();
    beam.name    = "STAIR BEAM";
    beam.draw_delay = 50; // Make beam animation slower than normal.

    beam.aimed_at_spot = true;
    beam.fire();

    // Clear out "missile trails"
    viewwindow(true, false);

    if (!swap_features(stair_pos, ray.pos(), false, false))
        mprf(MSGCH_ERROR, "_move_stair(): failed to move %s",
             stair_str.c_str());
}

static void _stairs_card(int power, deck_rarity_type rarity)
{
    UNUSED(power);
    UNUSED(rarity);

    you.duration[DUR_REPEL_STAIRS_MOVE]  = 0;
    you.duration[DUR_REPEL_STAIRS_CLIMB] = 0;

    if (grid_stair_direction(grd(you.pos())) == CMD_NO_CMD)
        you.duration[DUR_REPEL_STAIRS_MOVE] = 1000;
    else
        you.duration[DUR_REPEL_STAIRS_CLIMB] = 1000;

    std::vector<coord_def> stairs_avail;

    radius_iterator ri(you.pos(), LOS_RADIUS, false, true, true);
    for (; ri; ++ri)
    {
        dungeon_feature_type feat = grd(*ri);
        if (grid_stair_direction(feat) != CMD_NO_CMD
            && feat != DNGN_ENTER_SHOP)
        {
            stairs_avail.push_back(*ri);
        }
    }

    if (stairs_avail.size() == 0)
    {
        mpr("No stairs available to move.");
        return;
    }

    std::random_shuffle(stairs_avail.begin(), stairs_avail.end());

    for (unsigned int i = 0; i < stairs_avail.size(); i++)
        _move_stair(stairs_avail[i], stair_draw_count % 2);

    stair_draw_count++;
}

static int _drain_monsters(coord_def where, int pow, int, actor *)
{
    if (where == you.pos())
        drain_exp();
    else
    {
        const int mnstr = mgrd(where);
        if (mnstr == NON_MONSTER)
            return 0;

        monsters& mon = menv[mnstr];

        if (mons_res_negative_energy(&mon))
            simple_monster_message(&mon, " is unaffected.");
        else
        {
            simple_monster_message(&mon, " is drained.");

            if (x_chance_in_y(pow / 60, 20))
            {
                mon.hit_dice--;
                mon.experience = 0;
            }

            mon.max_hit_points -= 2 + random2(pow/50);
            mon.hurt(&you, 2 + random2(50), BEAM_NEG);
        }
    }
    return 1;
}

static void _mass_drain(int pow)
{
    apply_area_visible(_drain_monsters, pow);
}

// Return true if it was a "genuine" draw, i.e., there was a monster
// to target. This is still exploitable by finding popcorn monsters.
static bool _damaging_card(card_type card, int power, deck_rarity_type rarity)
{
    bool rc = there_are_monsters_nearby(true, false);
    const int power_level = get_power_level(power, rarity);

    dist target;
    zap_type ztype = ZAP_DEBUGGING_RAY;
    const zap_type firezaps[3]   = { ZAP_FLAME, ZAP_STICKY_FLAME, ZAP_FIRE };
    const zap_type frostzaps[3]  = { ZAP_FROST, ZAP_ICE_BOLT, ZAP_COLD };
    const zap_type hammerzaps[3] = { ZAP_STRIKING, ZAP_STONE_ARROW,
                                     ZAP_CRYSTAL_SPEAR };
    const zap_type venomzaps[3]  = { ZAP_STING, ZAP_VENOM_BOLT,
                                     ZAP_POISON_ARROW };
    const zap_type sparkzaps[3]  = { ZAP_ELECTRICITY, ZAP_LIGHTNING,
                                     ZAP_ORB_OF_ELECTRICITY };
    const zap_type painzaps[2]   = { ZAP_AGONY, ZAP_NEGATIVE_ENERGY };

    switch (card)
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

    case CARD_SPARK:
        ztype = sparkzaps[power_level];
        break;

    case CARD_PAIN:
        if (power_level == 2)
        {
            mprf("You have drawn %s.", card_name(card));
            _mass_drain(power);
            return (true);
        }
        else
            ztype = painzaps[power_level];
        break;

    default:
        break;
    }

    snprintf(info, INFO_SIZE, "You have drawn %s.  Aim where? ",
             card_name(card));

    bolt beam;
    beam.range = LOS_RADIUS;
    if (spell_direction(target, beam, DIR_NONE, TARG_ENEMY,
                        LOS_RADIUS, true, true, false, info)
        && player_tracer(ZAP_DEBUGGING_RAY, power/4, beam))
    {
        zapping(ztype, random2(power/4), beam);
    }
    else
        rc = false;

    return (rc);
}

static void _elixir_card(int power, deck_rarity_type rarity)
{
    int power_level = get_power_level(power, rarity);

    if (power_level == 1 && you.hp * 2 > you.hp_max)
        power_level = 0;

    if (power_level == 0)
    {
        if (coinflip())
            potion_effect( POT_HEAL_WOUNDS, 40 ); // power doesn't matter
        else
            cast_regen( random2(power / 4) );
    }
    else if (power_level == 1)
    {
        you.hp = you.hp_max;
        you.magic_points = 0;
    }
    else if (power_level >= 2)
    {
        you.hp = you.hp_max;
        you.magic_points = you.max_magic_points;
    }
    you.redraw_hit_points = true;
    you.redraw_magic_points = true;
}

static void _battle_lust_card(int power, deck_rarity_type rarity)
{
    const int power_level = get_power_level(power, rarity);
    if (power_level >= 2)
    {
        you.duration[DUR_SLAYING] = random2(power/6) + 1;
        mpr("You feel deadly.");
    }
    else if (power_level == 1)
    {
        you.duration[DUR_BUILDING_RAGE] = 1;
        mpr("You feel your rage building.");
    }
    else if (power_level == 0)
        potion_effect(POT_MIGHT, random2(power/4));
}

static void _metamorphosis_card(int power, deck_rarity_type rarity)
{
    const int power_level = get_power_level(power, rarity);
    transformation_type trans;

    if (power_level >= 2)
        trans = coinflip() ? TRAN_DRAGON : TRAN_LICH;
    else if (power_level == 1)
        trans = coinflip() ? TRAN_STATUE : TRAN_BLADE_HANDS;
    else
    {
        trans = one_chance_in(3) ? TRAN_SPIDER :
                coinflip()       ? TRAN_ICE_BEAST
                                 : TRAN_BAT;
    }

    // Might fail, e.g. because of cursed equipment or potential death by
    // stat loss. Aren't we being nice? (jpeg)
    if (!transform(random2(power/4), trans, true))
        canned_msg(MSG_NOTHING_HAPPENS);
}

static void _helm_card(int power, deck_rarity_type rarity)
{
    const int power_level = get_power_level(power, rarity);
    bool do_forescry  = false;
    bool do_stoneskin = false;
    bool do_shield    = false;
    int num_resists = 0;

    // Chances are cumulative.
    if (power_level >= 2)
    {
        if (coinflip()) do_forescry  = true;
        if (coinflip()) do_stoneskin = true;
        if (coinflip()) do_shield    = true;
        num_resists = random2(4);
    }
    if (power_level >= 1)
    {
        if (coinflip()) do_forescry  = true;
        if (coinflip()) do_stoneskin = true;
        if (coinflip()) do_shield    = true;
    }
    if (power_level >= 0)
    {
        if (coinflip())
            do_forescry = true;
        else
            do_stoneskin = true;
    }

    if (do_forescry)
        cast_forescry( random2(power/4) );
    if (do_stoneskin)
        cast_stoneskin( random2(power/4) );
    if (num_resists)
    {
        const duration_type possible_resists[4] = {
            DUR_RESIST_POISON, DUR_INSULATION,
            DUR_RESIST_FIRE, DUR_RESIST_COLD
        };
        const char* resist_names[4] = {
            "poison", "electricity", "fire", "cold"
        };

        for (int i = 0; i < 4 && num_resists; ++i)
        {
            // If there are n left, of which we need to choose
            // k, we have chance k/n of selecting the next item.
            if (x_chance_in_y(num_resists, 4-i))
            {
                // Add a temporary resistance.
                you.duration[possible_resists[i]] += random2(power/7) + 1;
                msg::stream << "You feel resistant to " << resist_names[i]
                            << '.' << std::endl;
                --num_resists;
            }
        }
    }

    if (do_shield)
    {
        if (you.duration[DUR_MAGIC_SHIELD] == 0)
            mpr("A magical shield forms in front of you.");
        you.duration[DUR_MAGIC_SHIELD] += random2(power/6) + 1;
    }
}

static void _blade_card(int power, deck_rarity_type rarity)
{
    // Pause before jumping to the list.
    if (Options.auto_list)
        more();

    wield_weapon(false);

    const int power_level = get_power_level(power, rarity);
    if (power_level >= 2)
    {
        cast_tukimas_dance( random2(power/4) );
    }
    else if (power_level == 1)
    {
        cast_sure_blade( random2(power/4) );
    }
    else
    {
        const brand_type brands[] = {
            SPWPN_FLAMING, SPWPN_FREEZING, SPWPN_VENOM, SPWPN_DRAINING,
            SPWPN_VORPAL, SPWPN_DISTORTION, SPWPN_PAIN, SPWPN_DUMMY_CRUSHING
        };

        if (!brand_weapon(RANDOM_ELEMENT(brands), random2(power/4)))
        {
            if (you.equip[EQ_WEAPON] == -1)
                mprf("Your %s twitch.", your_hand(true).c_str());
            else
                mpr("Your weapon vibrates for a moment.");
        }
    }
}

static void _shadow_card(int power, deck_rarity_type rarity)
{
    const int power_level = get_power_level(power, rarity);

    if (power_level >= 1)
    {
        mpr( you.duration[DUR_STEALTH] ? "You feel more catlike."
                                       : "You feel stealthy.");
        you.duration[DUR_STEALTH] += random2(power/4) + 1;
    }

    potion_effect(POT_INVISIBILITY, random2(power/4));
}

static void _potion_card(int power, deck_rarity_type rarity)
{
    const int power_level = get_power_level(power, rarity);
    potion_type pot_effects[] = {
        POT_HEAL_WOUNDS, POT_HEAL_WOUNDS, POT_HEAL_WOUNDS,
        POT_HEALING, POT_HEALING, POT_HEALING,
        POT_RESTORE_ABILITIES, POT_RESTORE_ABILITIES,
        POT_POISON, POT_CONFUSION, POT_DEGENERATION
    };

    potion_type pot = RANDOM_ELEMENT(pot_effects);

    if (power_level >= 1 && coinflip())
        pot = (coinflip() ? POT_CURE_MUTATION : POT_MUTATION);

    if (power_level >= 2 && one_chance_in(5))
        pot = POT_MAGIC;

    potion_effect(pot, random2(power/4));
}

static void _focus_card(int power, deck_rarity_type rarity)
{
    char* max_statp[]  = { &you.max_strength, &you.max_intel, &you.max_dex };
    char* base_statp[] = { &you.strength, &you.intel, &you.dex };
    int best_stat = 0;
    int worst_stat = 0;

    for (int i = 1; i < 3; ++i)
    {
        const int best_diff = *max_statp[i] - *max_statp[best_stat];
        if (best_diff > 0 || best_diff == 0 && coinflip())
            best_stat = i;

        const int worst_diff = *max_statp[i] - *max_statp[worst_stat];
        if (worst_diff < 0 || worst_diff == 0 && coinflip())
            worst_stat = i;
    }

    while (best_stat == worst_stat)
    {
        best_stat  = random2(3);
        worst_stat = random2(3);
    }

    (*max_statp[best_stat])++;
    (*max_statp[worst_stat])--;
    (*base_statp[best_stat])++;
    (*base_statp[worst_stat])--;

    // Did focusing kill the player?
    const kill_method_type kill_types[3] = {
        KILLED_BY_WEAKNESS,
        KILLED_BY_CLUMSINESS,
        KILLED_BY_STUPIDITY
    };

    std::string cause = "the Focus card";

    if (crawl_state.is_god_acting())
    {
        god_type which_god = crawl_state.which_god_acting();
        if (crawl_state.is_god_retribution())
            cause = "the wrath of " + god_name(which_god);
        else
        {
            if (which_god == GOD_XOM)
                cause = "the capriciousness of Xom";
            else
                cause = "the 'helpfulness' of " + god_name(which_god);
        }
    }

    for (int i = 0; i < 3; ++i)
        if (*max_statp[i] < 1 || *base_statp[i] < 1)
        {
            ouch(INSTANT_DEATH, NON_MONSTER, kill_types[i], cause.c_str(),
                 true);
        }

    // The player survived! Yay!
    you.redraw_strength     = true;
    you.redraw_intelligence = true;
    you.redraw_dexterity    = true;

    burden_change();
}

static void _shuffle_card(int power, deck_rarity_type rarity)
{
    stat_type stats[3] = {STAT_STRENGTH, STAT_DEXTERITY, STAT_INTELLIGENCE};
    int old_base[3]    = {you.strength, you.dex, you.intel};
    int old_max[3]     = {you.max_strength, you.max_dex, you.max_intel};
    int modifiers[3];

    int perm[3] = { 0, 1, 2 };

    for (int i = 0; i < 3; ++i)
        modifiers[i] = stat_modifier(stats[i]);

    std::random_shuffle( perm, perm + 3 );

    int new_base[3];
    int new_max[3];
    for (int i = 0; i < 3; ++i)
    {
        new_base[perm[i]] = old_base[i] - modifiers[i] + modifiers[perm[i]];
        new_max[perm[i]]  = old_max[i] - modifiers[i] + modifiers[perm[i]];
    }

    // Did the shuffling kill the player?
    kill_method_type kill_types[3] = {
        KILLED_BY_WEAKNESS,
        KILLED_BY_CLUMSINESS,
        KILLED_BY_STUPIDITY
    };

    std::string cause = "the Shuffle card";

    if (crawl_state.is_god_acting())
    {
        god_type which_god = crawl_state.which_god_acting();
        if (crawl_state.is_god_retribution())
            cause = "the wrath of " + god_name(which_god);
        else
        {
            if (which_god == GOD_XOM)
                cause = "the capriciousness of Xom";
            else
                cause = "the 'helpfulness' of " + god_name(which_god);
        }
    }

    for (int i = 0; i < 3; ++i)
        if (new_base[i] < 1 || new_max[i] < 1)
        {
            ouch(INSTANT_DEATH, NON_MONSTER, kill_types[i], cause.c_str(),
                 true);
        }

    // The player survived!

    // Sometimes you just long for Python.
    you.strength = new_base[0];
    you.dex      = new_base[1];
    you.intel    = new_base[2];

    you.max_strength = new_max[0];
    you.max_dex      = new_max[1];
    you.max_intel    = new_max[2];

    you.redraw_strength     = true;
    you.redraw_intelligence = true;
    you.redraw_dexterity    = true;
    you.redraw_evasion      = true;

    burden_change();
}

static void _experience_card(int power, deck_rarity_type rarity)
{
    const int power_level = get_power_level(power, rarity);

    if (you.experience_level < 27)
    {
        mpr("You feel more experienced.");
        const unsigned long xp_cap = 1 + exp_needed(2 + you.experience_level);

        // power_level 2 means automatic level gain.
        if (power_level == 2)
            you.experience = xp_cap;
        else
        {
            // Likely to give a level gain (power of ~500 is reasonable
            // at high levels even for non-Nemelexites, so 50,000 XP.)
            // But not guaranteed.
            // Overrides archmagi effect, like potions of experience.
            you.experience += power * 100;
            if (you.experience > xp_cap)
                you.experience = xp_cap;
        }
    }
    else
        mpr("You feel knowledgeable.");

    // Put some free XP into pool; power_level 2 means fill pool
    you.exp_available += power * 50;
    if (power_level >= 2 || you.exp_available > 20000)
        you.exp_available = 20000;

    level_change();
}

static void _remove_bad_mutation()
{
    if (!delete_mutation(RANDOM_BAD_MUTATION, false))
        mpr("You feel transcendent for a moment.");
}

static void _helix_card(int power, deck_rarity_type rarity)
{
    const int power_level = get_power_level(power, rarity);

    if (power_level == 0)
    {
        switch (how_mutated() ? random2(3) : 0)
        {
        case 0:
            mutate(RANDOM_MUTATION);
            break;
        case 1:
            delete_mutation(RANDOM_MUTATION);
            mutate(RANDOM_MUTATION);
            break;
        case 2:
            delete_mutation(RANDOM_MUTATION);
            break;
        }
    }
    else if (power_level == 1)
    {
        switch (how_mutated() ? random2(3) : 0)
        {
        case 0:
            mutate(coinflip() ? RANDOM_GOOD_MUTATION : RANDOM_MUTATION);
            break;
        case 1:
            if (coinflip())
                _remove_bad_mutation();
            else
                delete_mutation(RANDOM_MUTATION);
            break;
        case 2:
            if (coinflip())
            {
                if (coinflip())
                {
                    _remove_bad_mutation();
                    mutate(RANDOM_MUTATION);
                }
                else
                {
                    delete_mutation(RANDOM_MUTATION);
                    mutate(RANDOM_GOOD_MUTATION);
                }
            }
            else
            {
                delete_mutation(RANDOM_MUTATION);
                mutate(RANDOM_MUTATION);
            }
            break;
        }
    }
    else
    {
        switch (random2(3))
        {
        case 0:
            _remove_bad_mutation();
            break;
        case 1:
            mutate(RANDOM_GOOD_MUTATION);
            break;
        case 2:
            if (coinflip())
            {
                // If you get unlucky, you could get here with no bad
                // mutations and simply get a mutation effect. Oh well.
                _remove_bad_mutation();
                mutate(RANDOM_MUTATION);
            }
            else
            {
                delete_mutation(RANDOM_MUTATION);
                mutate(RANDOM_GOOD_MUTATION);
            }
            break;
        }
    }
}

static void _sage_card(int power, deck_rarity_type rarity)
{
    const int power_level = get_power_level(power, rarity);
    int c;                      // how much to weight your skills
    if (power_level == 0)
        c = 0;
    else if (power_level == 1)
        c = random2(10) + 1;
    else
        c = 10;

    // FIXME: yet another reproduction of random_choose_weighted
    // Ah for Python:
    // skill = random_choice([x*(40-x)*c/10 for x in skill_levels])
    int totalweight = 0;
    int result = -1;
    for (int i = 0; i < NUM_SKILLS; ++i )
    {
        if (skill_name(i) == NULL)
            continue;

        if (you.skills[i] < MAX_SKILL_LEVEL)
        {
            // Choosing a skill is likelier if you are somewhat skilled in it.
            const int curweight = 1 + you.skills[i] * (40 - you.skills[i]) * c;
            totalweight += curweight;
            if (x_chance_in_y(curweight, totalweight))
                result = i;
        }
    }

    if (result == -1)
        mpr("You feel omnipotent.");  // All skills maxed.
    else
    {
        you.duration[DUR_SAGE] = random2(1800) + 200;
        you.sage_bonus_skill   = static_cast<skill_type>(result);
        you.sage_bonus_degree  = power / 25;
        mprf(MSGCH_PLAIN, "You feel studious about %s.", skill_name(result));
    }
}

static void _create_pond(const coord_def& center, int radius, bool allow_deep)
{
    for ( radius_iterator ri(center, radius, false); ri; ++ri )
    {
        const coord_def p = *ri;
        if (p != you.pos() && coinflip())
        {
            if (grd(p) == DNGN_FLOOR)
            {
                dungeon_feature_type feat;

                if (allow_deep && coinflip())
                    feat = DNGN_DEEP_WATER;
                else
                    feat = DNGN_SHALLOW_WATER;

                dungeon_terrain_changed(p, feat);
            }
        }
    }
}

static void _deepen_water(const coord_def& center, int radius)
{
    for ( radius_iterator ri(center, radius, false); ri; ++ri )
    {
        // FIXME The iteration shouldn't affect the later squares in the
        // same iteration, i.e., a newly-flooded square shouldn't count
        // in the decision as to whether to make the next square flooded.
        const coord_def p = *ri;
        if (grd(p) == DNGN_SHALLOW_WATER
            && p != you.pos()
            && x_chance_in_y(1+count_neighbours(p.x, p.y, DNGN_DEEP_WATER), 8))
        {
            dungeon_terrain_changed(p, DNGN_DEEP_WATER);
        }
        if (grd(p) == DNGN_FLOOR
            && random2(3) < random2(count_neighbours(p.x,p.y,DNGN_DEEP_WATER)
                               + count_neighbours(p.x,p.y,DNGN_SHALLOW_WATER)))
        {
            dungeon_terrain_changed(p, DNGN_SHALLOW_WATER);
        }
    }
}

static void _water_card(int power, deck_rarity_type rarity)
{
    const int power_level = get_power_level(power, rarity);
    if (power_level == 0)
    {
        mpr("You create a pond!");
        _create_pond(you.pos(), 4, false);
    }
    else if (power_level == 1)
    {
        mpr("You feel the tide rushing in!");
        _create_pond(you.pos(), 6, true);
        for (int i = 0; i < 2; ++i)
            _deepen_water(you.pos(), 6);
    }
    else
    {
        mpr("Water floods your area!");

        // Flood all visible squares.
        for ( radius_iterator ri( you.pos(), LOS_RADIUS, false ); ri; ++ri )
        {
            coord_def p = *ri;
            destroy_trap(p);
            if (grd(p) == DNGN_FLOOR)
            {
                dungeon_feature_type new_feature = DNGN_SHALLOW_WATER;
                if (p != you.pos() && coinflip())
                    new_feature = DNGN_DEEP_WATER;
                dungeon_terrain_changed(p, new_feature);
            }
        }
    }
}

static void _glass_card(int power, deck_rarity_type rarity)
{
    const int power_level = get_power_level(power, rarity);
    const int radius = (power_level == 2) ? 1000
                                          : random2(power/40) + 2;
    vitrify_area(radius);
}

static void _dowsing_card(int power, deck_rarity_type rarity)
{
    const int power_level = get_power_level(power, rarity);
    bool things_to_do[3] = { false, false, false };
    things_to_do[random2(3)] = true;

    if (power_level == 1)
        things_to_do[random2(3)] = true;

    if (power_level >= 2)
    {
        for (int i = 0; i < 3; ++i)
            things_to_do[i] = true;
    }

    if (things_to_do[0])
        cast_detect_secret_doors( random2(power/4) );
    if (things_to_do[1])
        detect_traps( random2(power/4) );
    if (things_to_do[2])
    {
        mpr("You feel telepathic!");
        you.duration[DUR_TELEPATHY] = random2(power/4);
    }
}

static bool _trowel_card(int power, deck_rarity_type rarity)
{
    // Early exit: don't clobber important features.
    if (is_critical_feature(grd(you.pos())))
    {
        mpr("The dungeon trembles momentarily.");
        return (false);
    }

    const int power_level = get_power_level(power, rarity);
    bool done_stuff = false;
    if (power_level >= 2)
    {
        // Generate a portal to something.
        const map_def *map = random_map_for_tag("trowel_portal", false);
        if (!map)
        {
            mpr("A buggy portal flickers into view, then vanishes.");
        }
        else
        {
            {
                no_messages n;
                dgn_place_map(map, true, true, you.pos());
            }
            mpr("A mystic portal forms.");
        }
        done_stuff = true;
    }
    else if (power_level == 1)
    {
        if (coinflip())
        {
            // Create a random bad statue and a friendly, timed golem.
            // This could be really bad, because they're placed adjacent
            // to you...
            int num_made = 0;

            const monster_type statues[] = {
                MONS_ORANGE_STATUE, MONS_SILVER_STATUE, MONS_ICE_STATUE
            };

            if (create_monster(
                    mgen_data::hostile_at(
                        RANDOM_ELEMENT(statues),
                        you.pos(), 0, 0, true)) != -1)
            {
                mpr("A menacing statue appears!");
                num_made++;
            }

            const monster_type golems[] = {
                MONS_CLAY_GOLEM, MONS_WOOD_GOLEM, MONS_STONE_GOLEM,
                MONS_IRON_GOLEM, MONS_CRYSTAL_GOLEM, MONS_TOENAIL_GOLEM
            };

            if (create_monster(
                    mgen_data(RANDOM_ELEMENT(golems),
                              BEH_FRIENDLY, 5, 0,
                              you.pos(), you.pet_target)) != -1)
            {
                mpr("You construct a golem!");
                num_made++;
            }

            if (num_made == 2)
                mpr("The constructs glare at each other.");

            done_stuff = (num_made > 0);
        }
        else
        {
            // Do-nothing (effectively): create a cosmetic feature
            const coord_def pos = pick_adjacent_free_square(you.pos());
            if (pos.x >= 0 && pos.y >= 0)
            {
                const dungeon_feature_type statfeat[] = {
                    DNGN_GRANITE_STATUE, DNGN_ORCISH_IDOL
                };
                // We leave the items on the square
                grd(pos) = RANDOM_ELEMENT(statfeat);
                mpr("A statue takes form beside you.");
                done_stuff = true;
            }
        }
    }
    else
    {
        // Generate an altar.
        if (grd(you.pos()) == DNGN_FLOOR)
        {
            // Might get GOD_NO_GOD and no altar.
            god_type rgod = static_cast<god_type>(random2(NUM_GODS));
            grd(you.pos()) = altar_for_god(rgod);

            if (grd(you.pos()) != DNGN_FLOOR)
            {
                done_stuff = true;
                mprf("An altar to %s grows from the floor before you!",
                     god_name(rgod).c_str());
            }
        }
    }

    if (!done_stuff)
        canned_msg(MSG_NOTHING_HAPPENS);

    return (done_stuff);
}

static void _genie_card(int power, deck_rarity_type rarity)
{
    if (coinflip())
    {
        mpr("A genie takes form and thunders: "
            "\"Choose your reward, mortal!\"");
        more();
        acquirement( OBJ_RANDOM, AQ_CARD_GENIE );
    }
    else
    {
        mpr("A genie takes form and thunders: "
            "\"You disturbed me, fool!\"");
        // Use 41, not 40, to tell potion_effect() that this isn't a
        // real potion.
        potion_effect( coinflip() ? POT_DEGENERATION : POT_DECAY, 41 );
    }
}

// Special case for *your* god, maybe?
static void _godly_wrath()
{
    int tries = 100;
    while (tries-- > 0)
    {
        god_type god = static_cast<god_type>(random2(NUM_GODS - 1) + 1);

        // Don't recursively make player draw from the Deck of Punishment.
        if (god == GOD_NEMELEX_XOBEH)
            continue;

        // Stop once we find a god willing to punish the player.
        if (divine_retribution(god))
            break;
    }

    if (tries <= 0)
        mpr("You somehow manage to escape divine attention...");
}

static void _curse_card(int power, deck_rarity_type rarity)
{
    const int power_level = get_power_level(power, rarity);

    mpr("You feel a malignant aura surround you.");
    if (power_level >= 2)
    {
        // Curse (almost) everything + chance of decay.
        while ( curse_an_item(one_chance_in(6), true) && !one_chance_in(1000) )
            ;
    }
    else if (power_level == 1)
    {
        // Curse an average of four items.
        do
        {
            curse_an_item(false);
        }
        while ( !one_chance_in(4) );
    }
    else
    {
        // Curse 1.5 items on average.
        curse_an_item(false);
        if (coinflip())
            curse_an_item(false);
    }
}

static void _crusade_card(int power, deck_rarity_type rarity)
{
    const int power_level = get_power_level(power, rarity);
    if (power_level >= 1)
    {
        // A chance to convert opponents.
        for (int i = 0; i < MAX_MONSTERS; ++i)
        {
            monsters* const monster = &menv[i];
            if (monster->type == -1 || !mons_near(monster)
                || mons_friendly(monster)
                || mons_holiness(monster) != MH_NATURAL
                || mons_is_unique(monster->type)
                || mons_immune_magic(monster)
                || player_will_anger_monster(monster))
            {
                continue;
            }

            // Note that this bypasses the magic resistance
            // (though not immunity) check.  Specifically,
            // you can convert Killer Klowns this way.
            // Might be too good.
            if (monster->hit_dice * 35 < random2(power))
            {
                simple_monster_message(monster, " is converted.");

                if (one_chance_in(5 - power_level))
                {
                    monster->attitude = ATT_FRIENDLY;

                    // If you worship a god that lets you recruit
                    // permanent followers, or a god allied with one,
                    // count this as a recruitment.
                    if (is_good_god(you.religion)
                        || you.religion == GOD_BEOGH
                            && mons_species(monster->type) == MONS_ORC
                            && !mons_is_summoned(monster)
                            && !mons_is_shapeshifter(monster))
                    {
                        mons_make_god_gift(monster, is_good_god(you.religion) ?
                                           GOD_SHINING_ONE : GOD_BEOGH);
                    }
                }
                else
                    monster->add_ench(ENCH_CHARM);
            }
        }
    }
    abjuration(power/4);
}

static void _summon_demon_card(int power, deck_rarity_type rarity)
{
    const int power_level = get_power_level(power, rarity);
    demon_class_type dct;
    if (power_level >= 2)
        dct = DEMON_GREATER;
    else if (power_level == 1)
        dct = DEMON_COMMON;
    else
        dct = DEMON_LESSER;

    create_monster(
        mgen_data(summon_any_demon(dct), BEH_FRIENDLY,
                  std::min(power / 50, 6), 0,
                  you.pos(), you.pet_target));
}

static void _summon_any_monster(int power, deck_rarity_type rarity)
{
    const int power_level = get_power_level(power, rarity);
    monster_type mon_chosen = NUM_MONSTERS;
    coord_def chosen_spot;
    int num_tries;

    if (power_level == 0)
        num_tries = 1;
    else if (power_level == 1)
        num_tries = 4;
    else
        num_tries = 18;

    for (int i = 0; i < num_tries; ++i)
    {
        int dx, dy;
        do
        {
            dx = random2(3) - 1;
            dy = random2(3) - 1;
        }
        while (dx == 0 && dy == 0);

        coord_def delta(dx,dy);

        monster_type cur_try;
        do
        {
            cur_try = random_monster_at_grid(you.pos() + delta);
        }
        while (mons_is_unique(cur_try));

        if (mon_chosen == NUM_MONSTERS
            || mons_power(mon_chosen) < mons_power(cur_try))
        {
            mon_chosen = cur_try;
            chosen_spot = you.pos();
        }
    }

    if (mon_chosen == NUM_MONSTERS) // Should never happen.
        return;

    const bool friendly = (power_level > 0 || !one_chance_in(4));

    create_monster(
        mgen_data(mon_chosen,
                  friendly ? BEH_FRIENDLY : BEH_HOSTILE,
                  3, 0,
                  chosen_spot,
                  friendly ? you.pet_target : MHITYOU));
}

static void _summon_dancing_weapon(int power, deck_rarity_type rarity)
{
    const int power_level = get_power_level(power, rarity);
    const bool friendly   = (power_level > 0 || !one_chance_in(4));

    const int mon =
        create_monster(
            mgen_data(MONS_DANCING_WEAPON,
                      friendly ? BEH_FRIENDLY : BEH_HOSTILE,
                      power_level + 3, 0, you.pos(),
                      friendly ? you.pet_target : MHITYOU));

    // Given the abundance of Nemelex decks, not setting hard reset
    // leaves a trail of weapons behind, most of which just get
    // offered to Nemelex again, adding an unnecessary source of
    // piety.
    if (mon != -1)
    {
        // Override the weapon
        ASSERT( menv[mon].weapon() != NULL );
        item_def& wpn(*menv[mon].weapon());

        // FIXME Mega-hack (breaks encapsulation too)
        wpn.flags &= ~ISFLAG_RACIAL_MASK;

        if (power_level == 0)
        {
            // Wimpy, negative-enchantment weapon.
            wpn.plus  = -random2(4);
            wpn.plus2 = -random2(4);
            wpn.sub_type = (coinflip() ? WPN_DAGGER : WPN_CLUB );
            set_item_ego_type(wpn, OBJ_WEAPONS, SPWPN_NORMAL);
        }
        else if (power_level == 1)
        {
            // This is getting good...
            wpn.plus  = random2(4) - 1;
            wpn.plus2 = random2(4) - 1;
            wpn.sub_type = (coinflip() ? WPN_LONG_SWORD : WPN_HAND_AXE);
            if (coinflip())
            {
                set_item_ego_type(wpn, OBJ_WEAPONS,
                                  coinflip() ? SPWPN_FLAMING : SPWPN_FREEZING);
            }
        }
        else if (power_level == 2)
        {
            // Rare and powerful.
            wpn.plus  = random2(4) + 2;
            wpn.plus2 = random2(4) + 2;
            wpn.sub_type = (coinflip() ? WPN_KATANA : WPN_EXECUTIONERS_AXE);
            set_item_ego_type(wpn, OBJ_WEAPONS,
                              coinflip() ? SPWPN_SPEED : SPWPN_ELECTROCUTION);
        }
        menv[mon].flags |= MF_HARD_RESET;
    }
}

static void _summon_flying(int power, deck_rarity_type rarity)
{
    const int power_level = get_power_level(power, rarity);
    const bool friendly = (power_level > 0 || !one_chance_in(4));

    const monster_type flytypes[] = {
        MONS_BUTTERFLY, MONS_BUMBLEBEE, MONS_INSUBSTANTIAL_WISP,
        MONS_VAPOUR, MONS_YELLOW_WASP, MONS_RED_WASP
    };

    // Choose what kind of monster.
    // Be nice and don't summon friendly invisibles.
    monster_type result = MONS_PROGRAM_BUG;
    do
        result = flytypes[random2(4) + power_level];
    while (friendly && mons_class_flag(result, M_INVIS)
           && !player_see_invis());

    for (int i = 0; i < power_level * 5 + 2; ++i)
    {
        create_monster(
            mgen_data(result,
                      friendly ? BEH_FRIENDLY : BEH_HOSTILE,
                      std::min(power / 50, 6), 0,
                      you.pos(),
                      friendly ? you.pet_target : MHITYOU));
    }
}

static void _summon_skeleton(int power, deck_rarity_type rarity)
{
    const int power_level = get_power_level(power, rarity);
    const bool friendly = (power_level > 0 || !one_chance_in(4));
    const monster_type skeltypes[] = {
        MONS_SKELETON_LARGE, MONS_SKELETAL_WARRIOR, MONS_SKELETAL_DRAGON
    };

    create_monster(
        mgen_data(
            skeltypes[power_level],
            friendly ? BEH_FRIENDLY : BEH_HOSTILE,
            std::min(power / 50, 6), 0,
            you.pos(),
            friendly ? you.pet_target : MHITYOU));
}

static void _summon_ugly(int power, deck_rarity_type rarity)
{
    const int power_level = get_power_level(power, rarity);
    const bool friendly = (power_level > 0 || !one_chance_in(4));
    monster_type ugly;
    if (power_level >= 2)
        ugly = MONS_VERY_UGLY_THING;
    else if (power_level == 1)
        ugly = coinflip() ? MONS_VERY_UGLY_THING : MONS_UGLY_THING;
    else
        ugly = MONS_UGLY_THING;

    create_monster(
        mgen_data(ugly,
                  friendly ? BEH_FRIENDLY : BEH_HOSTILE,
                  std::min(power / 50, 6), 0,
                  you.pos(),
                  friendly ? you.pet_target : MHITYOU));
}

static int _card_power(deck_rarity_type rarity)
{
    int result = 0;

    if (you.penance[GOD_NEMELEX_XOBEH])
    {
        result -= you.penance[GOD_NEMELEX_XOBEH];
    }
    else if (you.religion == GOD_NEMELEX_XOBEH)
    {
        result = you.piety;
        result *= (you.skills[SK_EVOCATIONS] + 25);
        result /= 27;
    }

    result += you.skills[SK_EVOCATIONS] * 9;
    if (rarity == DECK_RARITY_RARE)
        result += 150;
    else if (rarity == DECK_RARITY_LEGENDARY)
        result += 300;

    return (result);
}

bool card_effect(card_type which_card, deck_rarity_type rarity,
                 unsigned char flags, bool tell_card)
{
    bool rc = true;
    const int power = _card_power(rarity);

    const god_type god =
        (crawl_state.is_god_acting()) ? crawl_state.which_god_acting()
                                      : GOD_NO_GOD;

#ifdef DEBUG_DIAGNOSTICS
    msg::streams(MSGCH_DIAGNOSTICS) << "Card power: " << power
                                    << ", rarity: " << static_cast<int>(rarity)
                                    << std::endl;
#endif

    if (tell_card)
    {
        // These card types will usually give this message in the targetting
        // prompt, and the cases where they don't are handled specially.
        if (which_card != CARD_VITRIOL && which_card != CARD_FLAME
            && which_card != CARD_FROST && which_card != CARD_HAMMER
            && which_card != CARD_SPARK && which_card != CARD_PAIN
            && which_card != CARD_VENOM)
        {
           mprf("You have drawn %s.", card_name(which_card));
        }
    }

    if (which_card == CARD_XOM && !crawl_state.is_god_acting())
    {
        if (you.religion == GOD_XOM)
        {
            // Being a self-centered deity, Xom *always* finds this
            // maximally hilarious.
            god_speaks(GOD_XOM, "Xom roars with laughter!");
            you.gift_timeout = 255;
        }
        else if (you.penance[GOD_XOM])
            god_speaks(GOD_XOM, "Xom laughs nastily.");
    }

    switch (which_card)
    {
    case CARD_PORTAL:           _portal_card(power, rarity); break;
    case CARD_WARP:             _warp_card(power, rarity); break;
    case CARD_SWAP:             _swap_monster_card(power, rarity); break;
    case CARD_VELOCITY:         _velocity_card(power, rarity); break;
    case CARD_DAMNATION:        _damnation_card(power, rarity); break;
    case CARD_SOLITUDE:         cast_dispersal(power/4); break;
    case CARD_ELIXIR:           _elixir_card(power, rarity); break;
    case CARD_BATTLELUST:       _battle_lust_card(power, rarity); break;
    case CARD_METAMORPHOSIS:    _metamorphosis_card(power, rarity); break;
    case CARD_HELM:             _helm_card(power, rarity); break;
    case CARD_BLADE:            _blade_card(power, rarity); break;
    case CARD_SHADOW:           _shadow_card(power, rarity); break;
    case CARD_POTION:           _potion_card(power, rarity); break;
    case CARD_FOCUS:            _focus_card(power, rarity); break;
    case CARD_SHUFFLE:          _shuffle_card(power, rarity); break;
    case CARD_EXPERIENCE:       _experience_card(power, rarity); break;
    case CARD_HELIX:            _helix_card(power, rarity); break;
    case CARD_SAGE:             _sage_card(power, rarity); break;
    case CARD_WATER:            _water_card(power, rarity); break;
    case CARD_GLASS:            _glass_card(power, rarity); break;
    case CARD_DOWSING:          _dowsing_card(power, rarity); break;
    case CARD_MINEFIELD:        _minefield_card(power, rarity); break;
    case CARD_STAIRS:           _stairs_card(power, rarity); break;
    case CARD_GENIE:            _genie_card(power, rarity); break;
    case CARD_CURSE:            _curse_card(power, rarity); break;
    case CARD_WARPWRIGHT:       _warpwright_card(power, rarity); break;
    case CARD_FLIGHT:           _flight_card(power, rarity); break;
    case CARD_TOMB:             entomb(power); break;
    case CARD_WRAITH:           drain_exp(false); lose_level(); break;
    case CARD_WRATH:            _godly_wrath(); break;
    case CARD_CRUSADE:          _crusade_card(power, rarity); break;
    case CARD_SUMMON_DEMON:     _summon_demon_card(power, rarity); break;
    case CARD_SUMMON_ANIMAL:    summon_animals(random2(power/3)); break;
    case CARD_SUMMON_ANY:       _summon_any_monster(power, rarity); break;
    case CARD_SUMMON_WEAPON:    _summon_dancing_weapon(power, rarity); break;
    case CARD_SUMMON_FLYING:    _summon_flying(power, rarity); break;
    case CARD_SUMMON_SKELETON:  _summon_skeleton(power, rarity); break;
    case CARD_SUMMON_UGLY:      _summon_ugly(power, rarity); break;
    case CARD_XOM:              xom_acts(5 + random2(power/10)); break;
    case CARD_TROWEL:      rc = _trowel_card(power, rarity); break;
    case CARD_SPADE:   your_spells(SPELL_DIG, random2(power/4), false); break;
    case CARD_BANSHEE: mass_enchantment(ENCH_FEAR, power, MHITYOU); break;
    case CARD_TORMENT: torment(TORMENT_CARDS, you.pos()); break;

    case CARD_VENOM:
        if (coinflip())
        {
            mprf("You have drawn %s.", card_name(which_card));
            your_spells(SPELL_OLGREBS_TOXIC_RADIANCE, random2(power/4), false);
        }
        else
            rc = _damaging_card(which_card, power, rarity);
        break;

    case CARD_VITRIOL:
    case CARD_FLAME:
    case CARD_FROST:
    case CARD_HAMMER:
    case CARD_SPARK:
    case CARD_PAIN:
        rc = _damaging_card(which_card, power, rarity);
        break;

    case CARD_BARGAIN:
        you.duration[DUR_BARGAIN] += random2(power) + random2(power) + 2;
        break;

    case CARD_MAP:
        if (!magic_mapping( random2(power/10) + 15, random2(power), true))
            mpr("The map is blank.");
        break;

    case CARD_WILD_MAGIC:
        // Yes, high power is bad here.
        MiscastEffect( &you, god == GOD_NO_GOD ? NON_MONSTER : -god,
                       SPTYP_RANDOM, random2(power/15) + 5, random2(power),
                       "a card of wild magic" );
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
        // The compiler will complain if any card remains unhandled.
        mpr("You have drawn a buggy card!");
        break;
    }

    if (you.religion == GOD_XOM && !rc)
    {
        god_speaks(GOD_XOM, "\"How boring, let's spice things up a little.\"");
        xom_acts(abs(you.piety - 100));
    }

    if (you.religion == GOD_NEMELEX_XOBEH && !rc)
        simple_god_message(" seems disappointed in you.");

    return rc;
}

bool top_card_is_known(const item_def &deck)
{
    if (!is_deck(deck))
        return (false);

    unsigned char flags;
    get_card_and_flags(deck, -1, flags);

    return (flags & CFLAG_MARKED);
}

card_type top_card(const item_def &deck)
{
    if (!is_deck(deck))
        return (NUM_CARDS);

    unsigned char flags;
    card_type card = get_card_and_flags(deck, -1, flags);

    UNUSED(flags);

    return (card);
}

bool is_deck(const item_def &item)
{
    return (item.base_type == OBJ_MISCELLANY
            && item.sub_type >= MISC_DECK_OF_ESCAPE
            && item.sub_type <= MISC_DECK_OF_DEFENCE);
}

bool bad_deck(const item_def &item)
{
    if (!is_deck(item))
        return (false);

    return (!item.props.exists("cards")
            || item.props["cards"].get_type() != SV_VEC
            || item.props["cards"].get_vector().get_type() != SV_BYTE
            || cards_in_deck(item) == 0);
}

deck_rarity_type deck_rarity(const item_def &item)
{
    ASSERT( is_deck(item) );

    return static_cast<deck_rarity_type>(item.special);
}

unsigned char deck_rarity_to_color(deck_rarity_type rarity)
{
    switch (rarity)
    {
    case DECK_RARITY_COMMON:
    {
        const unsigned char colours[] = {LIGHTBLUE, GREEN, CYAN, RED};
        return RANDOM_ELEMENT(colours);
    }

    case DECK_RARITY_RARE:
        return (coinflip() ? MAGENTA : BROWN);

    case DECK_RARITY_LEGENDARY:
        return LIGHTMAGENTA;
    }

    return (WHITE);
}

void init_deck(item_def &item)
{
    CrawlHashTable &props = item.props;

    ASSERT(is_deck(item));
    ASSERT(!props.exists("cards"));
    ASSERT(item.plus > 0);
    ASSERT(item.plus <= 127);
    ASSERT(item.special >= DECK_RARITY_COMMON
           && item.special <= DECK_RARITY_LEGENDARY);

    props.set_default_flags(SFLAG_CONST_TYPE);

    props["cards"].new_vector(SV_BYTE).resize((vec_size)item.plus);
    props["card_flags"].new_vector(SV_BYTE).resize((vec_size)item.plus);
    props["drawn_cards"].new_vector(SV_BYTE);

    for (int i = 0; i < item.plus; i++)
    {
        bool      was_odd = false;
        card_type card    = _random_card(item, was_odd);

        unsigned char flags = 0;
        if (was_odd)
            flags = CFLAG_ODDITY;

        _set_card_and_flags(item, i, card, flags);
    }

    ASSERT(cards_in_deck(item) == item.plus);

    props["num_marked"]        = (char) 0;
    props["non_brownie_draws"] = (char) 0;

    props.assert_validity();

    item.plus2  = 0;
    item.colour = deck_rarity_to_color((deck_rarity_type) item.special);
}

static void _unmark_deck(item_def& deck)
{
    if (!is_deck(deck))
        return;

    CrawlHashTable &props = deck.props;
    if (!props.exists("card_flags"))
        return;

    CrawlVector &flags = props["card_flags"];

    for (unsigned int i = 0; i < flags.size(); ++i)
    {
        flags[i] =
            static_cast<char>((static_cast<char>(flags[i]) & ~CFLAG_MARKED));
    }

    // We'll be mean and leave non_brownie_draws as-is.
    props["num_marked"] = static_cast<char>(0);
}

static void _unmark_and_shuffle_deck(item_def& deck)
{
    if (is_deck(deck))
    {
        _unmark_deck(deck);
        _shuffle_deck(deck);
    }
}

static bool _shuffle_all_decks_on_level()
{
    bool success = false;

    for (int i = 0; i < MAX_ITEMS; ++i)
    {
        item_def& item(mitm[i]);
        if (is_valid_item(item) && is_deck(item))
        {
#ifdef DEBUG_DIAGNOSTICS
            mprf(MSGCH_DIAGNOSTICS, "Shuffling: %s on level %d, branch %d",
                 item.name(DESC_PLAIN).c_str(),
                 static_cast<int>(you.your_level),
                 static_cast<int>(you.where_are_you));
#endif
            _unmark_and_shuffle_deck(item);

            success = true;
        }
    }

    return success;
}

static bool _shuffle_inventory_decks()
{
    bool success = false;

    for (int i = 0; i < ENDOFPACK; ++i)
    {
        item_def& item(you.inv[i]);
        if (is_valid_item(item) && is_deck(item))
        {
#ifdef DEBUG_DIAGNOSTICS
            mprf(MSGCH_DIAGNOSTICS, "Shuffling in inventory: %s",
                 item.name(DESC_PLAIN).c_str());
#endif
            _unmark_and_shuffle_deck(item);

            success = true;
        }
    }

    return success;
}

void nemelex_shuffle_decks()
{
    bool success = false;

    if (apply_to_all_dungeons(_shuffle_all_decks_on_level))
        success = true;

    if (_shuffle_inventory_decks())
        success = true;

    if (success)
        god_speaks(GOD_NEMELEX_XOBEH, "You hear Nemelex chuckle.");
}
