/**
 * @file
 * @brief Functions with decks of cards.
**/

#include "AppHdr.h"

#include "decks.h"

#include <algorithm>
#include <iostream>
#include <sstream>

#include "act-iter.h"
#include "artefact.h"
#include "attitude-change.h"
#include "cloud.h"
#include "coordit.h"
#include "dactions.h"
#include "database.h"
#include "directn.h"
#include "dungeon.h"
#include "effects.h"
#include "english.h"
#include "evoke.h"
#include "food.h"
#include "ghost.h"
#include "godwrath.h"
#include "invent.h"
#include "itemprop.h"
#include "items.h"
#include "item_use.h"
#include "libutil.h"
#include "macro.h"
#include "message.h"
#include "misc.h"
#include "mon-cast.h"
#include "mon-clone.h"
#include "mon-death.h"
#include "mon-place.h"
#include "mon-poly.h"
#include "mon-project.h"
#include "mutation.h"
#include "notes.h"
#include "output.h"
#include "player-equip.h"
#include "player-stats.h"
#include "potion.h"
#include "prompt.h"
#include "religion.h"
#include "spl-clouds.h"
#include "spl-goditem.h"
#include "spl-miscast.h"
#include "spl-monench.h"
#include "spl-other.h"
#include "spl-selfench.h"
#include "spl-summoning.h"
#include "spl-transloc.h"
#include "spl-wpnench.h"
#include "state.h"
#include "stringutil.h"
#include "teleport.h"
#include "terrain.h"
#include "transform.h"
#include "traps.h"
#include "uncancel.h"
#include "view.h"
#include "xom.h"

// DECK STRUCTURE: deck.initial_cards is the number of cards the deck *started*
// with, deck.used_count is* the number of cards drawn, deck.rarity is the
// deck rarity, deck.props["cards"] holds the list of cards (with the
// highest index card being the top card, and index 0 being the bottom
// card), deck.props["drawn_cards"] holds the list of drawn cards
// (with index 0 being the first drawn), deck.props["card_flags"]
// holds the flags for each card, deck.props["num_marked"] is the
// number of marked cards left in the deck.
//
// if deck.used_count is negative, it's actually -(cards_left). wtf.
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

const deck_archetype deck_of_transport[] =
{
    { CARD_WARPWRIGHT, {5, 5, 5} },
    { CARD_SWAP,       {5, 5, 5} },
    { CARD_VELOCITY,   {5, 5, 5} },
    { CARD_SOLITUDE,   {5, 5, 5} },
    END_OF_DECK
};

const deck_archetype deck_of_emergency[] =
{
    { CARD_TOMB,       {5, 5, 5} },
    { CARD_BANSHEE,    {5, 5, 5} },
    { CARD_DAMNATION,  {0, 1, 2} },
    { CARD_SHAFT,      {5, 5, 5} },
    { CARD_ALCHEMIST,  {5, 5, 5} },
    { CARD_ELIXIR,     {5, 5, 5} },
    { CARD_CLOUD,      {5, 5, 5} },
    END_OF_DECK
};

const deck_archetype deck_of_destruction[] =
{
    { CARD_VITRIOL,  {5, 5, 5} },
    { CARD_HAMMER,   {5, 5, 5} },
    { CARD_VENOM,    {5, 5, 5} },
    { CARD_STORM,    {5, 5, 5} },
    { CARD_PAIN,     {5, 5, 3} },
    { CARD_ORB,      {5, 5, 5} },
    { CARD_DEGEN,    {5, 5, 5} },
    END_OF_DECK
};

const deck_archetype deck_of_battle[] =
{
    { CARD_ELIXIR,        {5, 5, 5} },
    { CARD_POTION,        {5, 5, 5} },
    { CARD_HELM,          {5, 5, 5} },
    { CARD_BLADE,         {5, 5, 5} },
    { CARD_SHADOW,        {5, 5, 5} },
    { CARD_FORTITUDE,     {5, 5, 5} },
    END_OF_DECK
};

const deck_archetype deck_of_summoning[] =
{
    { CARD_CRUSADE,         {5, 5, 5} },
    { CARD_ELEMENTS,        {5, 5, 5} },
    { CARD_SUMMON_DEMON,    {5, 5, 5} },
    { CARD_SUMMON_WEAPON,   {5, 5, 5} },
    { CARD_SUMMON_FLYING,   {5, 5, 5} },
    { CARD_RANGERS,         {5, 5, 5} },
    { CARD_SUMMON_UGLY,     {5, 5, 5} },
    { CARD_ILLUSION,        {5, 5, 5} },
    END_OF_DECK
};

const deck_archetype deck_of_wonders[] =
{
    { CARD_FOCUS,        {3, 3, 3} },
    { CARD_HELIX,        {3, 4, 5} },
    { CARD_SHAFT,        {5, 5, 5} },
    { CARD_DOWSING,      {5, 5, 5} },
    { CARD_MERCENARY,    {5, 5, 5} },
    { CARD_ALCHEMIST,    {5, 5, 5} },
    { CARD_PLACID_MAGIC, {5, 5, 5} },
    END_OF_DECK
};

#if TAG_MAJOR_VERSION == 34
const deck_archetype deck_of_dungeons[] =
{
    { CARD_WATER,     {5, 5, 5} },
    { CARD_GLASS,     {5, 5, 5} },
    { CARD_DOWSING,   {5, 5, 5} },
    { CARD_TROWEL,    {0, 0, 3} },
    { CARD_MINEFIELD, {5, 5, 5} },
    END_OF_DECK
};
#endif

const deck_archetype deck_of_oddities[] =
{
    { CARD_WRATH,   {5, 5, 5} },
    { CARD_XOM,     {5, 5, 5} },
    { CARD_FEAST,   {5, 5, 5} },
    { CARD_FAMINE,  {5, 5, 5} },
    { CARD_CURSE,   {5, 5, 5} },
    { CARD_HELIX,   {5, 5, 5} },
    { CARD_FOCUS,   {5, 5, 5} },
    END_OF_DECK
};

const deck_archetype deck_of_punishment[] =
{
    { CARD_WRAITH,     {5, 5, 5} },
    { CARD_WILD_MAGIC, {5, 5, 5} },
    { CARD_WRATH,      {5, 5, 5} },
    { CARD_XOM,        {5, 5, 5} },
    { CARD_FAMINE,     {5, 5, 5} },
    { CARD_CURSE,      {5, 5, 5} },
    { CARD_DAMNATION,  {3, 3, 3} },
    { CARD_SWINE,      {5, 5, 5} },
    { CARD_TORMENT,    {5, 5, 5} },
    END_OF_DECK
};

static void _check_odd_card(uint8_t flags)
{
    if ((flags & CFLAG_ODDITY) && !(flags & CFLAG_SEEN))
        mpr("This card doesn't seem to belong here.");
}

static bool _card_forbidden(card_type card)
{
    if (crawl_state.game_is_zotdef())
    {
        switch (card)
        {
        case CARD_TOMB:
        case CARD_WARPWRIGHT:
        case CARD_STAIRS:
            return true;
        default:
            break;
        }
    }
    return false;
}

int cards_in_deck(const item_def &deck)
{
    ASSERT(is_deck(deck));

    const CrawlHashTable &props = deck.props;
    ASSERT(props.exists("cards"));

    return props["cards"].get_vector().size();
}

static void _shuffle_deck(item_def &deck)
{
    ASSERT(is_deck(deck));

    CrawlHashTable &props = deck.props;
    ASSERT(props.exists("cards"));

    CrawlVector &cards = props["cards"].get_vector();

    CrawlVector &flags = props["card_flags"].get_vector();
    ASSERT(flags.size() == cards.size());

    // Don't use shuffle(), since we want to apply exactly the
    // same shuffling to both the cards vector and the flags vector.
    vector<vec_size> pos;
    for (size_t i = 0; i < cards.size(); ++i)
        pos.push_back(random2(cards.size()));

    for (vec_size i = 0; i < pos.size(); ++i)
    {
        swap(cards[i], cards[pos[i]]);
        swap(flags[i], flags[pos[i]]);
    }
}

card_type get_card_and_flags(const item_def& deck, int idx,
                             uint8_t& _flags)
{
    const CrawlHashTable &props = deck.props;
    const CrawlVector    &cards = props["cards"].get_vector();
    const CrawlVector    &flags = props["card_flags"].get_vector();

    // Negative idx means read from the end.
    if (idx < 0)
        idx += static_cast<int>(cards.size());

    _flags = (uint8_t) flags[idx].get_byte();

    return static_cast<card_type>(cards[idx].get_byte());
}

static void _set_card_and_flags(item_def& deck, int idx, card_type card,
                                uint8_t _flags)
{
    CrawlHashTable &props = deck.props;
    CrawlVector    &cards = props["cards"].get_vector();
    CrawlVector    &flags = props["card_flags"].get_vector();

    if (idx < 0)
        idx = static_cast<int>(cards.size()) + idx;

    cards[idx].get_byte() = card;
    flags[idx].get_byte() = _flags;
}

const char* card_name(card_type card)
{
    switch (card)
    {
#if TAG_MAJOR_VERSION == 34
    case CARD_PORTAL:          return "the Portal";
    case CARD_WARP:            return "the Warp";
    case CARD_BATTLELUST:      return "Battlelust";
    case CARD_METAMORPHOSIS:   return "Metamorphosis";
    case CARD_SHUFFLE:         return "Shuffle";
    case CARD_EXPERIENCE:      return "Experience";
    case CARD_SAGE:            return "the Sage";
    case CARD_TROWEL:          return "the Trowel";
    case CARD_MINEFIELD:       return "the Minefield";
    case CARD_GENIE:           return "the Genie";
    case CARD_WATER:           return "Water";
    case CARD_GLASS:           return "Vitrification";
    case CARD_BARGAIN:         return "the Bargain";
    case CARD_SUMMON_ANIMAL:   return "the Herd";
    case CARD_SUMMON_SKELETON: return "the Bones";
#endif
    case CARD_SWAP:            return "Swap";
    case CARD_VELOCITY:        return "Velocity";
    case CARD_DAMNATION:       return "Damnation";
    case CARD_SOLITUDE:        return "Solitude";
    case CARD_ELIXIR:          return "the Elixir";
    case CARD_HELM:            return "the Helm";
    case CARD_BLADE:           return "the Blade";
    case CARD_SHADOW:          return "the Shadow";
    case CARD_POTION:          return "the Potion";
    case CARD_FOCUS:           return "Focus";
    case CARD_HELIX:           return "the Helix";
    case CARD_DOWSING:         return "Dowsing";
    case CARD_STAIRS:          return "the Stairs";
    case CARD_TOMB:            return "the Tomb";
    case CARD_BANSHEE:         return "the Banshee";
    case CARD_WILD_MAGIC:      return "Wild Magic";
    case CARD_PLACID_MAGIC:    return "Placid Magic";
    case CARD_CRUSADE:         return "the Crusade";
    case CARD_ELEMENTS:        return "the Elements";
    case CARD_SUMMON_DEMON:    return "the Pentagram";
    case CARD_SUMMON_WEAPON:   return "the Dance";
    case CARD_SUMMON_FLYING:   return "Foxfire";
    case CARD_RANGERS:         return "the Rangers";
    case CARD_SUMMON_UGLY:     return "Repulsiveness";
    case CARD_XOM:             return "Xom";
    case CARD_FAMINE:          return "Famine";
    case CARD_FEAST:           return "the Feast";
    case CARD_WARPWRIGHT:      return "Warpwright";
    case CARD_SHAFT:           return "the Shaft";
    case CARD_VITRIOL:         return "Vitriol";
    case CARD_CLOUD:           return "the Cloud";
    case CARD_HAMMER:          return "the Hammer";
    case CARD_VENOM:           return "Venom";
    case CARD_STORM:           return "the Storm";
    case CARD_FORTITUDE:       return "Fortitude";
    case CARD_PAIN:            return "Pain";
    case CARD_TORMENT:         return "Torment";
    case CARD_WRATH:           return "Wrath";
    case CARD_WRAITH:          return "the Wraith";
    case CARD_CURSE:           return "the Curse";
    case CARD_SWINE:           return "the Swine";
    case CARD_ALCHEMIST:       return "the Alchemist";
    case CARD_ORB:             return "the Orb";
    case CARD_MERCENARY:       return "the Mercenary";
    case CARD_ILLUSION:        return "the Illusion";
    case CARD_DEGEN:           return "Degeneration";
    case NUM_CARDS:            return "a buggy card";
    }
    return "a very buggy card";
}

