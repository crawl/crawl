/**
 * @file
 * @brief Functions with decks of cards.
**/

#include "AppHdr.h"

#include "decks.h"

#include <algorithm>
#include <iostream>
#include <sstream>

#include "abyss.h"
#include "act-iter.h"
#include "artefact.h"
#include "attitude-change.h"
#include "cloud.h"
#include "coordit.h"
#include "dactions.h"
#include "database.h"
#include "directn.h"
#include "dungeon.h"
#include "english.h"
#include "evoke.h"
#include "food.h"
#include "ghost.h"
#include "god-passive.h" // passive_t::no_haste
#include "god-wrath.h"
#include "invent.h"
#include "item-prop.h"
#include "item-status-flag-type.h"
#include "items.h"
#include "item-use.h"
#include "libutil.h"
#include "macro.h"
#include "message.h"
#include "mon-cast.h"
#include "mon-clone.h"
#include "mon-death.h"
#include "mon-place.h"
#include "mon-poly.h"
#include "mon-project.h"
#include "mon-tentacle.h"
#include "mon-util.h"
#include "mutation.h"
#include "nearby-danger.h"
#include "notes.h"
#include "output.h"
#include "player-equip.h"
#include "player-stats.h"
#include "potion.h"
#include "prompt.h"
#include "random.h"
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

// For information on deck structure, reference the comment near the beginning

#define STACKED_KEY "stacked"
// 'deck is stacked' prop

static void _deck_ident(item_def& deck);

struct card_with_weights
{
    card_type card;
    int weight[3];
};

typedef vector<card_with_weights> deck_archetype;

deck_archetype deck_of_escape =
{
    { CARD_TOMB,       {5, 5, 5} },
    { CARD_EXILE,      {0, 1, 2} },
    { CARD_ELIXIR,     {5, 5, 5} },
    { CARD_CLOUD,      {5, 5, 5} },
    { CARD_VELOCITY,   {5, 5, 5} },
    { CARD_SHAFT,      {5, 5, 5} },
};

deck_archetype deck_of_destruction =
{
    { CARD_VITRIOL,    {5, 5, 5} },
    { CARD_STORM,      {5, 5, 5} },
    { CARD_PAIN,       {5, 5, 3} },
    { CARD_ORB,        {5, 5, 5} },
    { CARD_DEGEN,      {3, 3, 3} },
    { CARD_WILD_MAGIC, {5, 5, 3} },
};

deck_archetype deck_of_summoning =
{
    { CARD_ELEMENTS,        {5, 5, 5} },
    { CARD_SUMMON_DEMON,    {5, 5, 5} },
    { CARD_SUMMON_WEAPON,   {5, 5, 5} },
    { CARD_SUMMON_FLYING,   {5, 5, 5} },
    { CARD_RANGERS,         {5, 5, 5} },
    { CARD_ILLUSION,        {5, 5, 5} },
};

deck_archetype deck_of_punishment =
{
    { CARD_WRAITH,     {5, 5, 5} },
    { CARD_WRATH,      {5, 5, 5} },
    { CARD_FAMINE,     {5, 5, 5} },
    { CARD_SWINE,      {5, 5, 5} },
    { CARD_TORMENT,    {5, 5, 5} },
};

#if TAG_MAJOR_VERSION == 34
deck_archetype removed_deck =
{
    { CARD_XOM, {5, 5, 5} },
};
#endif

struct deck_type_data
{
    /// The name of the deck. (Doesn't include "deck of ".)
    string name;
    /// The list of cards this deck contains.
    const deck_archetype* cards;
};

static map<misc_item_type, deck_type_data> all_decks =
{
    { MISC_DECK_OF_ESCAPE, {
        "escape", &deck_of_escape,
    } },
    { MISC_DECK_OF_DESTRUCTION, {
        "destruction", &deck_of_destruction,
    } },
#if TAG_MAJOR_VERSION == 34
    { MISC_DECK_OF_DUNGEONS, {
        "dungeons", &removed_deck,
    } },
#endif
    { MISC_DECK_OF_SUMMONING, {
        "summoning", &deck_of_summoning,
    } },
#if TAG_MAJOR_VERSION == 34
    { MISC_DECK_OF_WONDERS, {
        "wonders", &removed_deck,
    } },
    { MISC_DECK_OF_ODDITIES, {
        "oddities", &removed_deck,
    } },
#endif
    { MISC_DECK_OF_PUNISHMENT, {
        "punishment", &deck_of_punishment,
    } },
#if TAG_MAJOR_VERSION == 34
    { MISC_DECK_OF_WAR, {
        "war", &removed_deck,
    } },
    { MISC_DECK_OF_CHANGES, {
        "changes", &removed_deck,
    } },
    { MISC_DECK_OF_DEFENCE, {
        "defence", &removed_deck,
    } },
#endif
};

int cards_in_deck(const item_def &deck)
{
    ASSERT(is_deck(deck));

    const CrawlHashTable &props = deck.props;
    ASSERT(props.exists(CARD_KEY));

    return props[CARD_KEY].get_vector().size();
}

card_type get_card_and_flags(const item_def& deck, int idx,
                             uint8_t& _flags)
{
    const CrawlHashTable &props = deck.props;
    const CrawlVector    &cards = props[CARD_KEY].get_vector();
    const CrawlVector    &flags = props[CARD_FLAG_KEY].get_vector();

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
    CrawlVector    &cards = props[CARD_KEY].get_vector();
    CrawlVector    &flags = props[CARD_FLAG_KEY].get_vector();

    if (idx < 0)
        idx = static_cast<int>(cards.size()) + idx;

    cards[idx].get_byte() = card;
    flags[idx].get_byte() = _flags;
}