card_type name_to_card(string name)
{
    for (int i = 0; i < NUM_CARDS; i++)
    {
        if (card_name(static_cast<card_type>(i)) == name)
            return static_cast<card_type>(i);
    }
    return NUM_CARDS;
}

static const vector<const deck_archetype *> _subdecks(uint8_t deck_type)
{
    vector<const deck_archetype *> subdecks;

    switch (deck_type)
    {
    case MISC_DECK_OF_ESCAPE:
        subdecks.push_back(deck_of_transport);
        subdecks.push_back(deck_of_emergency);
        break;
    case MISC_DECK_OF_DESTRUCTION:
        subdecks.push_back(deck_of_destruction);
        break;
#if TAG_MAJOR_VERSION == 34
    case MISC_DECK_OF_DUNGEONS:
        subdecks.push_back(deck_of_dungeons);
        break;
#endif
    case MISC_DECK_OF_SUMMONING:
        subdecks.push_back(deck_of_summoning);
        break;
    case MISC_DECK_OF_WONDERS:
        subdecks.push_back(deck_of_wonders);
        break;
    case MISC_DECK_OF_PUNISHMENT:
        subdecks.push_back(deck_of_punishment);
        break;
    case MISC_DECK_OF_WAR:
        subdecks.push_back(deck_of_battle);
        subdecks.push_back(deck_of_summoning);
        break;
    case MISC_DECK_OF_CHANGES:
        subdecks.push_back(deck_of_battle);
        subdecks.push_back(deck_of_wonders);
        break;
    case MISC_DECK_OF_DEFENCE:
        subdecks.push_back(deck_of_emergency);
        subdecks.push_back(deck_of_battle);
        break;
    }

    ASSERT(subdecks.size() > 0);
    return subdecks;
}

const string deck_contents(uint8_t deck_type)
{
    string output = "It may contain the following cards: ";
    bool first = true;

    // XXX: This awkward way of doing things is intended to prevent a card
    // that appears in multiple subdecks from showing up twice in the
    // output.
    FixedVector<bool, NUM_CARDS> cards;
    cards.init(false);
    const vector<const deck_archetype *> subdecks = _subdecks(deck_type);
    for (unsigned int i = 0; i < subdecks.size(); i++)
    {
        const deck_archetype *pdeck = subdecks[i];
        for (int j = 0; pdeck[j].card != NUM_CARDS; ++j)
            cards[pdeck[j].card] = true;
    }

    for (int i = 0; i < NUM_CARDS; i++)
    {
        if (!cards[i])
            continue;

        if (!first)
            output += ", ";
        else
            first = false;

        output += card_name(static_cast<card_type>(i));
    }

    if (first)
        output += "BUGGY cards";
    output += ".";

    return output;
}

static const deck_archetype* _random_sub_deck(uint8_t deck_type)
{
    const vector<const deck_archetype *> subdecks = _subdecks(deck_type);
    const deck_archetype *pdeck = subdecks[random2(subdecks.size())];

    ASSERT(pdeck);

    return pdeck;
}

static card_type _choose_from_archetype(const deck_archetype* pdeck,
                                        deck_rarity_type rarity)
{
    // Random rarity should have been replaced by one of the others by now.
    ASSERT_RANGE(rarity, DECK_RARITY_COMMON, DECK_RARITY_LEGENDARY + 1);

    // FIXME: We should use one of the various choose_random_weighted
    // functions here, probably with an iterator, instead of
    // duplicating the implementation.

    int totalweight = 0;
    card_type result = NUM_CARDS;
    for (int i = 0; pdeck[i].card != NUM_CARDS; ++i)
    {
        const card_with_weights& cww = pdeck[i];
        if (_card_forbidden(cww.card))
            continue;
        totalweight += cww.weight[rarity - DECK_RARITY_COMMON];
        if (x_chance_in_y(cww.weight[rarity - DECK_RARITY_COMMON], totalweight))
            result = cww.card;
    }
    return result;
}

static card_type _random_card(uint8_t deck_type, deck_rarity_type rarity,
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
    return _random_card(item.sub_type, item.deck_rarity, was_oddity);
}

static card_type _draw_top_card(item_def& deck, bool message,
                                uint8_t &_flags)
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
        const char *verb = (_flags & CFLAG_DEALT) ? "deal" : "draw";

        if (_flags & CFLAG_MARKED)
            mprf("You %s %s.", verb, card_name(card));
        else
            mprf("You %s a card... It is %s.", verb, card_name(card));

        _check_odd_card(_flags);
    }

    return card;
}

static void _push_top_card(item_def& deck, card_type card,
                           uint8_t _flags)
{
    CrawlHashTable &props = deck.props;
    CrawlVector    &cards = props["cards"].get_vector();
    CrawlVector    &flags = props["card_flags"].get_vector();

    cards.push_back((char) card);
    flags.push_back((char) _flags);
}

static void _remember_drawn_card(item_def& deck, card_type card, bool allow_id)
{
    ASSERT(is_deck(deck));
    CrawlHashTable &props = deck.props;
    CrawlVector &drawn = props["drawn_cards"].get_vector();
    drawn.push_back(static_cast<char>(card));

    // Once you've drawn two cards, you know the deck.
    if (allow_id && (drawn.size() >= 2 || origin_is_god_gift(deck)))
        _deck_ident(deck);
}

const vector<card_type> get_drawn_cards(const item_def& deck)
{
    vector<card_type> result;
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
    ostream& strm = msg::streams(MSGCH_DIAGNOSTICS);
    if (!is_deck(deck))
    {
        crawl_state.zero_turns_taken();
        strm << "This isn't a deck at all!" << endl;
        return true;
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
                if (deck.used_count >= 0)
                    cards_left = deck.initial_cards - deck.used_count;
                else
                    cards_left = -deck.used_count;

                if (cards_left != 0)
                {
                    strm << " But there should have been " <<  cards_left
                         << " cards left.";
                }
            }
        }
        strm << endl
             << "A swarm of software bugs snatches the deck from you "
                "and whisks it away." << endl;

        if (deck.link == you.equip[EQ_WEAPON])
            unwield_item();

        dec_inv_item_quantity(deck.link, 1);

        return true;
    }

    bool problems = false;

    CrawlVector &cards = props["cards"].get_vector();
    CrawlVector &flags = props["card_flags"].get_vector();

    vec_size num_cards = cards.size();
    vec_size num_flags = flags.size();

    unsigned int num_buggy     = 0;
    unsigned int num_marked    = 0;

    for (vec_size i = 0; i < num_cards; ++i)
    {
        uint8_t card   = cards[i].get_byte();
        uint8_t _flags = flags[i].get_byte();

        // Bad card, or "dealt" card not on the top.
        if (card >= NUM_CARDS
            || (_flags & CFLAG_DEALT) && i < num_cards - 1)
        {
            cards.erase(i);
            flags.erase(i);
            i--;
            num_cards--;
            num_buggy++;
        }
        else if (_flags & CFLAG_MARKED)
            num_marked++;
    }

    if (num_buggy > 0)
    {
        strm << num_buggy << " buggy cards found in the deck, discarding them."
             << endl;

        deck.used_count += num_buggy;

        num_cards = cards.size();
        num_flags = cards.size();

        problems = true;
    }

    if (num_cards == 0)
    {
        crawl_state.zero_turns_taken();

        strm << "Oops, all of the cards seem to be gone." << endl
             << "A swarm of software bugs snatches the deck from you "
                "and whisks it away." << endl;

        if (deck.link == you.equip[EQ_WEAPON])
            unwield_item();

        dec_inv_item_quantity(deck.link, 1);

        return true;
    }

    if (num_cards > deck.initial_cards)
    {
        if (deck.initial_cards == 0)
            strm << "Deck was created with zero cards???" << endl;
        else if (deck.initial_cards < 0)
            strm << "Deck was created with *negative* cards?!" << endl;
        else
            strm << "Deck has more cards than it was created with?" << endl;

        deck.initial_cards = num_cards;
        problems  = true;
    }

    if (num_cards > num_flags)
    {
        strm << (num_cards - num_flags) << " more cards than flags.";
        strm << endl;
        for (unsigned int i = num_flags + 1; i <= num_cards; ++i)
            flags[i] = static_cast<char>(0);

        problems = true;
    }
    else if (num_flags > num_cards)
    {
        strm << (num_cards - num_flags) << " more cards than flags.";
        strm << endl;

        for (unsigned int i = num_flags; i > num_cards; --i)
            flags.erase(i);

        problems = true;
    }

    if (props["num_marked"].get_byte() > static_cast<char>(num_cards))
    {
        strm << "More cards marked than in the deck?" << endl;
        props["num_marked"] = static_cast<char>(num_marked);
        problems = true;
    }
    else if (props["num_marked"].get_byte() != static_cast<char>(num_marked))
    {
        strm << "Oops, counted " << static_cast<int>(num_marked)
             << " marked cards, but num_marked is "
             << (static_cast<int>(props["num_marked"].get_byte()));
        strm << endl;

        props["num_marked"] = static_cast<char>(num_marked);
        problems = true;
    }

    if (deck.used_count >= 0)
    {
        if (deck.initial_cards != (deck.used_count + num_cards))
        {
            strm << "Have you used " << deck.used_count << " cards, or "
                 << (deck.initial_cards - num_cards) << "? Oops.";
            strm << endl;
            deck.used_count = deck.initial_cards - num_cards;
            problems = true;
        }
    }
    else
    {
        if (-deck.used_count != num_cards)
        {
            strm << "There are " << num_cards << " cards left, not "
                 << (-deck.used_count) << ".  Oops.";
            strm << endl;
            deck.used_count = -num_cards;
            problems = true;
        }
    }

    if (!problems)
        return false;

    you.wield_change = true;

    if (!yesno("Problems might not have been completely fixed; "
               "still use deck?", true, 'n'))
    {
        crawl_state.zero_turns_taken();
        return true;
    }
    return false;
}

// Choose a deck from inventory and return its slot (or -1).
static int _choose_inventory_deck(const char* prompt)
{
    const int slot = prompt_invent_item(prompt,
                                        MT_INVLIST, OSEL_DRAW_DECK,
                                        true, true, true, 0, -1, NULL,
                                        OPER_EVOKE);

    if (prompt_failed(slot))
        return -1;

    if (!is_deck(you.inv[slot]))
    {
        mpr("That isn't a deck!");
        return -1;
    }

    return slot;
}

static void _deck_ident(item_def& deck)
{
    if (in_inventory(deck) && !item_ident(deck, ISFLAG_KNOW_TYPE))
    {
        set_ident_flags(deck, ISFLAG_KNOW_TYPE);
        mprf("This is %s.", deck.name(DESC_A).c_str());
        you.wield_change = true;
    }
}

bool deck_identify_first(int slot)
{
    item_def& deck(you.inv[slot]);
    if (top_card_is_known(deck))
        return false;

    uint8_t flags;
    card_type card = get_card_and_flags(deck, -1, flags);

    _set_card_and_flags(deck, -1, card, flags | CFLAG_SEEN | CFLAG_MARKED);
    deck.props["num_marked"]++;

    mprf("You get a glimpse of the first card. It is %s.", card_name(card));
    return true;
}

// Draw the top four cards of an unmarked deck and play them all.
// Discards the rest of the deck.  Return false if the operation was
// failed/aborted along the way.
bool deck_deal()
{
    const int slot = _choose_inventory_deck("Deal from which deck?");
    if (slot == -1)
    {
        crawl_state.zero_turns_taken();
        return false;
    }
    item_def& deck(you.inv[slot]);
    if (_check_buggy_deck(deck))
        return false;

    CrawlHashTable &props = deck.props;
    if (props["num_marked"].get_byte() > 0)
    {
        mpr("You cannot deal from marked decks.");
        crawl_state.zero_turns_taken();
        return false;
    }
    if (props["stacked"].get_bool())
    {
        mpr("This deck seems insufficiently random for dealing.");
        crawl_state.zero_turns_taken();
        return false;
    }

    const int num_cards = cards_in_deck(deck);
    _deck_ident(deck);

    if (num_cards == 1)
        mpr("There's only one card left!");
    else if (num_cards < 4)
        mprf("The deck only has %d cards.", num_cards);

    const int num_to_deal = (num_cards < 4 ? num_cards : 4);

    for (int i = 0; i < num_to_deal; ++i)
    {
        int last = cards_in_deck(deck) - 1;
        uint8_t flags;

        // Flag the card as dealt (changes messages and gives no piety).
        card_type card = get_card_and_flags(deck, last, flags);
        _set_card_and_flags(deck, last, card, flags | CFLAG_DEALT);

        evoke_deck(deck);
        redraw_screen();
    }

    // Nemelex doesn't like dealers with inadequate decks.
    if (num_to_deal < 4)
    {
        mpr("Nemelex gives you another card to finish dealing.");
        draw_from_deck_of_punishment(true);
    }

    // If the deck had cards left, exhaust it.
    if (deck.quantity > 0)
    {
        canned_msg(MSG_DECK_EXHAUSTED);
        if (slot == you.equip[EQ_WEAPON])
            unwield_item();

        dec_inv_item_quantity(slot, 1);
    }

    return true;
}

static void _redraw_stacked_cards(const vector<card_type>& draws,
                                  unsigned int selected)
{
    for (unsigned int i = 0; i < draws.size(); ++i)
    {
        cgotoxy(1, i+2);
        textcolour(selected == i ? WHITE : LIGHTGREY);
        cprintf("%u - %s", i+1, card_name(draws[i]));
        clear_to_end_of_line();
    }
}

static bool _card_in_deck(card_type card, const deck_archetype *pdeck)
{
    for (int i = 0; pdeck[i].card != NUM_CARDS; ++i)
    {
        if (pdeck[i].card == card)
            return true;
    }

    return false;
}

string which_decks(card_type card)
{
    vector<string> decks;
    string output = "";
    bool punishment = false;
    for (uint8_t deck = MISC_FIRST_DECK; deck <= MISC_LAST_DECK; deck++)
    {
        vector<const deck_archetype *> subdecks = _subdecks(deck);
        for (unsigned int i = 0; i < subdecks.size(); i++)
        {
            if (_card_in_deck(card, subdecks[i]))
            {
                if (deck == MISC_DECK_OF_PUNISHMENT)
                    punishment = true;
                else
                {
                    item_def tmp;
                    tmp.base_type = OBJ_MISCELLANY;
                    tmp.sub_type = deck;
                    // 8 - "deck of "
                    decks.push_back(sub_type_string(tmp, true).substr(8));
                }
                break;
            }
        }
    }

    if (decks.size())
    {
        output += "It is usually found in decks of "
               +  comma_separated_line(decks.begin(), decks.end());
        if (punishment)
            output += ", or in Nemelex Xobeh's deck of punishment";
        output += ".";
    }
    else if (punishment)
    {
        output += "It is usually only found in Nemelex Xobeh's deck of "
                  "punishment.";
    }
    else
        output += " It is normally not part of any deck.";

    return output;
}

static void _describe_cards(vector<card_type> cards)
{
    ASSERT(!cards.empty());

#ifdef USE_TILE_WEB
    tiles_crt_control show_as_menu(CRT_MENU, "describe_cards");
#endif

    ostringstream data;
    for (unsigned int i = 0; i < cards.size(); ++i)
    {
        string name = card_name(cards[i]);
        string desc = getLongDescription(name + " card");
        if (desc.empty())
            desc = "No description found.";

        name = uppercase_first(name);
        data << "<w>" << name << "</w>\n"
             << get_linebreak_string(desc, get_number_of_cols() - 1)
             << "\n" << which_decks(cards[i]) << "\n";
    }
    formatted_string fs = formatted_string::parse_string(data.str());
    clrscr();
    fs.display();
    getchm();
}

// Stack a deck: look at the next five cards, put them back in any
// order, discard the rest of the deck.
// Return false if the operation was failed/aborted along the way.
bool deck_stack()
{
    const int slot = _choose_inventory_deck("Stack which deck?");
    if (slot == -1)
    {
        crawl_state.zero_turns_taken();
        return false;
    }

    item_def& deck(you.inv[slot]);
    if (_check_buggy_deck(deck))
        return false;

    CrawlHashTable &props = deck.props;
    if (props["num_marked"].get_byte() > 0)
    {
        mpr("You can't stack a marked deck.");
        crawl_state.zero_turns_taken();
        return false;
    }

    _deck_ident(deck);
    const int num_cards    = cards_in_deck(deck);

    if (num_cards == 1)
        mpr("There's only one card left!");
    else if (num_cards < 5)
        mprf("The deck only has %d cards.", num_cards);
    else if (num_cards == 5)
        mpr("The deck has exactly five cards.");
    else
    {
        mprf("You draw the first five cards out of %d and discard the rest.",
             num_cards);
    }
    more();

    run_uncancel(UNC_STACK_FIVE, slot);
    return true;
}

bool stack_five(int slot)
{
    item_def& deck(you.inv[slot]);
    if (_check_buggy_deck(deck))
        return false;

    const int num_cards    = cards_in_deck(deck);
    const int num_to_stack = (num_cards < 5 ? num_cards : 5);

#ifdef USE_TILE_WEB
    tiles_crt_control show_as_menu(CRT_MENU, "deck_stack");
#endif

    vector<card_type> draws;
    vector<uint8_t>   flags;
    for (int i = 0; i < num_cards; ++i)
    {
        uint8_t   _flags;
        card_type card = _draw_top_card(deck, false, _flags);

        if (i < num_to_stack)
        {
            draws.push_back(card);
            flags.push_back(_flags | CFLAG_SEEN | CFLAG_MARKED);
        }
        // Rest of deck is discarded.
    }

    CrawlHashTable &props = deck.props;
    deck.used_count = -num_to_stack;
    props["num_marked"] = static_cast<char>(num_to_stack);
    // Remember that the deck was stacked even if it is later unmarked
    // (e.g. by Nemelex abandonment).
    props["stacked"] = true;
    you.wield_change = true;
    bool done = true;

    if (draws.size() > 1)
    {
        bool need_prompt_redraw = true;
        unsigned int selected = draws.size();
        while (true)
        {
            if (need_prompt_redraw)
            {
                clrscr();
                cgotoxy(1,1);
                textcolour(WHITE);
                cprintf("Press a digit to select a card, then another digit "
                        "to swap it.");
                cgotoxy(1,10);
                cprintf("Press ? for the card descriptions, or Enter to "
                        "accept.");

                _redraw_stacked_cards(draws, selected);
                need_prompt_redraw = false;
            }

            // Hand-hacked implementation, instead of using Menu. Oh well.
            const int c = getchk();
            if (c == CK_ENTER)
            {
                cgotoxy(1,11);
                textcolour(LIGHTGREY);
                cprintf("Are you done? (press y or Y to confirm)");
                if (toupper(getchk()) == 'Y')
                    break;

                cgotoxy(1,11);
                clear_to_end_of_line();
                continue;
            }

            if (c == '?')
            {
                _describe_cards(draws);
                need_prompt_redraw = true;
            }
            else if (c >= '1' && c <= '0' + static_cast<int>(draws.size()))
            {
                const unsigned int new_selected = c - '1';
                if (selected < draws.size())
                {
                    swap(draws[selected], draws[new_selected]);
                    swap(flags[selected], flags[new_selected]);
                    selected = draws.size();
                }
                else
                    selected = new_selected;

                _redraw_stacked_cards(draws, selected);
            }
            else if (c == CK_ESCAPE && crawl_state.seen_hups)
            {
                done = false;
                break; // continue on game restore
            }
        }
        redraw_screen();
    }
    for (unsigned int i = 0; i < draws.size(); ++i)
    {
        _push_top_card(deck, draws[draws.size() - 1 - i],
                       flags[flags.size() - 1 - i]);
    }

    _check_buggy_deck(deck);
    you.wield_change = true;

    return done;
}

// Draw the next three cards, discard two and pick one.
bool deck_triple_draw()
{
    const int slot = _choose_inventory_deck("Triple draw from which deck?");
    if (slot == -1)
    {
        crawl_state.zero_turns_taken();
        return false;
    }

    item_def& deck(you.inv[slot]);
    if (_check_buggy_deck(deck))
        return false;

    run_uncancel(UNC_DRAW_THREE, slot);
    return true;
}

bool draw_three(int slot)
{
    item_def& deck(you.inv[slot]);

    if (_check_buggy_deck(deck))
        return false;

    const int num_cards = cards_in_deck(deck);

    // We have to identify the deck before removing cards from it.
    // Otherwise, _remember_drawn_card() will implicitly call
    // _deck_ident() when the deck might have no cards left.
    _deck_ident(deck);

    if (num_cards == 1)
    {
        // Only one card to draw, so just draw it.
        mpr("There's only one card left!");
        evoke_deck(deck);
        return true;
    }

    const int num_to_draw = (num_cards < 3 ? num_cards : 3);
    vector<card_type> draws;
    vector<uint8_t>   flags;

    for (int i = 0; i < num_to_draw; ++i)
    {
        uint8_t _flags;
        card_type card = _draw_top_card(deck, false, _flags);

        draws.push_back(card);
        flags.push_back(_flags);
    }

    int selected = -1;
    bool need_prompt_redraw = true;
    while (true)
    {
        if (need_prompt_redraw)
        {
            mpr("You draw... (choose one card, ? for their descriptions)");
            for (int i = 0; i < num_to_draw; ++i)
            {
                msg::streams(MSGCH_PROMPT) << (static_cast<char>(i + 'a')) << " - "
                                           << card_name(draws[i]) << endl;
            }
            need_prompt_redraw = false;
        }
        const int keyin = toalower(get_ch());

        if (crawl_state.seen_hups)
        {
            // Return the cards, for now.
            for (int i = 0; i < num_to_draw; ++i)
                _push_top_card(deck, draws[i], flags[i]);

            return false;
        }

        if (keyin == '?')
        {
            _describe_cards(draws);
            redraw_screen();
            need_prompt_redraw = true;
        }
        else if (keyin >= 'a' && keyin < 'a' + num_to_draw)
        {
            selected = keyin - 'a';
            break;
        }
        else
            canned_msg(MSG_HUH);
    }

    // Note how many cards were removed from the deck.
    deck.used_count += num_to_draw;

    // Don't forget to update the number of marked ones, too.
    // But don't reduce the number of non-brownie draws.
    uint8_t num_marked_left = deck.props["num_marked"].get_byte();
    for (int i = 0; i < num_to_draw; ++i)
    {
        _remember_drawn_card(deck, draws[i], false);
        if (flags[i] & CFLAG_MARKED)
        {
            ASSERT(num_marked_left > 0);
            --num_marked_left;
        }
    }
    deck.props["num_marked"] = num_marked_left;

    you.wield_change = true;

    // Make deck disappear *before* the card effect, since we
    // don't want to unwield an empty deck.
    const deck_rarity_type rarity = deck.deck_rarity;
    if (cards_in_deck(deck) == 0)
    {
        canned_msg(MSG_DECK_EXHAUSTED);
        if (slot == you.equip[EQ_WEAPON])
            unwield_item();

        dec_inv_item_quantity(slot, 1);
    }

    // Note that card_effect() might cause you to unwield the deck.
    card_effect(draws[selected], rarity,
                flags[selected] | CFLAG_SEEN | CFLAG_MARKED, false);

    return true;
}

// This is Nemelex retribution. If deal is true, use the word "deal"
// rather than "draw" (for the Deal Four out-of-cards situation).
void draw_from_deck_of_punishment(bool deal)
{
    bool oddity;
    uint8_t flags = CFLAG_PUNISHMENT;
    if (deal)
        flags |= CFLAG_DEALT;
    card_type card = _random_card(MISC_DECK_OF_PUNISHMENT, DECK_RARITY_COMMON,
                                  oddity);

    mprf("You %s a card...", deal ? "deal" : "draw");
    card_effect(card, DECK_RARITY_COMMON, flags);
}