const char* card_name(card_type card)
{
    switch (card)
    {
    case CARD_VELOCITY:        return "Velocity";
    case CARD_EXILE:           return "Exile";
    case CARD_ELIXIR:          return "the Elixir";
    case CARD_STAIRS:          return "the Stairs";
    case CARD_TOMB:            return "the Tomb";
    case CARD_WILD_MAGIC:      return "Wild Magic";
    case CARD_ELEMENTS:        return "the Elements";
    case CARD_SUMMON_DEMON:    return "the Pentagram";
    case CARD_SUMMON_WEAPON:   return "the Dance";
    case CARD_SUMMON_FLYING:   return "Foxfire";
    case CARD_RANGERS:         return "the Rangers";
    case CARD_SHAFT:           return "the Shaft";
    case CARD_VITRIOL:         return "Vitriol";
    case CARD_CLOUD:           return "the Cloud";
    case CARD_STORM:           return "the Storm";
    case CARD_PAIN:            return "Pain";
    case CARD_TORMENT:         return "Torment";
    case CARD_WRATH:           return "Wrath";
    case CARD_WRAITH:          return "the Wraith";
    case CARD_SWINE:           return "the Swine";
    case CARD_ORB:             return "the Orb";
    case CARD_ILLUSION:        return "the Illusion";
    case CARD_DEGEN:           return "Degeneration";
    case CARD_FAMINE:          return "Famine";

#if TAG_MAJOR_VERSION == 34
    // Removed cards.
    case CARD_MERCENARY:       return "the Mercenary";
    case CARD_ALCHEMIST:       return "the Alchemist";
    case CARD_CURSE:           return "the Curse";
    case CARD_VENOM:           return "Venom";
    case CARD_FORTITUDE:       return "Fortitude";
    case CARD_HAMMER:          return "the Hammer";
    case CARD_XOM:             return "Xom";
    case CARD_FEAST:           return "the Feast";
    case CARD_WARPWRIGHT:      return "Warpwright";
    case CARD_SUMMON_UGLY:     return "Repulsiveness";
    case CARD_PLACID_MAGIC:    return "Placid Magic";
    case CARD_CRUSADE:         return "the Crusade";
    case CARD_HELM:            return "the Helm";
    case CARD_BLADE:           return "the Blade";
    case CARD_SHADOW:          return "the Shadow";
    case CARD_POTION:          return "the Potion";
    case CARD_FOCUS:           return "Focus";
    case CARD_HELIX:           return "the Helix";
    case CARD_DOWSING:         return "Dowsing";
    case CARD_BANSHEE:         return "the Banshee";
    case CARD_SOLITUDE:        return "Solitude";
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
    case CARD_GLASS:           return "Vitrification";
    case CARD_BARGAIN:         return "the Bargain";
    case CARD_SUMMON_ANIMAL:   return "the Herd";
    case CARD_SUMMON_SKELETON: return "the Bones";
    case CARD_WATER:           return "Water";
    case CARD_SWAP:            return "Swap";
#endif

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

static const deck_archetype* _cards_in_deck(uint8_t deck_type)
{
    deck_type_data *deck_data = map_find(all_decks, (misc_item_type)deck_type);

    if (deck_data)
        return deck_data->cards;

#ifdef ASSERTS
    die("No cards found for %u", unsigned(deck_type));
#endif
    return {};
}

const string deck_contents(uint8_t deck_type)
{
    string output = "\nIt may contain the following cards: ";

    // This way of doing things is intended to prevent a card
    // that appears in multiple subdecks from showing up twice in the
    // output.
    set<card_type> cards;
    const deck_archetype* pdeck =_cards_in_deck(deck_type);
    for (const card_with_weights& cww : *pdeck)
        cards.insert(cww.card);

    output += comma_separated_fn(cards.begin(), cards.end(), card_name);
    output += ".";

    return output;
}

static card_type _choose_from_deck(const deck_archetype* pdeck,
                                   deck_rarity_type rarity)
{
    // Random rarity should have been replaced by one of the others by now.
    ASSERT_RANGE(rarity, DECK_RARITY_COMMON, DECK_RARITY_LEGENDARY + 1);

    // FIXME: We should use one of the various choose_random_weighted
    // functions here, probably with an iterator, instead of
    // duplicating the implementation.

    int totalweight = 0;
    card_type result = NUM_CARDS;
    for (const card_with_weights cww : *pdeck)
    {
        totalweight += cww.weight[rarity - DECK_RARITY_COMMON];
        if (x_chance_in_y(cww.weight[rarity - DECK_RARITY_COMMON], totalweight))
            result = cww.card;
    }
    return result;
}

static card_type _random_card(uint8_t deck_type, deck_rarity_type rarity)
{
    const deck_archetype *pdeck = _cards_in_deck(deck_type);

    return _choose_from_deck(pdeck, rarity);
}

static card_type _random_card(const item_def& item)
{
    return _random_card(item.sub_type, item.deck_rarity);
}

static card_type _draw_top_card(item_def& deck, bool message,
                                uint8_t &_flags)
{
    CrawlHashTable &props = deck.props;
    CrawlVector    &cards = props[CARD_KEY].get_vector();
    CrawlVector    &flags = props[CARD_FLAG_KEY].get_vector();

    int num_cards = cards.size();
    int idx       = num_cards - 1;

    ASSERT(num_cards > 0);

    card_type card = get_card_and_flags(deck, idx, _flags);
    cards.pop_back();
    flags.pop_back();

    if (message)
    {
        const char *verb = (_flags & CFLAG_DEALT) ? "deal" : "draw";

        mprf("You %s a card... It is %s.", verb, card_name(card));
    }

    return card;
}

static void _push_top_card(item_def& deck, card_type card,
                           uint8_t _flags)
{
    CrawlHashTable &props = deck.props;
    CrawlVector    &cards = props[CARD_KEY].get_vector();
    CrawlVector    &flags = props[CARD_FLAG_KEY].get_vector();

    cards.push_back((char) card);
    flags.push_back((char) _flags);
}

static void _remember_drawn_card(item_def& deck, card_type card, bool allow_id)
{
    ASSERT(is_deck(deck));
    CrawlHashTable &props = deck.props;
    CrawlVector &drawn = props[DRAWN_CARD_KEY].get_vector();
    drawn.push_back(static_cast<char>(card));

    // Once you've drawn two cards, you know the deck.
    if (allow_id && (drawn.size() >= 2 || origin_as_god_gift(deck)))
        _deck_ident(deck);
}

const vector<card_type> get_drawn_cards(const item_def& deck)
{
    vector<card_type> result;
    if (is_deck(deck))
    {
        const CrawlHashTable &props = deck.props;
        const CrawlVector &drawn = props[DRAWN_CARD_KEY].get_vector();
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

    if (!props.exists(CARD_KEY)
        || props[CARD_KEY].get_type() != SV_VEC
        || props[CARD_KEY].get_vector().get_type() != SV_BYTE
        || cards_in_deck(deck) == 0)
    {
        crawl_state.zero_turns_taken();

        if (!props.exists(CARD_KEY))
            strm << "Seems this deck never had any cards in the first place!";
        else if (props[CARD_KEY].get_type() != SV_VEC)
            strm << "'cards' property isn't a vector.";
        else
        {
            if (props[CARD_KEY].get_vector().get_type() != SV_BYTE)
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

    CrawlVector &cards = props[CARD_KEY].get_vector();
    CrawlVector &flags = props[CARD_FLAG_KEY].get_vector();

    vec_size num_cards = cards.size();
    vec_size num_flags = flags.size();

    unsigned int num_buggy     = 0;

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
                 << (-deck.used_count) << ". Oops.";
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

/**
 * What message should be printed when a deck of the given rarity is exhausted?
 *
 * @param rarity    The deck_rarity of the deck that's been exhausted.
 * @return          A message to print;
 *                  e.g. "the deck of cards disappears without a trace."
 */
static string _empty_deck_msg(deck_rarity_type rarity)
{
    static const map<deck_rarity_type, const char*> empty_deck_messages = {
        { DECK_RARITY_COMMON,
            "disappears without a trace." },
        { DECK_RARITY_RARE,
            "glows slightly and disappears." },
        { DECK_RARITY_LEGENDARY,
            "glows with a rainbow of weird colours and disappears." },
    };
    const char* const *rarity_message = map_find(empty_deck_messages, rarity);
    ASSERT(rarity_message);
    return make_stringf("The deck of cards %s", *rarity_message);
}

// Choose a deck from inventory and return its slot (or -1).
static int _choose_inventory_deck(const char* prompt)
{
    const int slot = prompt_invent_item(prompt, MT_INVLIST,
                                        OSEL_DRAW_DECK, OPER_EVOKE);

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

// Draw the top four cards of an unstacked deck and play them all.
// Discards the rest of the deck. Return false if the operation was
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
    if (props[STACKED_KEY].get_bool())
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
        mpr(_empty_deck_msg(deck.deck_rarity));
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
    return any_of(pdeck->begin(), pdeck->end(),
                  [card](const card_with_weights& cww){return cww.card == card;});

}

string which_decks(card_type card)
{
    vector<string> decks;
    string output = "\n";
    bool punishment = false;
    for (auto &deck_data : all_decks)
    {
        misc_item_type deck = (misc_item_type)deck_data.first;
        if (!_card_in_deck(card, deck_data.second.cards))
            continue;

        if (deck == MISC_DECK_OF_PUNISHMENT)
            punishment = true;
        else
            decks.push_back(deck_data.second.name);
    }

    if (!decks.empty())
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
        output += "It is normally not part of any deck.";

    return output;
}

static void _describe_cards(vector<card_type> cards)
{
    ASSERT(!cards.empty());

#ifdef USE_TILE_WEB
    tiles_crt_control show_as_menu(CRT_MENU, "describe_cards");
#endif

    ostringstream data;
    bool first = true;
    for (card_type card : cards)
    {
        string name = card_name(card);
        string desc = getLongDescription(name + " card");
        if (desc.empty())
            desc = "No description found.";

        name = uppercase_first(name);
        if (first)
            first = false;
        else
            data << "\n";
        data << "<w>" << name << "</w>\n"
             << get_linebreak_string(desc, get_number_of_cols() - 1)
             << "\n" << which_decks(card) << "\n";
    }
    formatted_scroller fs(0, data.str());
    fs.show();
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
    // these are included in default force_more_message to show them before menu

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
            flags.push_back(_flags | CFLAG_SEEN);
        }
        // Rest of deck is discarded.
    }

    CrawlHashTable &props = deck.props;
    deck.used_count = -num_to_stack;
    props[STACKED_KEY] = true;
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
                msg::streams(MSGCH_PROMPT)
                    << msg::nocap << (static_cast<char>(i + 'a')) << " - "
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

    you.wield_change = true;

    // Make deck disappear *before* the card effect, since we
    // don't want to unwield an empty deck.
    const deck_rarity_type rarity = deck.deck_rarity;
    if (cards_in_deck(deck) == 0)
    {
        mpr(_empty_deck_msg(deck.deck_rarity));
        if (slot == you.equip[EQ_WEAPON])
            unwield_item();

        dec_inv_item_quantity(slot, 1);
    }

    // Note that card_effect() might cause you to unwield the deck.
    card_effect(draws[selected], rarity,
                flags[selected] | CFLAG_SEEN, false);

    return true;
}

// This is Nemelex retribution. If deal is true, use the word "deal"
// rather than "draw" (for the Deal Four out-of-cards situation).
void draw_from_deck_of_punishment(bool deal)
{
    uint8_t flags = CFLAG_PUNISHMENT;
    if (deal)
        flags |= CFLAG_DEALT;
    card_type card = _random_card(MISC_DECK_OF_PUNISHMENT, DECK_RARITY_COMMON);

    mprf("You %s a card...", deal ? "deal" : "draw");
    card_effect(card, DECK_RARITY_COMMON, flags);
}

void evoke_deck(item_def& deck)
{
    if (_check_buggy_deck(deck))
        return;

    bool allow_id = in_inventory(deck) && !item_ident(deck, ISFLAG_KNOW_TYPE);

    const deck_rarity_type rarity = deck.deck_rarity;

    uint8_t flags = 0;
    card_type card = _draw_top_card(deck, true, flags);

    deck.used_count++;
    _remember_drawn_card(deck, card, allow_id);

    // Get rid of the deck *before* the card effect because a card
    // might cause a wielded deck to be swapped out for something else,
    // in which case we don't want an empty deck to go through the
    // swapping process.
    const bool deck_gone = (cards_in_deck(deck) == 0);
    if (deck_gone)
    {
        mpr(_empty_deck_msg(deck.deck_rarity));
        dec_inv_item_quantity(deck.link, 1);
    }

    card_effect(card, rarity, flags, false);

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
        // small chance for an upgrade - approx 1/2 ORNATE chance
        // (plain decks don't get the +150 power boost)
        if (x_chance_in_y(power, 1000))
            ++power_level;
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

    // other functions in this file will break if this assertion is violated
    ASSERT(power_level >= 0 && power_level <= 2);
    return power_level;
}

static void _suppressed_card_message(god_type god, conduct_type done)
{
    string forbidden_act;

    switch (done)
    {
        case DID_EVIL: forbidden_act = "evil"; break;
        case DID_CHAOS: forbidden_act = "chaotic"; break;
        case DID_HASTY: forbidden_act = "hasty"; break;
        case DID_FIRE: forbidden_act = "fiery"; break;

        default: forbidden_act = "buggy"; break;
    }

    mprf("By %s power, the %s magic on the card dissipates harmlessly.",
         apostrophise(god_name(you.religion)).c_str(), forbidden_act.c_str());
}

// Actual card implementations follow.

static void _velocity_card(int power, deck_rarity_type rarity)
{

    const int power_level = _get_power_level(power, rarity);
    bool did_something = false;
    enchant_type for_allies = ENCH_NONE, for_hostiles = ENCH_NONE;

    if (you.duration[DUR_SLOW] && (power_level > 0 || coinflip()))
    {
        you.duration[DUR_SLOW] = 1;
        did_something = true;
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
                                                      ENCH_SWIFT);
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

    if (!apply_visible_monsters([=](monster& mon)
          {
              bool affected = false;
              if (!mons_immune_magic(mon))
              {
                  const bool hostile = !mon.wont_attack();
                  const bool haste_immune = (mon.stasis()
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
                              affected = true;
                          }
                          else if (!(for_hostiles == ENCH_HASTE && haste_immune))
                          {
                              if (have_passive(passive_t::no_haste))
                                  _suppressed_card_message(you.religion, DID_HASTY);
                              else
                              {
                                  mon.add_ench(for_hostiles);
                                  affected = true;
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
                              affected = true;
                          }
                          else if (!(for_allies == ENCH_HASTE && haste_immune))
                          {
                              if (have_passive(passive_t::no_haste))
                                  _suppressed_card_message(you.religion, DID_HASTY);
                              else
                              {
                                  mon.add_ench(for_allies);
                                  affected = true;
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
              return affected;
          })
        && !did_something)
    {
        canned_msg(MSG_NOTHING_HAPPENS);
    }
}

static void _exile_card(int power, deck_rarity_type rarity)
{
    if (player_in_branch(BRANCH_ABYSS))
    {
        canned_msg(MSG_NOTHING_HAPPENS);
        return;
    }

    // Calculate how many extra banishments you get.
    const int power_level = _get_power_level(power, rarity);
    int extra_targets = power_level + random2(1 + power_level);

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

static void _shaft_card(int power, deck_rarity_type rarity)
{
    const int power_level = _get_power_level(power, rarity);
    bool did_something = false;

    if (is_valid_shaft_level())
    {
        if (grd(you.pos()) == DNGN_FLOOR)
        {
            place_specific_trap(you.pos(), TRAP_SHAFT);
            trap_at(you.pos())->reveal();
            mpr("A shaft materialises beneath you!");
            did_something = true;
        }

        did_something = apply_visible_monsters([=](monster& mons)
        {
            return !mons.wont_attack()
                   && mons_is_threatening(mons)
                   && x_chance_in_y(power_level, 3)
                   && mons.do_shaft();
        }) || did_something;
    }

    if (!did_something)
        canned_msg(MSG_NOTHING_HAPPENS);
}

static int stair_draw_count = 0;

// This does not describe an actual card. Instead, it only exists to test
// the stair movement effect in wizard mode ("&c stairs").
static void _stairs_card(int /*power*/, deck_rarity_type /*rarity*/)
{
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

    for (coord_def stair : stairs_avail)
        move_stair(stair, stair_draw_count % 2, false);

    stair_draw_count++;
}

static monster* _friendly(monster_type mt, int dur)
{
    return create_monster(mgen_data(mt, BEH_FRIENDLY, you.pos(), MHITYOU,
                                    MG_AUTOFOE)
                          .set_summoned(&you, dur, 0));
}

static void _damaging_card(card_type card, int power, deck_rarity_type rarity,
                           bool dealt = false)
{
    const int power_level = _get_power_level(power, rarity);
    const char *participle = dealt ? "dealt" : "drawn";

    bool done_prompt = false;
    string prompt = make_stringf("You have %s %s.", participle,
                                 card_name(card));

    dist target;
    zap_type ztype = ZAP_DEBUGGING_RAY;
    const zap_type painzaps[2] = { ZAP_AGONY, ZAP_BOLT_OF_DRAINING };
    const zap_type acidzaps[3] = { ZAP_BREATHE_ACID, ZAP_CORROSIVE_BOLT,
                                   ZAP_CORROSIVE_BOLT };
    const zap_type orbzaps[3]  = { ZAP_ISKENDERUNS_MYSTIC_BLAST, ZAP_IOOD,
                                   ZAP_IOOD };

    switch (card)
    {
    case CARD_VITRIOL:
        if (power_level == 2)
        {
            done_prompt = true;
            mpr(prompt);
            mpr("You radiate a wave of entropy!");
            apply_visible_monsters([](monster& mons)
            {
                return !mons.wont_attack()
                       && mons_is_threatening(mons)
                       && coinflip()
                       && mons.corrode_equipment();
            });
        }
        ztype = acidzaps[power_level];
        break;

    case CARD_ORB:
        ztype = orbzaps[power_level];
        break;

    case CARD_PAIN:
        if (power_level == 2)
        {
            if (is_good_god(you.religion))
            {
                _suppressed_card_message(you.religion, DID_EVIL);
                return;
            }

            mpr(prompt);

            if (monster *ghost = _friendly(MONS_FLAYED_GHOST, 3))
            {
                apply_visible_monsters([&, ghost](monster& mons)
                {
                    if (mons.wont_attack()
                        || !(mons.holiness() & MH_NATURAL))
                    {
                        return false;
                    }


                    flay(*ghost, mons, mons.hit_points * 2 / 5);
                    return true;
                }, ghost->pos());

                ghost->foe = MHITYOU; // follow you around (XXX: rethink)
                return;
            }
            // else, fallback to level 1
        }

        ztype = painzaps[min(power_level, (int)ARRAYSZ(painzaps)-1)];
        break;

    default:
        break;
    }

    bolt beam;
    beam.range = LOS_RADIUS;

    if (card == CARD_PAIN && is_good_god(you.religion))
    {
        _suppressed_card_message(you.religion, DID_EVIL);
        return;
    }

    direction_chooser_args args;
    args.mode = TARG_HOSTILE;
    if (!done_prompt)
        args.top_prompt = prompt;
    if (spell_direction(target, beam, &args)
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

    you.duration[DUR_ELIXIR_HEALTH] = 0;
    you.duration[DUR_ELIXIR_MAGIC] = 0;

    switch (power_level)
    {
    case 0:
        if (coinflip())
            you.set_duration(DUR_ELIXIR_HEALTH, 1 + random2(3));
        else
            you.set_duration(DUR_ELIXIR_MAGIC, 3 + random2(5));
        break;
    case 1:
        if (you.hp * 2 < you.hp_max)
            you.set_duration(DUR_ELIXIR_HEALTH, 3 + random2(3));
        else
            you.set_duration(DUR_ELIXIR_MAGIC, 10);
        break;
    default:
        you.set_duration(DUR_ELIXIR_HEALTH, 10);
        you.set_duration(DUR_ELIXIR_MAGIC, 10);
    }

    if (you.duration[DUR_ELIXIR_HEALTH] && you.duration[DUR_ELIXIR_MAGIC])
        mpr("You begin rapidly regenerating health and magic.");
    else if (you.duration[DUR_ELIXIR_HEALTH])
        mpr("You begin rapidly regenerating.");
    else
        mpr("You begin rapidly regenerating magic.");

    apply_visible_monsters([=](monster& mon)
    {
        if (mon.wont_attack())
        {
            const int hp = mon.max_hit_points / (4 - power_level);
            if (mon.heal(hp + random2avg(hp, 2)))
               simple_monster_message(mon, " is healed.");
        }
        return true;
    });
}

// Special case for *your* god, maybe?
static void _godly_wrath()
{
    for (int tries = 0; tries < 100; tries++)
    {
        god_type god = random_god();

        // Don't recursively make player draw from the Deck of Punishment.
        if (god != GOD_NEMELEX_XOBEH && divine_retribution(god))
            return; // Stop once we find a god willing to punish the player.
    }

    mpr("You somehow manage to escape divine attention...");
}

static void _summon_demon_card(int power, deck_rarity_type rarity)
{
    const int power_level = _get_power_level(power, rarity);
    // one demon (potentially hostile), and one other demonic creature (always
    // friendly)
    monster_type dct, dct2;
    switch (power_level)
    {
    case 0:
        dct = random_demon_by_tier(4);
        dct2 = MONS_HELL_HOUND;
        break;
    case 1:
        dct = random_demon_by_tier(3);
        dct2 = MONS_RAKSHASA;
        break;
    default:
        dct = random_demon_by_tier(2);
        dct2 = MONS_PANDEMONIUM_LORD;
    }

    if (is_good_god(you.religion))
    {
        _suppressed_card_message(you.religion, DID_EVIL);
        return;
    }

    // FIXME: The manual testing for message printing is there because
    // we can't rely on create_monster() to do it for us. This is
    // because if you are completely surrounded by walls, create_monster()
    // will never manage to give a position which isn't (-1,-1)
    // and thus not print the message.
    // This hack appears later in this file as well.

    const bool hostile = one_chance_in(power_level + 4);

    if (!create_monster(mgen_data(dct, hostile ? BEH_HOSTILE : BEH_FRIENDLY,
                                  you.pos(), MHITYOU, MG_AUTOFOE)
                        .set_summoned(&you, 5 - power_level, 0)))
    {
        mpr("You see a puff of smoke.");
    }
    else if (hostile
             && mons_class_flag(dct, M_INVIS)
             && !you.can_see_invisible())
    {
        mpr("You sense the presence of something unfriendly.");
    }

    _friendly(dct2, 5 - power_level);
}

static void _elements_card(int power, deck_rarity_type rarity)
{

    const int power_level = _get_power_level(power, rarity);
    const monster_type element_list[][3] =
    {
        {MONS_RAIJU, MONS_WIND_DRAKE, MONS_SHOCK_SERPENT},
        {MONS_BASILISK, MONS_CATOBLEPAS, MONS_IRON_GOLEM},
        {MONS_FIRE_VORTEX, MONS_MOLTEN_GARGOYLE, MONS_FIRE_DRAGON},
        {MONS_ICE_BEAST, MONS_POLAR_BEAR, MONS_ICE_DRAGON}
    };

    int start = random2(ARRAYSZ(element_list));

    for (int i = 0; i < 3; ++i)
    {
        monster_type mons_type = element_list[start % ARRAYSZ(element_list)][power_level];
        monster hackmon;

        hackmon.type = mons_type;
        mons_load_spells(hackmon);

        if (you_worship(GOD_DITHMENOS) && mons_is_fiery(hackmon))
        {
            _suppressed_card_message(you.religion, DID_FIRE);
            start++;
            continue;
        }

        _friendly(mons_type, power_level + 2);
        start++;
    }

}

static void _summon_dancing_weapon(int power, deck_rarity_type rarity)
{
    const int power_level = _get_power_level(power, rarity);

    monster *mon =
        create_monster(
            mgen_data(MONS_DANCING_WEAPON, BEH_FRIENDLY, you.pos(), MHITYOU,
                      MG_AUTOFOE).set_summoned(&you, power_level + 2, 0),
            false);

    if (!mon)
    {
        mpr("You see a puff of smoke.");
        return;
    }

    // Override the weapon.
    ASSERT(mon->weapon() != nullptr);
    item_def& wpn(*mon->weapon());

    switch (power_level)
    {
    case 0:
        // Wimpy, negative-enchantment weapon.
        wpn.plus = random2(3) - 2;
        wpn.sub_type = random_choose(WPN_QUARTERSTAFF, WPN_HAND_AXE);

        set_item_ego_type(wpn, OBJ_WEAPONS,
                          random_choose(SPWPN_VENOM, SPWPN_NORMAL));
        break;
    case 1:
        // This is getting good.
        wpn.plus = random2(4) - 1;
        wpn.sub_type = random_choose(WPN_LONG_SWORD, WPN_TRIDENT);

        if (coinflip())
        {
            set_item_ego_type(wpn, OBJ_WEAPONS,
                              random_choose(SPWPN_FLAMING, SPWPN_FREEZING));
        }
        else
            set_item_ego_type(wpn, OBJ_WEAPONS, SPWPN_NORMAL);
        break;
    default:
        // Rare and powerful.
        wpn.plus = random2(4) + 2;
        wpn.sub_type = random_choose(WPN_DEMON_TRIDENT, WPN_EXECUTIONERS_AXE);

        set_item_ego_type(wpn, OBJ_WEAPONS,
                          random_choose(SPWPN_SPEED, SPWPN_ELECTROCUTION));
    }

    item_colour(wpn); // this is probably not needed

    // sometimes give a randart instead
    if (one_chance_in(3))
    {
        make_item_randart(wpn, true);
        set_ident_flags(wpn, ISFLAG_KNOW_PROPERTIES| ISFLAG_KNOW_TYPE);
    }

    // Don't leave a trail of weapons behind. (Especially not randarts!)
    mon->flags |= MF_HARD_RESET;

    ghost_demon newstats;
    newstats.init_dancing_weapon(wpn, power / 4);

    mon->set_ghost(newstats);
    mon->ghost_demon_init();
}

static void _summon_flying(int power, deck_rarity_type rarity)
{
    const int power_level = _get_power_level(power, rarity);

    const monster_type flytypes[] =
    {
        MONS_INSUBSTANTIAL_WISP, MONS_WYVERN, MONS_KILLER_BEE,
        MONS_VAMPIRE_MOSQUITO, MONS_HORNET
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
        const bool hostile = one_chance_in(power_level + 4);

        create_monster(
            mgen_data(result,
                      hostile ? BEH_HOSTILE : BEH_FRIENDLY, you.pos(), MHITYOU,
                      MG_AUTOFOE).set_summoned(&you, 3, 0));

        if (hostile && mons_class_flag(result, M_INVIS) && !you.can_see_invisible())
            hostile_invis = true;
    }

    if (hostile_invis)
        mpr("You sense the presence of something unfriendly.");
}

static void _summon_rangers(int power, deck_rarity_type rarity)
{
    const int power_level = _get_power_level(power, rarity);
    const monster_type dctr  = random_choose(MONS_CENTAUR, MONS_YAKTAUR),
                       dctr2 = random_choose(MONS_CENTAUR_WARRIOR, MONS_FAUN),
                       dctr3 = random_choose(MONS_YAKTAUR_CAPTAIN,
                                             MONS_NAGA_SHARPSHOOTER),
                       dctr4 = random_choose(MONS_SATYR,
                                             MONS_MERFOLK_JAVELINEER,
                                             MONS_DEEP_ELF_MASTER_ARCHER);

    const monster_type base_choice = power_level == 2 ? dctr2 :
                                                        dctr;
    monster_type placed_choice  = power_level == 2 ? dctr3 :
                                  power_level == 1 ? dctr2 :
                                                     dctr;
    const bool extra_monster = coinflip();

    if (!extra_monster && power_level > 0)
        placed_choice = power_level == 2 ? dctr4 : dctr3;

    for (int i = 0; i < 1 + extra_monster; ++i)
        _friendly(base_choice, 5 - power_level);

    _friendly(placed_choice, 5 - power_level);
}

#if TAG_MAJOR_VERSION == 34
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
        simple_monster_message(*mon, " attacks!");
        return true;
    }

    simple_monster_message(*mon, " joins your ranks!");
    for (mon_inv_iterator ii(*mon); ii; ++ii)
        ii->flags &= ~ISFLAG_SUMMONED;
    mon->flags &= ~MF_HARD_RESET;
    mon->attitude = ATT_FRIENDLY;
    mons_att_changed(mon);
    take_note(Note(NOTE_MESSAGE, 0, 0, message), true);
    you.del_gold(fee);
    return true;
}
#endif

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

        if (!mons || mons->wont_attack() || !mons_is_threatening(*mons))
            continue;

        for (adjacent_iterator ai(mons->pos()); ai; ++ai)
        {
            // don't place clouds on the player or monsters
            if (*ai == you.pos() || monster_at(*ai))
                continue;

            if (grd(*ai) == DNGN_FLOOR && !cloud_at(*ai))
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

static void _storm_card(int power, deck_rarity_type rarity)
{
    const int power_level = _get_power_level(power, rarity);

    _friendly(MONS_AIR_ELEMENTAL, 3);

    wind_blast(&you, (power_level == 0) ? 100 : 200, coord_def(), true);

    for (radius_iterator ri(you.pos(), 4, C_SQUARE, LOS_SOLID); ri; ++ri)
    {
        monster *mons = monster_at(*ri);

        if (adjacent(*ri, you.pos()))
            continue;

        if (mons && mons->wont_attack())
            continue;

        if ((feat_has_solid_floor(grd(*ri))
             || grd(*ri) == DNGN_DEEP_WATER)
            && !cloud_at(*ri))
        {
            place_cloud(CLOUD_STORM, *ri,
                        5 + (power_level + 1) * random2(10), & you);
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

    if (you_worship(GOD_ZIN))
    {
        _suppressed_card_message(you.religion, DID_CHAOS);
        return;
    }

    if (!apply_visible_monsters([power_level](monster& mons)
           {
               if (mons.wont_attack() || !mons_is_threatening(mons))
                   return false;

               if (!x_chance_in_y((power_level + 1) * 5 + random2(5),
                                  mons.get_hit_dice()))
               {
                   return false;
               }

               if (mons.can_polymorph())
               {
                   monster_polymorph(&mons, RANDOM_MONSTER, PPT_LESS);
                   mons.malmutate("");
               }
               else
               {
                   const int daze_time = (5 + 5 * power_level) * BASELINE_DELAY;
                   mons.add_ench(mon_enchant(ENCH_DAZED, 0, &you, daze_time));
                   simple_monster_message(mons,
                                          " is dazed by the mutagenic energy.");
               }
               return true;
           }))
    {
        canned_msg(MSG_NOTHING_HAPPENS);
    }
}

static void _wild_magic_card(int power, deck_rarity_type rarity)
{
    const int power_level = _get_power_level(power, rarity);
    int num_affected = 0;

    for (radius_iterator di(you.pos(), LOS_NO_TRANS); di; ++di)
    {
        monster *mons = monster_at(*di);

        if (!mons || mons->wont_attack() || !mons_is_threatening(*mons))
            continue;

        if (x_chance_in_y((power_level + 1) * 5 + random2(5),
                           mons->get_hit_dice()))
        {
            spschool_flag_type type = random_choose(SPTYP_CONJURATION,
                                                    SPTYP_FIRE,
                                                    SPTYP_ICE,
                                                    SPTYP_EARTH,
                                                    SPTYP_AIR,
                                                    SPTYP_POISON);

            MiscastEffect(mons, actor_by_mid(MID_YOU_FAULTLESS),
                          DECK_MISCAST, type,
                          random2(power/15) + 5, random2(power),
                          "a card of wild magic");

            num_affected++;
        }
    }

    if (num_affected > 0)
    {
        int mp = 0;

        for (int i = 0; i < num_affected; ++i)
            mp += random2(5);

        mpr("You feel a surge of magic.");
        if (mp && you.magic_points < you.max_magic_points)
        {
            inc_mp(mp);
            canned_msg(MSG_GAIN_MAGIC);
        }
    }
    else
        canned_msg(MSG_NOTHING_HAPPENS);
}

static void _torment_card()
{
    if (you.undead_or_demonic())
        holy_word_player(HOLY_WORD_CARD);
    else
        torment_player(&you, TORMENT_CARDS);
}

// Punishment cards don't have their power adjusted depending on Nemelex piety,
// and are based on experience level instead of invocations skill.
static int _card_power(deck_rarity_type rarity, bool punishment)
{
    if (punishment)
        return you.experience_level * 18;

    int result = you.piety;
    result *= you.skill(SK_INVOCATIONS, 100) + 2500;
    result /= 2700;
    result += you.skill(SK_INVOCATIONS, 9);

    if (rarity == DECK_RARITY_RARE)
        result += 150;
    else if (rarity == DECK_RARITY_LEGENDARY)
        result += 300;

    return result;
}

void card_effect(card_type which_card, deck_rarity_type rarity,
                 uint8_t flags, bool tell_card)
{
    const char *participle = (flags & CFLAG_DEALT) ? "dealt" : "drawn";
    const int power = _card_power(rarity, flags & CFLAG_PUNISHMENT);

    dprf("Card power: %d, rarity: %d", power, rarity);

    if (tell_card)
    {
        // These card types will usually give this message in the targeting
        // prompt, and the cases where they don't are handled specially.
        if (which_card != CARD_VITRIOL
            && which_card != CARD_PAIN
            && which_card != CARD_ORB)
        {
            mprf("You have %s %s.", participle, card_name(which_card));
        }
    }

    switch (which_card)
    {
    case CARD_VELOCITY:         _velocity_card(power, rarity); break;
    case CARD_EXILE:            _exile_card(power, rarity); break;
    case CARD_ELIXIR:           _elixir_card(power, rarity); break;
    case CARD_STAIRS:           _stairs_card(power, rarity); break;
    case CARD_SHAFT:            _shaft_card(power, rarity); break;
    case CARD_TOMB:             entomb(10 + power/20 + random2(power/4)); break;
    case CARD_WRAITH:           drain_player(power / 4, false, true); break;
    case CARD_WRATH:            _godly_wrath(); break;
    case CARD_SUMMON_DEMON:     _summon_demon_card(power, rarity); break;
    case CARD_ELEMENTS:         _elements_card(power, rarity); break;
    case CARD_RANGERS:          _summon_rangers(power, rarity); break;
    case CARD_SUMMON_WEAPON:    _summon_dancing_weapon(power, rarity); break;
    case CARD_SUMMON_FLYING:    _summon_flying(power, rarity); break;
    case CARD_TORMENT:          _torment_card(); break;
    case CARD_CLOUD:            _cloud_card(power, rarity); break;
    case CARD_STORM:            _storm_card(power, rarity); break;
    case CARD_ILLUSION:         _illusion_card(power, rarity); break;
    case CARD_DEGEN:            _degeneration_card(power, rarity); break;
    case CARD_WILD_MAGIC:       _wild_magic_card(power, rarity); break;

    case CARD_VITRIOL:
    case CARD_PAIN:
    case CARD_ORB:
        _damaging_card(which_card, power, rarity, flags & CFLAG_DEALT);
        break;

    case CARD_FAMINE:
        if (you_foodless())
            mpr("You feel rather smug.");
        else
            set_hunger(min(you.hunger, HUNGER_STARVING / 2), true);
        break;

    case CARD_SWINE:
        if (transform(5 + power/10 + random2(power/10), transformation::pig, true))
            you.transform_uncancellable = true;
        else
            mpr("You feel a momentary urge to oink.");
        break;

#if TAG_MAJOR_VERSION == 34
    case CARD_VENOM:
    case CARD_HAMMER:
    case CARD_FORTITUDE:
    case CARD_WATER:
    case CARD_BANSHEE:
    case CARD_ALCHEMIST:
    case CARD_CRUSADE:
    case CARD_SUMMON_UGLY:
    case CARD_WARPWRIGHT:
    case CARD_SWAP:
    case CARD_SOLITUDE:
    case CARD_HELM:
    case CARD_BLADE:
    case CARD_SHADOW:
    case CARD_POTION:
    case CARD_SHUFFLE:
    case CARD_EXPERIENCE:
    case CARD_SAGE:
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
    case CARD_PLACID_MAGIC:
    case CARD_FOCUS:
    case CARD_HELIX:
    case CARD_MERCENARY:
    case CARD_XOM:
    case CARD_FEAST:
    case CARD_CURSE:
    case CARD_DOWSING:
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

    return flags & CFLAG_SEEN;
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

/**
 * Return the appropriate name for a known deck of the given type.
 *
 * @param sub_type  The type of deck in question.
 * @return          A name, e.g. "deck of destruction".
 *                  Does not include rarity.
 *                  If the given type isn't a deck, return "deck of bugginess".
 */
string deck_name(uint8_t sub_type)
{
    const deck_type_data *deck_data = map_find(all_decks,
                                               (misc_item_type)sub_type);
    const string name = deck_data ? deck_data->name : "bugginess";
    return "deck of " + name;
}

/**
 * Returns the appropriate type for a deck name.
 *
 * @param name      The name in question; e.g. "destruction", "summonings", etc.
 * @return          The type of the deck, if one is found;
 *                  else, MISC_DECK_UNKNOWN.
 */
uint8_t deck_type_by_name(string name)
{
    for (auto &deck_data : all_decks)
        if (deck_data.second.name == name)
            return deck_data.first;

    return MISC_DECK_UNKNOWN;
}

/**
 * Choose a random deck type for normal generation. (Not Nemelex.)
 */
uint8_t random_deck_type()
{
    const misc_item_type deck_type = random_choose(MISC_DECK_OF_ESCAPE,
                                                   MISC_DECK_OF_DESTRUCTION,
                                                   MISC_DECK_OF_SUMMONING);
    return deck_type;
}

bool is_deck_type(uint8_t sub_type, bool allow_unided)
{
    return allow_unided && sub_type == MISC_DECK_UNKNOWN
           || map_find(all_decks, (misc_item_type)sub_type) != nullptr;
}

bool is_deck(const item_def &item, bool iinfo)
{
    return item.base_type == OBJ_MISCELLANY
           && is_deck_type(item.sub_type, iinfo);
}

bool bad_deck(const item_def &item)
{
    if (!is_deck(item))
        return false;

    return !item.props.exists(CARD_KEY)
           || item.props[CARD_KEY].get_type() != SV_VEC
           || item.props[CARD_KEY].get_vector().get_type() != SV_BYTE
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
    ASSERT_RANGE(item.initial_cards, 1, 128);
    ASSERT(!props.exists(CARD_KEY));
    ASSERT(item.deck_rarity >= DECK_RARITY_COMMON
           && item.deck_rarity <= DECK_RARITY_LEGENDARY);

    const store_flags fl = SFLAG_CONST_TYPE;

    props[CARD_KEY].new_vector(SV_BYTE, fl).resize((vec_size)
                                                   item.initial_cards);
    props[CARD_FLAG_KEY].new_vector(SV_BYTE, fl).resize((vec_size)
                                                        item.initial_cards);
    props[DRAWN_CARD_KEY].new_vector(SV_BYTE, fl);

    for (int i = 0; i < item.initial_cards; ++i)
    {
        card_type card    = _random_card(item);
        _set_card_and_flags(item, i, card, 0);
    }

    ASSERT(cards_in_deck(item) == item.initial_cards);

    props.assert_validity();

    item.used_count  = 0;
}

void reclaim_decks_on_level()
{
    for (auto &item : mitm)
        if (item.defined() && is_deck(item))
            destroy_item(item.index());
}

static void _reclaim_inventory_decks()
{
    for (auto &item : you.inv)
        if (item.defined() && is_deck(item))
            dec_inv_item_quantity(item.link, 1);
}

void nemelex_reclaim_decks()
{
    add_daction(DACT_RECLAIM_DECKS);
    _reclaim_inventory_decks();
}