static int _xom_check_card(item_def &deck, card_type card,
                           uint8_t flags)
{
    int amusement = 64;

    if (flags & CFLAG_PUNISHMENT)
        amusement = 200;
    else if (!item_type_known(deck))
        amusement *= 2;
    // Expecting one type of card but got another, real funny.
    else if (flags & CFLAG_ODDITY)
        amusement = 200;

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
        if (player_in_branch(BRANCH_ABYSS))
            amusement = 0;
        break;

    case CARD_FAMINE:
    case CARD_CURSE:
    case CARD_SWINE:
        // Always hilarious.
        amusement = 255;

    default:
        break;
    }

    return amusement;
}

void evoke_deck(item_def& deck)
{
    if (_check_buggy_deck(deck))
        return;

    bool allow_id = in_inventory(deck) && !item_ident(deck, ISFLAG_KNOW_TYPE);

    const deck_rarity_type rarity = deck.deck_rarity;
    CrawlHashTable &props = deck.props;

    uint8_t flags = 0;
    card_type card = _draw_top_card(deck, true, flags);

    // Passive Nemelex retribution: sometimes a card gets swapped out.
    // More likely to happen with marked decks.
    if (player_under_penance(GOD_NEMELEX_XOBEH))
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
                flags |= CFLAG_PUNISHMENT;
                simple_god_message(" seems to have exchanged this card "
                                   "behind your back!", GOD_NEMELEX_XOBEH);
                mprf("It's actually %s.", card_name(card));
                // You never completely appease Nemelex, but the effects
                // get less frequent.
                you.penance[GOD_NEMELEX_XOBEH] -=
                    random2((you.penance[GOD_NEMELEX_XOBEH]+18) / 10);
            }
        }
    }

    const int amusement   = _xom_check_card(deck, card, flags);

    // Oddity and punishment cards don't give any information about the deck.
    if (flags & (CFLAG_ODDITY | CFLAG_PUNISHMENT))
        allow_id = false;

    // Do these before the deck item_def object is gone.
    if (flags & CFLAG_MARKED)
        props["num_marked"]--;

    deck.used_count++;
    _remember_drawn_card(deck, card, allow_id);

    // Get rid of the deck *before* the card effect because a card
    // might cause a wielded deck to be swapped out for something else,
    // in which case we don't want an empty deck to go through the
    // swapping process.
    const bool deck_gone = (cards_in_deck(deck) == 0);
    if (deck_gone)
    {
        canned_msg(MSG_DECK_EXHAUSTED);
        dec_inv_item_quantity(deck.link, 1);
    }

    card_effect(card, rarity, flags, false);

    if (!(flags & CFLAG_MARKED))
    {
        // Could a Xom worshipper ever get a stacked deck in the first
        // place?
        xom_is_stimulated(amusement);
    }

    // Always wield change, since the number of cards used/left has
    // changed, and it might be wielded.
    you.wield_change = true;
}

static int _get_power_level(int power, deck_rarity_type rarity)
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
    case DECK_RARITY_RANDOM:
        die("unset deck rarity");
    }
    dprf("Power level: %d", power_level);
    return power_level;
}

static void _suppressed_card_message(god_type god, conduct_type done)
{
    string forbidden_act;

    switch (done)
    {
        case DID_NECROMANCY:
        case DID_UNHOLY: forbidden_act = "evil"; break;

        case DID_POISON: forbidden_act = "poisonous"; break;

        case DID_CHAOS: forbidden_act = "chaotic"; break;

        case DID_HASTY: forbidden_act = "hasty"; break;

        case DID_FIRE: forbidden_act = "fiery"; break;

        default: forbidden_act = "buggy"; break;
    }

    mprf("By %s power, the %s magic on the card dissipates harmlessly.",
         apostrophise(god_name(you.religion)).c_str(), forbidden_act.c_str());
}

// Actual card implementations follow.

static void _swap_monster_card(int power, deck_rarity_type rarity)
{
    // Swap between you and another monster.
    // Don't choose yourself unless there are no monsters nearby.
    monster* mon_to_swap = choose_random_nearby_monster(0);
    if (!mon_to_swap)
        mpr("You spin around.");
    else
        swap_with_monster(mon_to_swap);
}

static void _velocity_card(int power, deck_rarity_type rarity)
{

    const int power_level = _get_power_level(power, rarity);
    bool did_something = false;
    enchant_type for_allies = ENCH_NONE, for_hostiles = ENCH_NONE;

    if (you.duration[DUR_SLOW] && (power_level > 0 || coinflip()))
    {
        if (you_worship(GOD_CHEIBRIADOS))
            simple_god_message(" protects you from inadvertent hurry.");
        else
        {
            you.duration[DUR_SLOW] = 1;
            did_something = true;
        }
    }

    if (you.duration[DUR_HASTE] && (power_level == 0 && coinflip()))
    {
        you.duration[DUR_HASTE] = 1;
        did_something = true;
    }

    switch (power_level)
    {
        case 0:
            for_allies = for_hostiles = random_choose(ENCH_SLOW, ENCH_HASTE,
                                                      ENCH_SWIFT, -1);
            break;

        case 1:
            if (coinflip())
                for_allies = ENCH_HASTE;
            else
                for_hostiles = ENCH_SLOW;
            break;

        case 2:
            for_allies = ENCH_HASTE; for_hostiles = ENCH_SLOW;
            break;
    }

    for (radius_iterator ri(you.pos(), LOS_NO_TRANS); ri; ++ri)
    {
        monster* mon = monster_at(*ri);

        if (mon && !mons_immune_magic(mon))
        {
            const bool hostile = !mon->wont_attack();
            const bool haste_immune = (mon->check_stasis(false)
                                || mons_is_immotile(mon));

            bool did_haste = false;
            bool did_swift = false;

            if (hostile)
            {
                if (for_hostiles != ENCH_NONE)
                {
                    if (for_hostiles == ENCH_SLOW)
                    {
                        do_slow_monster(mon, &you);
                        did_something = true;
                    }
                    else if (!(for_hostiles == ENCH_HASTE && haste_immune))
                    {
                        if (you_worship(GOD_CHEIBRIADOS))
                            _suppressed_card_message(you.religion, DID_HASTY);
                        else
                        {
                            mon->add_ench(for_hostiles);
                            did_something = true;
                            if (for_hostiles == ENCH_HASTE)
                                did_haste = true;
                            else if (for_hostiles == ENCH_SWIFT)
                                did_swift = true;
                        }
                    }
                }
            }
            else //allies
            {
                if (for_allies != ENCH_NONE)
                {
                    if (for_allies == ENCH_SLOW)
                    {
                        do_slow_monster(mon, &you);
                        did_something = true;
                    }
                    else if (!(for_allies == ENCH_HASTE && haste_immune))
                    {
                        if (you_worship(GOD_CHEIBRIADOS))
                            _suppressed_card_message(you.religion, DID_HASTY);
                        else
                        {
                            mon->add_ench(for_allies);
                            did_something = true;
                            if (for_allies == ENCH_HASTE)
                                did_haste = true;
                            else if (for_allies == ENCH_SWIFT)
                                did_swift = true;
                        }
                    }
                }
            }

            if (did_haste)
                simple_monster_message(mon, " seems to speed up.");

            if (did_swift)
                simple_monster_message(mon, " is moving somewhat quickly.");
        }
    }

    if (!did_something)
        canned_msg(MSG_NOTHING_HAPPENS);
}

static void _banshee_card(int power, deck_rarity_type rarity)
{
    const int power_level = _get_power_level(power, rarity);

    if (!is_good_god(you.religion))
    {
        for (radius_iterator ri(you.pos(), LOS_NO_TRANS); ri; ++ri)
        {
            monster* mon = monster_at(*ri);

            if (mon && !mon->wont_attack())
                mon->drain_exp(&you, false, 3 * (power_level + 1));
        }
    }
    mass_enchantment(ENCH_FEAR, power);
}

static void _damnation_card(int power, deck_rarity_type rarity)
{
    if (player_in_branch(BRANCH_ABYSS))
    {
        canned_msg(MSG_NOTHING_HAPPENS);
        return;
    }

    // Calculate how many extra banishments you get.
    const int power_level = _get_power_level(power, rarity);
    int nemelex_bonus = 0;
    if (in_good_standing(GOD_NEMELEX_XOBEH))
        nemelex_bonus = you.piety;

    int extra_targets = power_level + random2(you.skill(SK_EVOCATIONS, 20)
                                              + nemelex_bonus) / 240;

    for (int i = 0; i < 1 + extra_targets; ++i)
    {
        // Pick a random monster nearby to banish (or yourself).
        monster* mon_to_banish = choose_random_nearby_monster(1);

        // Bonus banishments only banish monsters.
        if (i != 0 && !mon_to_banish)
            continue;

        if (!mon_to_banish) // Banish yourself!
        {
            banished("drawing a card");
            break;              // Don't banish anything else.
        }
        else
            mon_to_banish->banish(&you);
    }
}

static void _warpwright_card(int power, deck_rarity_type rarity)
{
    const int power_level = _get_power_level(power, rarity);

    if (player_in_branch(BRANCH_ABYSS))
    {
        mpr("The power of the Abyss blocks your magic.");
        return;
    }

    int count = 0;
    coord_def f;
    for (adjacent_iterator ai(you.pos()); ai; ++ai)
        if (grd(*ai) == DNGN_FLOOR && !find_trap(*ai) && one_chance_in(++count))
            f = *ai;

    if (count > 0)              // found a spot
    {
        if (place_specific_trap(f, TRAP_TELEPORT, 1 + random2(5 * power_level)))
        {
            // Mark it discovered if enough power.
            if (x_chance_in_y(power_level, 2))
                find_trap(f)->reveal();
        }
    }
}

static void _shaft_card(int power, deck_rarity_type rarity)
{
    const int power_level = _get_power_level(power, rarity);

    if (is_valid_shaft_level())
    {
        if (grd(you.pos()) == DNGN_FLOOR
            && place_specific_trap(you.pos(), TRAP_SHAFT))
        {
            find_trap(you.pos())->reveal();
            mpr("A shaft materialises beneath you!");
        }

        for (radius_iterator di(you.pos(), LOS_NO_TRANS); di; ++di)
        {
            monster *mons = monster_at(*di);

            if (mons && !mons->wont_attack()
                && grd(mons->pos()) == DNGN_FLOOR
                && !mons->airborne() && !mons_is_firewood(mons)
                && x_chance_in_y(power_level, 3))
            {
                mons->do_shaft();
            }
        }
    }
    else
        canned_msg(MSG_NOTHING_HAPPENS);
}

static int stair_draw_count = 0;

// This does not describe an actual card. Instead, it only exists to test
// the stair movement effect in wizard mode ("&c stairs").
static void _stairs_card(int power, deck_rarity_type rarity)
{
    UNUSED(power);
    UNUSED(rarity);

    you.duration[DUR_REPEL_STAIRS_MOVE]  = 0;
    you.duration[DUR_REPEL_STAIRS_CLIMB] = 0;

    if (feat_stair_direction(grd(you.pos())) == CMD_NO_CMD)
        you.duration[DUR_REPEL_STAIRS_MOVE]  = 1000;
    else
        you.duration[DUR_REPEL_STAIRS_CLIMB] =  500; // more annoying

    vector<coord_def> stairs_avail;

    for (radius_iterator ri(you.pos(), LOS_DEFAULT, true); ri; ++ri)
    {
        dungeon_feature_type feat = grd(*ri);
        if (feat_stair_direction(feat) != CMD_NO_CMD
            && feat != DNGN_ENTER_SHOP)
        {
            stairs_avail.push_back(*ri);
        }
    }

    if (stairs_avail.empty())
    {
        mpr("No stairs available to move.");
        return;
    }

    shuffle_array(stairs_avail);

    for (unsigned int i = 0; i < stairs_avail.size(); ++i)
        move_stair(stairs_avail[i], stair_draw_count % 2, false);

    stair_draw_count++;
}

static void _damaging_card(card_type card, int power, deck_rarity_type rarity,
                           bool dealt = false)
{
    const int power_level = _get_power_level(power, rarity);
    const char *participle = dealt ? "dealt" : "drawn";

    dist target;
    zap_type ztype = ZAP_DEBUGGING_RAY;
    const zap_type hammerzaps[3]  = { ZAP_STONE_ARROW, ZAP_IRON_SHOT,
                                      ZAP_LEHUDIBS_CRYSTAL_SPEAR };
    const zap_type painzaps[2]   = { ZAP_AGONY, ZAP_BOLT_OF_DRAINING };
    const zap_type orbzaps[3]    = { ZAP_ISKENDERUNS_MYSTIC_BLAST, ZAP_IOOD,
                                     ZAP_IOOD };
    bool venom_vuln = false;

    switch (card)
    {
    case CARD_VITRIOL:
        if (power_level == 2 || (power_level == 1 && coinflip()))
            ztype = ZAP_CORROSIVE_BOLT;
        else
            ztype = ZAP_BREATHE_ACID;
        break;

    case CARD_VENOM:
        if (power_level < 2)
            venom_vuln = true;

        if (power_level == 2 || (power_level == 1 && coinflip()))
            ztype = ZAP_POISON_ARROW;
        else
            ztype = ZAP_VENOM_BOLT;
        break;

    case CARD_HAMMER:  ztype = hammerzaps[power_level];  break;
    case CARD_ORB:     ztype = orbzaps[power_level];     break;

    case CARD_PAIN:
        if (power_level == 2)
        {
            if (is_good_god(you.religion))
            {
                mprf("You have %s %s.", participle, card_name(card));
                _suppressed_card_message(you.religion, DID_NECROMANCY);
                return;
            }

            if (monster *ghost = create_monster(
                                    mgen_data(MONS_FLAYED_GHOST, BEH_FRIENDLY,
                                    &you, 3, 0, you.pos(), MHITYOU)))
            {
                bool msg = true;
                bolt beem;
                int dam = 5;

                beem.origin_spell = SPELL_FLAY;
                beem.source = ghost->pos();
                beem.source_id = ghost->mid;
                beem.range = 0;

                if (!you.res_torment())
                {
                    if (can_shave_damage())
                        dam = do_shave_damage(dam);

                    if (dam > 0)
                        dec_hp(dam, false);
                }

                for (radius_iterator di(ghost->pos(), LOS_NO_TRANS); di; ++di)
                {
                    monster *mons = monster_at(*di);

                    if (!mons || mons->wont_attack()
                        || mons->holiness() != MH_NATURAL)
                    {
                        continue;
                    }

                    beem.target = mons->pos();
                    ghost->foe = mons->mindex();
                    mons_cast(ghost, beem, SPELL_FLAY,
                             ghost->spell_slot_flags(SPELL_FLAY), msg);
                    msg = false;
                }

                ghost->foe = MHITYOU;
            }

            return;
        }
        else
            ztype = painzaps[power_level];
        break;

    default:
        break;
    }

    string prompt = "You have ";
    prompt += participle;
    prompt += " ";
    prompt += card_name(card);
    prompt += ".";

    bolt beam;
    beam.range = LOS_RADIUS;

    if (card == CARD_VENOM && you_worship(GOD_SHINING_ONE))
    {
        _suppressed_card_message(you.religion, DID_POISON);
        return;
    }

    if (card == CARD_PAIN && is_good_god(you.religion))
    {
        _suppressed_card_message(you.religion, DID_NECROMANCY);
        return;
    }

    if (spell_direction(target, beam, DIR_NONE, TARG_HOSTILE,
                        LOS_RADIUS, true, true, false, NULL, prompt.c_str())
        && player_tracer(ZAP_DEBUGGING_RAY, power/6, beam))
    {
        if (you.confused())
        {
            target.confusion_fuzz();
            beam.set_target(target);
        }

        if (ztype == ZAP_IOOD)
        {
            if (power_level == 1)
                cast_iood(&you, power/6, &beam);
            else
                cast_iood_burst(power/6, beam.target);
        }
        else
            zapping(ztype, power/6, beam);
            if (venom_vuln)
            {
                mpr("Releasing that poison leaves you vulnerable to poison.");
                you.increase_duration(DUR_POISON_VULN, 10 - power_level * 5 + random2(6), 50);
            }
    }
    else if (ztype == ZAP_IOOD && power_level == 2)
    {
        // cancelled orb bursts just become uncontrolled
        cast_iood_burst(power/6, coord_def(-1, -1));
    }
}

static void _elixir_card(int power, deck_rarity_type rarity)
{
    int power_level = _get_power_level(power, rarity);
    const int dur = random2avg(10 * (power_level + 1), 2) * BASELINE_DELAY;

    you.duration[DUR_ELIXIR_HEALTH] = 0;
    you.duration[DUR_ELIXIR_MAGIC] = 0;

    if (power_level == 0)
    {
        if (coinflip())
            you.set_duration(DUR_ELIXIR_HEALTH, 1 + random2(3));
        else
            you.set_duration(DUR_ELIXIR_MAGIC, 3 + random2(5));
    }
    else if (power_level == 1)
    {
        if (you.hp * 2 < you.hp_max)
            you.set_duration(DUR_ELIXIR_HEALTH, 3 + random2(3));
        else
            you.set_duration(DUR_ELIXIR_MAGIC, 10);
    }
    else if (power_level >= 2)
    {
        you.set_duration(DUR_ELIXIR_HEALTH, 10);
        you.set_duration(DUR_ELIXIR_MAGIC, 10);
    }

    for (radius_iterator ri(you.pos(), LOS_NO_TRANS); ri; ++ri)
    {
        monster* mon = monster_at(*ri);

        if (mon && mon->wont_attack())
        {
            mon->add_ench(mon_enchant(ENCH_EPHEMERAL_INFUSION, 50 * (power_level + 1),
                                      &you, dur));
        }
    }
}

static void _helm_card(int power, deck_rarity_type rarity)
{
    const int power_level = _get_power_level(power, rarity);
    bool do_phaseshift = false;
    bool do_stoneskin  = false;
    bool do_shield     = false;
    bool do_resistance = false;

    // Chances are cumulative.
    if (power_level >= 2)
    {
        if (coinflip()) do_phaseshift = true;
        if (coinflip()) do_stoneskin  = true;
        if (coinflip()) do_shield     = true;
        do_resistance = true;
    }
    if (power_level >= 1)
    {
        if (coinflip()) do_phaseshift = true;
        if (coinflip()) do_stoneskin  = true;
        if (coinflip()) do_shield     = true;
    }
    if (power_level >= 0)
    {
        if (coinflip())
            do_phaseshift = true;
        else
            do_stoneskin  = true;
    }

    if (do_phaseshift)
        cast_phase_shift(random2(power/4));
    if (do_stoneskin)
        cast_stoneskin(random2(power/4));
    if (do_resistance)
    {
        mpr("You feel resistant.");
        you.increase_duration(DUR_RESISTANCE, random2(power/7) + 1);
    }
    if (do_shield)
    {
        if (you.duration[DUR_MAGIC_SHIELD] == 0)
            mpr("A magical shield forms in front of you.");
        you.increase_duration(DUR_MAGIC_SHIELD, random2(power/6) + 1);
    }

    for (radius_iterator ri(you.pos(), LOS_NO_TRANS); ri; ++ri)
    {
        monster* mon = monster_at(*ri);

        if (mon && mon->wont_attack() && x_chance_in_y(power_level, 2))
            mon->add_ench(coinflip() ? ENCH_STONESKIN : ENCH_SHROUD);
    }
}

static void _blade_card(int power, deck_rarity_type rarity)
{
    const int power_level = _get_power_level(power, rarity);
    const bool cleaving = you.duration[DUR_CLEAVE] > 0;
    const item_def* const weapon = you.weapon();
    const bool axe = weapon && item_attack_skill(*weapon) == SK_AXES;
    const bool bladed = weapon && can_cut_meat(*weapon);
    const bool plural = (weapon && weapon->quantity > 1) || !weapon;
    string hand_part = "Your " + you.hand_name(true);

    you.increase_duration(DUR_CLEAVE, 10 + random2((power_level + 1) * 10));

    if (!cleaving)
    {
        mprf(MSGCH_DURATION,
             "%s look%s %ssharp%s",
             (weapon) ? weapon->name(DESC_YOUR).c_str() : hand_part.c_str(),
             (plural) ?  "" : "s", (bladed) ? "" : "oddly ",
             (axe) ? " (like it always does)." : ".");
    }
    else
        mprf(MSGCH_DURATION, "Your cleaving ability is renewed.");
}

static void _shadow_card(int power, deck_rarity_type rarity)
{
    const int power_level = _get_power_level(power, rarity);
    const int duration = random2(power/4) + 1;

    if (power_level != 1)
    {
        mpr(you.duration[DUR_STEALTH] ? "You feel more catlike."
                                      : "You feel stealthy.");
        you.increase_duration(DUR_STEALTH, duration);
    }

    if (power_level > 0 && !you.haloed())
        cast_darkness(duration, false);
}

static void _potion_card(int power, deck_rarity_type rarity)
{
    const int power_level = _get_power_level(power, rarity);

    potion_type pot = random_choose_weighted(3, POT_CURING,
                                             1, POT_AGILITY,
                                             1, POT_BRILLIANCE,
                                             1, POT_MIGHT,
                                             1, POT_BERSERK_RAGE,
                                             1, POT_INVISIBILITY,
                                             0);

    if (power_level >= 1 && coinflip())
        pot = (coinflip() ? POT_RESISTANCE : POT_HASTE);

    if (power_level >= 2 && coinflip())
        pot = (coinflip() ? POT_HEAL_WOUNDS : POT_MAGIC);

    if (you_worship(GOD_CHEIBRIADOS) && (pot == POT_HASTE
        || pot == POT_BERSERK_RAGE))
    {
        simple_god_message(" protects you from inadvertent hurry.");
        return;
    }

    potion_effect(pot, random2(power/4));

    for (radius_iterator ri(you.pos(), LOS_NO_TRANS); ri; ++ri)
    {
        monster* mon = monster_at(*ri);

        if (mon && mon->friendly())
            mon->drink_potion_effect(pot, true);
    }

}

static void _focus_card(int power, deck_rarity_type rarity)
{
    stat_type best_stat = STAT_STR;
    stat_type worst_stat = STAT_STR;

    for (int i = 1; i < 3; ++i)
    {
        stat_type s = static_cast<stat_type>(i);
        const int best_diff = you.base_stats[s] - you.base_stats[best_stat];
        if (best_diff > 0 || best_diff == 0 && coinflip())
            best_stat = s;

        const int worst_diff = you.base_stats[s] - you.base_stats[worst_stat];
        if (worst_diff < 0 || worst_diff == 0 && coinflip())
            worst_stat = s;
    }

    while (best_stat == worst_stat)
    {
        best_stat  = static_cast<stat_type>(random2(3));
        worst_stat = static_cast<stat_type>(random2(3));
    }

    modify_stat(best_stat, 1, true, true);
    modify_stat(worst_stat, -1, true, true);
}

static void _remove_bad_mutation()
{
    // Ensure that only bad mutations are removed.
    if (!delete_mutation(RANDOM_BAD_MUTATION, "helix card", false, false, false, true))
        mpr("You feel transcendent for a moment.");
}

static void _helix_card(int power, deck_rarity_type rarity)
{
    const int power_level = _get_power_level(power, rarity);

    if (power_level == 0)
    {
        switch (how_mutated() ? random2(3) : 0)
        {
        case 0:
            mutate(RANDOM_MUTATION, "helix card");
            break;
        case 1:
            delete_mutation(RANDOM_MUTATION, "helix card");
            mutate(RANDOM_MUTATION, "helix card");
            break;
        case 2:
            delete_mutation(RANDOM_MUTATION, "helix card");
            break;
        }
    }
    else if (power_level == 1)
    {
        switch (how_mutated() ? random2(3) : 0)
        {
        case 0:
            mutate(coinflip() ? RANDOM_GOOD_MUTATION : RANDOM_MUTATION,
                   "helix card");
            break;
        case 1:
            if (coinflip())
                _remove_bad_mutation();
            else
                delete_mutation(RANDOM_MUTATION, "helix card");
            break;
        case 2:
            if (coinflip())
            {
                if (coinflip())
                {
                    _remove_bad_mutation();
                    mutate(RANDOM_MUTATION, "helix card");
                }
                else
                {
                    delete_mutation(RANDOM_MUTATION, "helix card");
                    mutate(RANDOM_GOOD_MUTATION, "helix card");
                }
            }
            else
            {
                delete_mutation(RANDOM_MUTATION, "helix card");
                mutate(RANDOM_MUTATION, "helix card");
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
            mutate(RANDOM_GOOD_MUTATION, "helix card");
            break;
        case 2:
            if (coinflip())
            {
                // If you get unlucky, you could get here with no bad
                // mutations and simply get a mutation effect. Oh well.
                _remove_bad_mutation();
                mutate(RANDOM_MUTATION, "helix card");
            }
            else
            {
                delete_mutation(RANDOM_MUTATION, "helix card");
                mutate(RANDOM_GOOD_MUTATION, "helix card");
            }
            break;
        }
    }
}

static void _dowsing_card(int power, deck_rarity_type rarity)
{
    const int power_level = _get_power_level(power, rarity);
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
        magic_mapping(random2(power/2) + 18, random2(power*2), false);
    if (things_to_do[1])
    {
        if (detect_traps(random2(power)))
            mpr("You sense traps nearby.");
        if (detect_items(random2(power)))
            mpr("You sense items nearby.");
    }
    if (things_to_do[2])
    {
        you.set_duration(DUR_TELEPATHY, random2(power), 0,
                         "You feel telepathic!");
        detect_creatures(1 + you.duration[DUR_TELEPATHY] * 2 / BASELINE_DELAY,
                         true);
    }
}

// Special case for *your* god, maybe?
static void _godly_wrath()
{
    int tries = 100;
    while (tries-- > 0)
    {
        god_type god = random_god();

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
    const int power_level = _get_power_level(power, rarity);

    mpr("You feel a malignant aura surround you.");
    if (power_level >= 2)
    {
        // Curse (almost) everything.
        // Ignore holy wrath weapons here, to avoid being stuck in a loop
        while (curse_an_item(true) && !one_chance_in(1000))
            ;
    }
    else if (power_level == 1)
    {
        // Curse an average of four items.
        while (curse_an_item(true) && !one_chance_in(4))
            ;
    }
    else
    {
        // Curse 1.5 items on average.
        if (curse_an_item() && coinflip())
            curse_an_item();
    }
}

static void _crusade_card(int power, deck_rarity_type rarity)
{
    const int power_level = _get_power_level(power, rarity);
    if (power_level >= 1)
    {
        // A chance to convert opponents.
        for (monster_near_iterator mi(you.pos(), LOS_NO_TRANS); mi; ++mi)
        {
            if (mi->friendly()
               || mi->holiness() != MH_NATURAL
               || mons_is_unique(mi->type)
               || mons_immune_magic(*mi)
               || player_will_anger_monster(*mi))
            {
                continue;
            }

            // Note that this bypasses the magic resistance
            // (though not immunity) check.  Specifically,
            // you can convert Killer Klowns this way.
            // Might be too good.
            if (mi->get_hit_dice() * 35 < random2(power))
            {
                simple_monster_message(*mi, " is converted.");
                mi->add_ench(ENCH_CHARM);
                mons_att_changed(*mi);
            }
        }
    }
    cast_aura_of_abjuration(power/4);
}

static void _summon_demon_card(int power, deck_rarity_type rarity)
{
    const int power_level = _get_power_level(power, rarity);
    // one demon, and one other demonic creature
    monster_type dct, dct2;
    if (power_level >= 2)
    {
        dct = random_choose(MONS_BALRUG, MONS_BLIZZARD_DEMON,
              MONS_GREEN_DEATH, MONS_SHADOW_DEMON, MONS_CACODEMON,
              MONS_HELL_BEAST, MONS_REAPER, MONS_LOROCYPROCA,
              MONS_HELLION, MONS_TORMENTOR, -1);
        dct2 = MONS_PANDEMONIUM_LORD;
    }
    else if (power_level == 1)
    {
        dct = random_choose(MONS_SUN_DEMON, MONS_ICE_DEVIL,
              MONS_SOUL_EATER, MONS_CHAOS_SPAWN, MONS_SMOKE_DEMON,
              MONS_YNOXINUL, MONS_NEQOXEC, -1);
        dct2 = MONS_RAKSHASA;
    }
    else
    {
        dct = random_choose(MONS_RED_DEVIL, MONS_BLUE_DEVIL,
              MONS_RUST_DEVIL, MONS_HELLWING, MONS_ORANGE_DEMON,
              MONS_SIXFIRHY, -1);
        dct2 = MONS_HELL_HOUND;
    }

    if (is_good_god(you.religion))
    {
        _suppressed_card_message(you.religion, DID_UNHOLY);
        return;
    }

    // FIXME: The manual testing for message printing is there because
    // we can't rely on create_monster() to do it for us. This is
    // because if you are completely surrounded by walls, create_monster()
    // will never manage to give a position which isn't (-1,-1)
    // and thus not print the message.
    // This hack appears later in this file as well.
    if (!create_monster(
            mgen_data(dct, BEH_FRIENDLY, &you,
                      5 - power_level, 0, you.pos(), MHITYOU, MG_AUTOFOE),
            false))
    {
        mpr("You see a puff of smoke.");
    }

    create_monster(
            mgen_data(dct2,
                      BEH_FRIENDLY, &you, 5 - power_level, 0, you.pos(), MHITYOU,
                      MG_AUTOFOE));

}

static void _elements_card(int power, deck_rarity_type rarity)
{

    const int power_level = _get_power_level(power, rarity);
    const monster_type element_list[][3] =
    {
       {MONS_RAIJU, MONS_WIND_DRAKE, MONS_SHOCK_SERPENT},
       {MONS_BASILISK, MONS_BORING_BEETLE, MONS_BOULDER_BEETLE},
       {MONS_MOTTLED_DRAGON, MONS_MOLTEN_GARGOYLE, MONS_SALAMANDER_FIREBRAND},
       {MONS_ICE_BEAST, MONS_POLAR_BEAR, MONS_ICE_DRAGON}
    };

    int start = random2(ARRAYSZ(element_list));

    for (int i = 0; i < 3; ++i)
    {
        monster_type mons_type = element_list[start % ARRAYSZ(element_list)][power_level];
        monster hackmon;

        hackmon.type = mons_type;
        mons_load_spells(&hackmon);

        if (you_worship(GOD_DITHMENOS) && mons_is_fiery(&hackmon))
        {
            _suppressed_card_message(you.religion, DID_FIRE);
            start++;
            continue;
        }

        create_monster(
            mgen_data(mons_type, BEH_FRIENDLY, &you, power_level + 2, 0,
                      you.pos(), MHITYOU, MG_AUTOFOE));
        start++;
    }

}

static void _summon_dancing_weapon(int power, deck_rarity_type rarity)
{
    const int power_level = _get_power_level(power, rarity);

    monster *mon =
        create_monster(
            mgen_data(MONS_DANCING_WEAPON, BEH_FRIENDLY, &you,
                      power_level + 2, 0, you.pos(), MHITYOU, MG_AUTOFOE),
            false);

    // Given the abundance of Nemelex decks, not setting hard reset
    // leaves a trail of weapons behind, most of which just get
    // offered to Nemelex again, adding an unnecessary source of
    // piety.
    // This is of course irrelevant now that Nemelex sacrifices
    // are gone.
    if (mon)
    {
        // Override the weapon.
        ASSERT(mon->weapon() != NULL);
        item_def& wpn(*mon->weapon());

        if (power_level == 0)
        {
            // Wimpy, negative-enchantment weapon.
            wpn.plus = random2(3) - 2;
            wpn.sub_type = (coinflip() ? WPN_QUARTERSTAFF : WPN_HAND_AXE);

            set_item_ego_type(wpn, OBJ_WEAPONS,
                              coinflip() ? SPWPN_VENOM : SPWPN_NORMAL);
        }
        else if (power_level == 1)
        {
            // This is getting good.
            wpn.plus = random2(4) - 1;
            wpn.sub_type = (coinflip() ? WPN_LONG_SWORD : WPN_TRIDENT);

            if (coinflip())
            {
                set_item_ego_type(wpn, OBJ_WEAPONS,
                                  coinflip() ? SPWPN_FLAMING : SPWPN_FREEZING);
            }
            else
                set_item_ego_type(wpn, OBJ_WEAPONS, SPWPN_NORMAL);
        }
        else if (power_level == 2)
        {
            // Rare and powerful.
            wpn.plus = random2(4) + 2;
            wpn.sub_type = (coinflip() ? WPN_DEMON_TRIDENT : WPN_EXECUTIONERS_AXE);

            set_item_ego_type(wpn, OBJ_WEAPONS,
                              coinflip() ? SPWPN_SPEED : SPWPN_ELECTROCUTION);
        }

        item_colour(wpn);

        // sometimes give a randart instead
        if (one_chance_in(3))
        {
            make_item_randart(wpn, true);
            set_ident_flags(wpn, ISFLAG_KNOW_PROPERTIES| ISFLAG_KNOW_TYPE);
        }

        mon->flags |= MF_HARD_RESET;

        ghost_demon newstats;
        newstats.init_dancing_weapon(wpn, power / 4);

        mon->set_ghost(newstats);
        mon->ghost_demon_init();
    }
    else
        mpr("You see a puff of smoke.");
}

static void _summon_flying(int power, deck_rarity_type rarity)
{
    const int power_level = _get_power_level(power, rarity);

    const monster_type flytypes[] =
    {
        MONS_INSUBSTANTIAL_WISP, MONS_KILLER_BEE, MONS_RAVEN,
        MONS_VAMPIRE_MOSQUITO, MONS_YELLOW_WASP, MONS_RED_WASP
    };
    const int num_flytypes = ARRAYSZ(flytypes);

    // Choose what kind of monster.
    monster_type result;
    const int how_many = 2 + random2(3) + power_level * 3;
    bool hostile_invis = false;

    do
    {
        result = flytypes[random2(num_flytypes - 2) + power_level];
    }
    while (is_good_god(you.religion) && result == MONS_VAMPIRE_MOSQUITO);

    for (int i = 0; i < how_many; ++i)
    {
        const bool friendly = !one_chance_in(power_level + 4);

        create_monster(
            mgen_data(result,
                      friendly ? BEH_FRIENDLY : BEH_HOSTILE, &you,
                      3, 0, you.pos(), MHITYOU, MG_AUTOFOE));

        if (mons_class_flag(result, M_INVIS) && !you.can_see_invisible() && !friendly)
            hostile_invis = true;
    }

    if (hostile_invis)
        mpr("You sense the presence of something unfriendly.");
}

static void _summon_rangers(int power, deck_rarity_type rarity)
{
    const int power_level = _get_power_level(power, rarity);
    monster_type dctr, dctr2, dctr3, dctr4;
    monster_type base_choice, big_choice, mid_choice, placed_choice;
    dctr = random_choose(MONS_CENTAUR, MONS_YAKTAUR, -1);
    dctr2 = random_choose(MONS_CENTAUR_WARRIOR, MONS_FAUN, -1);
    dctr3 = random_choose(MONS_YAKTAUR_CAPTAIN, MONS_NAGA_SHARPSHOOTER, -1);
    dctr4 = random_choose(MONS_SATYR, MONS_MERFOLK_JAVELINEER,
                          MONS_DEEP_ELF_MASTER_ARCHER, -1);
    const int launch_count = 1 + random2(2);

    if (power_level >= 2)
    {
        base_choice = dctr2;
        big_choice  = dctr4;
        mid_choice  = dctr3;
    }
    else
    {
        base_choice = dctr;
        {
            if (power_level == 1)
            {
                big_choice  = dctr3;
                mid_choice  = dctr2;
            }
            else
            {
                big_choice  = dctr;
                mid_choice  = dctr;
            }
        }
    }

    if (launch_count < 2)
        placed_choice = big_choice;
    else
        placed_choice = mid_choice;

    for (int i = 0; i < launch_count; ++i)
    {
        create_monster(
            mgen_data(base_choice,
                      BEH_FRIENDLY, &you, 5 - power_level, 0, you.pos(), MHITYOU,
                      MG_AUTOFOE));
    }

    create_monster(
        mgen_data(placed_choice,
                  BEH_FRIENDLY, &you, 5 - power_level, 0, you.pos(), MHITYOU,
                  MG_AUTOFOE));

}

static void _summon_ugly(int power, deck_rarity_type rarity)
{
    const int power_level = _get_power_level(power, rarity);
    const bool friendly = !one_chance_in(4 + power_level * 2);
    monster_type ugly;
    if (power_level >= 1)
        ugly = MONS_VERY_UGLY_THING;
    else
        ugly = MONS_UGLY_THING;

    // TODO: Handle Dithmenos and red uglies

    if (you_worship(GOD_ZIN))
    {
        _suppressed_card_message(you.religion, DID_CHAOS);
        return;
    }

    if (!create_monster(mgen_data(ugly,
                                  friendly ? BEH_FRIENDLY : BEH_HOSTILE,
                                  &you,
                                  min(power/50 + 1, 5), 0,
                                  you.pos(), MHITYOU, MG_AUTOFOE),
                        false))
    {
        mpr("You see a puff of smoke.");
    }

    if (power_level == 2)
    {
        create_monster(mgen_data(MONS_UGLY_THING,
                        friendly ? BEH_FRIENDLY : BEH_HOSTILE,
                        &you,
                        min(power/50 + 1, 5), 0,
                        you.pos(), MHITYOU, MG_AUTOFOE));
    }

}

static void _mercenary_card(int power, deck_rarity_type rarity)
{
    const int power_level = _get_power_level(power, rarity);
    const monster_type merctypes[] =
    {
        MONS_BIG_KOBOLD, MONS_MERFOLK, MONS_NAGA,
        MONS_TENGU, MONS_DEEP_ELF_CONJURER, MONS_ORC_KNIGHT,
        RANDOM_BASE_DEMONSPAWN, MONS_OGRE_MAGE, MONS_MINOTAUR,
        RANDOM_BASE_DRACONIAN, MONS_DEEP_ELF_BLADEMASTER,
    };

    int merc;
    monster *mon;

    while (1)
    {
        merc = power_level + random2(3 * (power_level + 1));
        ASSERT(merc < (int)ARRAYSZ(merctypes));

        mgen_data mg(merctypes[merc], BEH_HOSTILE, &you,
                    0, 0, you.pos(), MHITYOU, MG_FORCE_BEH, you.religion);

        mg.extra_flags |= (MF_NO_REWARD | MF_HARD_RESET);

        // This is a bit of a hack to use give_monster_proper_name to feed
        // the mgen_data, but it gets the job done.
        monster tempmon;
        tempmon.type = merctypes[merc];
        if (give_monster_proper_name(&tempmon, false))
            mg.mname = tempmon.mname;
        else
            mg.mname = make_name(random_int(), false);
        // This is used for giving the merc better stuff in mon-gear.
        mg.props["mercenary items"] = true;

        mon = create_monster(mg);

        if (!mon)
        {
            mpr("You see a puff of smoke.");
            return;
        }

        if (player_will_anger_monster(mon))
        {
            dprf("God %s doesn't like %s, retrying.",
            god_name(you.religion).c_str(), mon->name(DESC_THE).c_str());
            monster_die(mon, KILL_RESET, NON_MONSTER);
            continue;
        }
        else
            break;
    }

    mon->props["dbname"].get_string() = mons_class_name(merctypes[merc]);

    redraw_screen(); // We want to see the monster while it's asking to be paid.

    const int fee = fuzz_value(exper_value(mon), 15, 15);
    if (fee > you.gold)
    {
        mprf("You cannot afford %s fee of %d gold!",
             mon->name(DESC_ITS).c_str(), fee);
        simple_monster_message(mon, " attacks!");
        return;
    }

    mon->props["mercenary_fee"] = fee;
    run_uncancel(UNC_MERCENARY, mon->mid);
}

bool recruit_mercenary(int mid)
{
    monster *mon = monster_by_mid(mid);
    if (!mon)
        return true; // wut?

    int fee = mon->props["mercenary_fee"].get_int();
    const string prompt = make_stringf("Pay %s fee of %d gold?",
                                       mon->name(DESC_ITS).c_str(), fee);
    bool paid = yesno(prompt.c_str(), false, 0);
    const string message = make_stringf("Hired %s for %d gold.",
                                        mon->full_name(DESC_A).c_str(), fee);
    if (crawl_state.seen_hups)
        return false;

    mon->props.erase("mercenary_fee");
    if (!paid)
    {
        simple_monster_message(mon, " attacks!");
        return true;
    }

    simple_monster_message(mon, " joins your ranks!");
    mon->attitude = ATT_FRIENDLY;
    mons_att_changed(mon);
    take_note(Note(NOTE_MESSAGE, 0, 0, message.c_str()), true);
    you.del_gold(fee);
    return true;
}

static void _alchemist_card(int power, deck_rarity_type rarity)
{
    const int power_level = _get_power_level(power, rarity);
    const int gold_max = min(you.gold, random2avg(100, 2) * (1 + power_level));
    int gold_used = 0;

    dprf("%d gold available to spend.", gold_max);

    // Spend some gold to regain health.
    int hp = min((gold_max - gold_used) / 3, you.hp_max - you.hp);
    if (hp > 0)
    {
        you.del_gold(hp * 2);
        inc_hp(hp);
        gold_used += hp * 2;
        mpr("You feel better.");
        dprf("Gained %d health, %d gold remaining.", hp, gold_max - gold_used);
    }
    // Maybe spend some more gold to regain magic.
    int mp = min((gold_max - gold_used) / 5,
                 you.max_magic_points - you.magic_points);
    if (mp > 0 && x_chance_in_y(power_level + 1, 5))
    {
        you.del_gold(mp * 5);
        inc_mp(mp);
        gold_used += mp * 5;
        mpr("You feel your power returning.");
        dprf("Gained %d magic, %d gold remaining.", mp, gold_max - gold_used);
    }

    if (gold_used > 0)
        mprf("%d of your gold pieces vanish!", gold_used);
    else
        canned_msg(MSG_NOTHING_HAPPENS);
}

static void _cloud_card(int power, deck_rarity_type rarity)
{
    const int power_level = _get_power_level(power, rarity);
    bool something_happened = false;

    for (radius_iterator di(you.pos(), LOS_NO_TRANS); di; ++di)
    {
        monster *mons = monster_at(*di);
        cloud_type cloudy;

        switch (power_level)
        {
            case 0: cloudy = (you_worship(GOD_SHINING_ONE) || !one_chance_in(5))
                              ? CLOUD_MEPHITIC : CLOUD_POISON;
                    break;

            case 1: cloudy = (you_worship(GOD_DITHMENOS) || coinflip())
                              ? CLOUD_COLD : CLOUD_FIRE;
                     break;

            case 2: cloudy = (is_good_god(you.religion) || coinflip())
                              ? CLOUD_ACID: CLOUD_MIASMA;
                    break;

            default: cloudy = CLOUD_DEBUGGING;
        }

        if (!mons || (mons && (mons->wont_attack()
            || mons_is_firewood(mons))))
        {
            continue;
        }

        for (adjacent_iterator ai(mons->pos()); ai; ++ai)
        {
            // don't place clouds on the player or monsters
            if (*ai == you.pos() || monster_at(*ai))
                continue;

            if (grd(*ai) == DNGN_FLOOR && env.cgrid(*ai) == EMPTY_CLOUD)
            {
                const int cloud_power = 5 + random2((power_level + 1) * 3);
                place_cloud(cloudy, *ai, cloud_power, &you);

                if (you.see_cell(*ai))
                something_happened = true;
            }
        }
    }

    if (something_happened)
        mpr("Clouds appear around you!");
    else
        canned_msg(MSG_NOTHING_HAPPENS);
}

static void _fortitude_card(int power, deck_rarity_type rarity)
{
    const int power_level = _get_power_level(power, rarity);
    const bool strong = you.duration[DUR_FORTITUDE] > 0;

    you.increase_duration(DUR_FORTITUDE, 10 + random2((power_level + 1) * 10));

    if (!strong)
    {
        mprf(MSGCH_DURATION, "You are filled with a great fortitude.");
        notify_stat_change(STAT_STR, 10, true, "");
    }
    else
        mprf(MSGCH_DURATION, "You become more resolute.");
}

static void _storm_card(int power, deck_rarity_type rarity)
{
    const int power_level = _get_power_level(power, rarity);

    if (coinflip())
    {
        const int num_to_summ = 1 + random2(1 + power_level);
        for (int i = 0; i < num_to_summ; ++i)
        {
            create_monster(
                    mgen_data(MONS_AIR_ELEMENTAL,
                              BEH_FRIENDLY, &you, 3, 0, you.pos(), MHITYOU,
                              MG_AUTOFOE));
        }
    }
    else
    {
        wind_blast(&you, (power_level == 0) ? 100 : 200, coord_def(), true);

        for (radius_iterator ri(you.pos(), 5, C_ROUND, LOS_SOLID); ri; ++ri)
        {
            monster *mons = monster_at(*ri);

            if (adjacent(*ri, you.pos()))
                continue;

            if (mons && mons->wont_attack())
                continue;


            if ((feat_has_solid_floor(grd(*ri))
                 || grd(*ri) == DNGN_DEEP_WATER)
                && env.cgrid(*ri) == EMPTY_CLOUD)
            {
                place_cloud(CLOUD_STORM, *ri,
                            5 + (power_level + 1) * random2(10), & you);
            }
        }
    }

    if (power_level > 1 || (power_level == 1 && coinflip()))
    {
        if (coinflip())
        {
            coord_def pos;
            int tries = 0;

            do
            {
                random_near_space(&you, you.pos(), pos, true);
                tries++;
            }
            while (distance2(pos, you.pos()) < 3 && tries < 50);

            if (tries > 50)
                pos = you.pos();


            if (create_monster(
                        mgen_data(MONS_TWISTER,
                                  BEH_HOSTILE, &you, 1 + random2(power_level + 1),
                                  0, pos, MHITYOU, MG_FORCE_PLACE)))
            {
                mpr("A tornado forms.");
            }
        }
        else
        {
            // create some water so the wellspring can place
            create_feat_splash(you.pos(), 2, 3);
            create_monster(
                    mgen_data(MONS_ELEMENTAL_WELLSPRING,
                              BEH_FRIENDLY, &you, 3, 0, you.pos(), MHITYOU,
                              MG_AUTOFOE));
        }
    }
 }

static void _illusion_card(int power, deck_rarity_type rarity)
{
    const int power_level = _get_power_level(power, rarity);
    monster* mon = get_free_monster();

    if (!mon || monster_at(you.pos()))
        return;

    mon->type = MONS_PLAYER;
    mon->behaviour = BEH_SEEK;
    mon->attitude = ATT_FRIENDLY;
    mon->set_position(you.pos());
    mon->mid = MID_PLAYER;
    mgrd(you.pos()) = mon->mindex();

    mons_summon_illusion_from(mon, (actor *)&you, SPELL_NO_SPELL, power_level);
    mon->reset();
 }

static void _degeneration_card(int power, deck_rarity_type rarity)
{
    const int power_level = _get_power_level(power, rarity);
    bool effects = false;

    if (you_worship(GOD_ZIN))
    {
        _suppressed_card_message(you.religion, DID_CHAOS);
        return;
    }

    for (radius_iterator di(you.pos(), LOS_NO_TRANS); di; ++di)
    {
        monster *mons = monster_at(*di);

        if (mons && (mons->wont_attack() || !mons->can_polymorph()
            || mons_is_firewood(mons)))
        {
            continue;
        }

        if (mons &&
            x_chance_in_y((power_level + 1) * 5 + random2(5),
                          mons->get_hit_dice()))
        {
            effects = true;
            monster_polymorph(mons, RANDOM_MONSTER, PPT_LESS);
            mons->malmutate("");
        }
    }

    if (!effects)
        canned_msg(MSG_NOTHING_HAPPENS);
}

static void _placid_magic_card(int power, deck_rarity_type rarity)
{
    const int power_level = _get_power_level(power, rarity);
    const int drain = max(you.magic_points - random2(power_level * 3), 0);

    mpr("You feel magic draining away.");

    drain_mp(drain);
    debuff_player();

    for (radius_iterator di(you.pos(), LOS_NO_TRANS); di; ++di)
    {
        monster *mons = monster_at(*di);

        if (!mons || mons->wont_attack())
            continue;

        debuff_monster(mons);
        if (!mons->antimagic_susceptible())
            continue;

        // XXX: this should be refactored together with other effects that
        // apply antimagic.
        const int duration = random2(div_rand_round(power / 3,
                                                    mons->get_hit_dice()))
                             * BASELINE_DELAY;
        mons->add_ench(mon_enchant(ENCH_ANTIMAGIC, 0, &you, duration));
        mprf("%s magic leaks into the air.",
             apostrophise(mons->name(DESC_THE)).c_str());
    }
}

// Punishment cards don't have their power adjusted depending on Nemelex piety
// or penance, and are based on experience level instead of evocations skill
// for more appropriate scaling.
static int _card_power(deck_rarity_type rarity, bool punishment)
{
    int result = 0;

    if (!punishment)
    {
        if (player_under_penance(GOD_NEMELEX_XOBEH))
            result -= you.penance[GOD_NEMELEX_XOBEH];
        else if (you_worship(GOD_NEMELEX_XOBEH))
        {
            result = you.piety;
            result *= (you.skill(SK_EVOCATIONS, 100) + 2500);
            result /= 2700;
        }
    }

    result += (punishment) ? you.experience_level * 18
                           : you.skill(SK_EVOCATIONS, 9);

    if (rarity == DECK_RARITY_RARE)
        result += 150;
    else if (rarity == DECK_RARITY_LEGENDARY)
        result += 300;

    if (result < 0)
        result = 0;

    return result;
}

void card_effect(card_type which_card, deck_rarity_type rarity,
                 uint8_t flags, bool tell_card)
{
    ASSERT(!_card_forbidden(which_card));

    const char *participle = (flags & CFLAG_DEALT) ? "dealt" : "drawn";
    const int power = _card_power(rarity, flags & CFLAG_PUNISHMENT);

    const god_type god =
        (crawl_state.is_god_acting()) ? crawl_state.which_god_acting()
                                      : GOD_NO_GOD;

    dprf("Card power: %d, rarity: %d", power, rarity);

    if (tell_card)
    {
        // These card types will usually give this message in the targeting
        // prompt, and the cases where they don't are handled specially.
        if (which_card != CARD_VITRIOL && which_card != CARD_HAMMER
            && which_card != CARD_PAIN && which_card != CARD_VENOM
            && which_card != CARD_ORB)
        {
            mprf("You have %s %s.", participle, card_name(which_card));
        }
    }

    if (which_card == CARD_XOM && !crawl_state.is_god_acting())
    {
        if (you_worship(GOD_XOM))
        {
            // Being a self-centered deity, Xom *always* finds this
            // maximally hilarious.
            god_speaks(GOD_XOM, "Xom roars with laughter!");
            you.gift_timeout = 200;
        }
        else if (player_under_penance(GOD_XOM))
            god_speaks(GOD_XOM, "Xom laughs nastily.");
    }

    switch (which_card)
    {
    case CARD_SWAP:             _swap_monster_card(power, rarity); break;
    case CARD_VELOCITY:         _velocity_card(power, rarity); break;
    case CARD_DAMNATION:        _damnation_card(power, rarity); break;
    case CARD_SOLITUDE:         cast_dispersal(power/4); break;
    case CARD_ELIXIR:           _elixir_card(power, rarity); break;
    case CARD_HELM:             _helm_card(power, rarity); break;
    case CARD_BLADE:            _blade_card(power, rarity); break;
    case CARD_SHADOW:           _shadow_card(power, rarity); break;
    case CARD_POTION:           _potion_card(power, rarity); break;
    case CARD_FOCUS:            _focus_card(power, rarity); break;
    case CARD_HELIX:            _helix_card(power, rarity); break;
    case CARD_DOWSING:          _dowsing_card(power, rarity); break;
    case CARD_STAIRS:           _stairs_card(power, rarity); break;
    case CARD_CURSE:            _curse_card(power, rarity); break;
    case CARD_WARPWRIGHT:       _warpwright_card(power, rarity); break;
    case CARD_SHAFT:            _shaft_card(power, rarity); break;
    case CARD_TOMB:             entomb(10 + power/20 + random2(power/4)); break;
    case CARD_WRAITH:           drain_player(power / 4, false); break;
    case CARD_WRATH:            _godly_wrath(); break;
    case CARD_CRUSADE:          _crusade_card(power, rarity); break;
    case CARD_SUMMON_DEMON:     _summon_demon_card(power, rarity); break;
    case CARD_ELEMENTS:         _elements_card(power, rarity); break;
    case CARD_RANGERS:          _summon_rangers(power, rarity); break;
    case CARD_SUMMON_WEAPON:    _summon_dancing_weapon(power, rarity); break;
    case CARD_SUMMON_FLYING:    _summon_flying(power, rarity); break;
    case CARD_SUMMON_UGLY:      _summon_ugly(power, rarity); break;
    case CARD_XOM:              xom_acts(5 + random2(power/10)); break;
    case CARD_BANSHEE:          _banshee_card(power, rarity); break;
    case CARD_TORMENT:          torment(&you, TORMENT_CARDS, you.pos()); break;
    case CARD_ALCHEMIST:        _alchemist_card(power, rarity); break;
    case CARD_MERCENARY:        _mercenary_card(power, rarity); break;
    case CARD_CLOUD:            _cloud_card(power, rarity); break;
    case CARD_FORTITUDE:        _fortitude_card(power, rarity); break;
    case CARD_STORM:            _storm_card(power, rarity); break;
    case CARD_ILLUSION:         _illusion_card(power, rarity); break;
    case CARD_DEGEN:            _degeneration_card(power, rarity); break;
    case CARD_PLACID_MAGIC:     _placid_magic_card(power, rarity); break;

    case CARD_VENOM:
    case CARD_VITRIOL:
    case CARD_HAMMER:
    case CARD_PAIN:
    case CARD_ORB:
        _damaging_card(which_card, power, rarity, flags & CFLAG_DEALT);
        break;

    case CARD_WILD_MAGIC:
        // Yes, high power is bad here.
        MiscastEffect(&you, &you,
                      god == GOD_NO_GOD ? MISC_MISCAST : GOD_MISCAST + god,
                      SPTYP_RANDOM, random2(power/15) + 5, random2(power),
                      "a card of wild magic");
        break;

    case CARD_FAMINE:
        if (you_foodless())
            mpr("You feel rather smug.");
        else
            set_hunger(min(you.hunger, HUNGER_STARVING / 2), true);
        break;

    case CARD_FEAST:
        if (you_foodless())
            mpr("You feel a horrible emptiness.");
        else
            set_hunger(HUNGER_MAXIMUM, true);
        break;

    case CARD_SWINE:
        if (!transform(5 + power/10 + random2(power/10), TRAN_PIG, true))
        {
            mpr("You feel like a pig.");
            break;
        }
        break;

#if TAG_MAJOR_VERSION == 34
    case CARD_SHUFFLE:
    case CARD_EXPERIENCE:
    case CARD_SAGE:
    case CARD_WATER:
    case CARD_GLASS:
    case CARD_TROWEL:
    case CARD_MINEFIELD:
    case CARD_PORTAL:
    case CARD_WARP:
    case CARD_GENIE:
    case CARD_BATTLELUST:
    case CARD_BARGAIN:
    case CARD_METAMORPHOSIS:
    case CARD_SUMMON_ANIMAL:
    case CARD_SUMMON_SKELETON:
        mpr("This type of card no longer exists!");
        break;
#endif

    case NUM_CARDS:
        // The compiler will complain if any card remains unhandled.
        mprf("You have %s a buggy card!", participle);
        break;
    }
}

bool top_card_is_known(const item_def &deck)
{
    if (!is_deck(deck))
        return false;

    uint8_t flags;
    get_card_and_flags(deck, -1, flags);

    return flags & CFLAG_MARKED;
}

card_type top_card(const item_def &deck)
{
    if (!is_deck(deck))
        return NUM_CARDS;

    uint8_t flags;
    card_type card = get_card_and_flags(deck, -1, flags);

    UNUSED(flags);

    return card;
}

bool is_deck(const item_def &item)
{
    return item.base_type == OBJ_MISCELLANY
           && item.sub_type >= MISC_FIRST_DECK
           && item.sub_type <= MISC_LAST_DECK;
}

bool bad_deck(const item_def &item)
{
    if (!is_deck(item))
        return false;

    return !item.props.exists("cards")
           || item.props["cards"].get_type() != SV_VEC
           || item.props["cards"].get_vector().get_type() != SV_BYTE
           || cards_in_deck(item) == 0;
}

colour_t deck_rarity_to_colour(deck_rarity_type rarity)
{
    switch (rarity)
    {
    case DECK_RARITY_COMMON:
        return GREEN;

    case DECK_RARITY_RARE:
        return MAGENTA;

    case DECK_RARITY_LEGENDARY:
        return LIGHTMAGENTA;

    case DECK_RARITY_RANDOM:
        die("unset deck rarity");
    }

    return WHITE;
}

void init_deck(item_def &item)
{
    CrawlHashTable &props = item.props;

    ASSERT(is_deck(item));
    ASSERT(!props.exists("cards"));
    ASSERT_RANGE(item.initial_cards, 1, 128);
    ASSERT(item.special >= DECK_RARITY_COMMON
           && item.special <= DECK_RARITY_LEGENDARY);

    const store_flags fl = SFLAG_CONST_TYPE;

    props["cards"].new_vector(SV_BYTE, fl).resize((vec_size)
                                                  item.initial_cards);
    props["card_flags"].new_vector(SV_BYTE, fl).resize((vec_size)
                                                       item.initial_cards);
    props["drawn_cards"].new_vector(SV_BYTE, fl);

    for (int i = 0; i < item.initial_cards; ++i)
    {
        bool      was_odd = false;
        card_type card    = _random_card(item, was_odd);

        uint8_t flags = 0;
        if (was_odd)
            flags = CFLAG_ODDITY;

        _set_card_and_flags(item, i, card, flags);
    }

    ASSERT(cards_in_deck(item) == item.initial_cards);

    props["num_marked"]        = (char) 0;

    props.assert_validity();

    item.used_count  = 0;
}

static void _unmark_deck(item_def& deck)
{
    if (!is_deck(deck))
        return;

    CrawlHashTable &props = deck.props;
    if (!props.exists("card_flags"))
        return;

    CrawlVector &flags = props["card_flags"].get_vector();

    for (unsigned int i = 0; i < flags.size(); ++i)
    {
        flags[i] =
            static_cast<char>((static_cast<char>(flags[i]) & ~CFLAG_MARKED));
    }

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

void shuffle_all_decks_on_level()
{
    for (int i = 0; i < MAX_ITEMS; ++i)
    {
        item_def& item(mitm[i]);
        if (item.defined() && is_deck(item))
        {
#ifdef DEBUG_DIAGNOSTICS
            mprf(MSGCH_DIAGNOSTICS, "Shuffling: %s on %s",
                 item.name(DESC_PLAIN).c_str(),
                 level_id::current().describe().c_str());
#endif
            _unmark_and_shuffle_deck(item);
        }
    }
}

static bool _shuffle_inventory_decks()
{
    bool success = false;

    for (int i = 0; i < ENDOFPACK; ++i)
    {
        item_def& item(you.inv[i]);
        if (item.defined() && is_deck(item))
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
    add_daction(DACT_SHUFFLE_DECKS);
    _shuffle_inventory_decks();

    // Wildly inaccurate, but of similar quality as the old code which
    // was triggered by the presence of any deck anywhere.
    if (you.num_total_gifts[GOD_NEMELEX_XOBEH])
        god_speaks(GOD_NEMELEX_XOBEH, "You hear Nemelex Xobeh chuckle.");
}
