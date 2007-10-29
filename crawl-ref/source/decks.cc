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
#include "output.h"
#include "player.h"
#include "religion.h"
#include "spells1.h"
#include "spells2.h"
#include "spells3.h"
#include "spells4.h"
#include "spl-cast.h"
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
// card), deck.props.["card_flags"] holds the flags for each card,
// deck.props["num_marked"] is the number of marked cards left in the
// deck, and deck.props["non_brownie_draws"] is the number of
// non-marked draws you have to make from that deck before earning
// brownie points from it again.
//
// The card type and per-card flags are each stored as unsigned bytes,
// for a maximum of 256 different kinds of cards and 8 bits of flags.

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

static void check_odd_card(unsigned char flags)
{
    if ((flags & CFLAG_ODDITY) && !(flags & CFLAG_SEEN))
        mpr("This card doesn't seem to belong here.");
}

static unsigned long cards_in_deck(const item_def &deck)
{
    ASSERT(is_deck(deck));

    const CrawlHashTable &props = deck.props;
    ASSERT(props.exists("cards"));

    return static_cast<unsigned long>(props["cards"].get_vector().size());
}

static void shuffle_deck(item_def &deck)
{
    ASSERT(is_deck(deck));

    CrawlHashTable &props = deck.props;
    ASSERT(props.exists("cards"));

    CrawlVector &cards = props["cards"];
    ASSERT(cards.size() > 1);

    CrawlVector &flags = props["card_flags"];
    ASSERT(flags.size() == cards.size());

    // Don't use std::shuffle(), since we want to apply exactly the
    // same shuffling to both the cards vector and the flags vector.
    std::vector<long> pos;
    for (unsigned long i = 0, size = cards.size(); i < size; i++)
        pos.push_back(random2(size));

    for (unsigned long i = 0, size = pos.size(); i < size; i++)
    {
        std::swap(cards[i], cards[pos[i]]);
        std::swap(flags[i], flags[pos[i]]);
    }
}

static card_type get_card_and_flags(const item_def& deck, int idx,
                                    unsigned char& _flags)
{
    const CrawlHashTable &props = deck.props;
    const CrawlVector    &cards = props["cards"].get_vector();
    const CrawlVector    &flags = props["card_flags"].get_vector();

    if (idx == -1)
        idx = (int) cards.size() - 1;

    _flags = (unsigned char) flags[idx].get_byte();

    return static_cast<card_type>(cards[idx].get_byte());
}

static void set_card_and_flags(item_def& deck, int idx, card_type card,
                               unsigned char _flags)
{
    CrawlHashTable &props = deck.props;
    CrawlVector    &cards = props["cards"];
    CrawlVector    &flags = props["card_flags"];

    if (idx == -1)
        idx = (int) cards.size() - 1;

    cards[idx] = (char) card;
    flags[idx] = (char) _flags;
}

const char* card_name(card_type card)
{
    switch (card)
    {
    case CARD_BLANK1: return "blank card";
    case CARD_BLANK2: return "blank card";
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

static const std::vector<card_type>* random_sub_deck(unsigned char deck_type)
{
    const std::vector<card_type> *pdeck = NULL;
    switch ( deck_type )
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

    return pdeck;
}

static card_type random_card(unsigned char deck_type, bool &was_oddity)
{
    const std::vector<card_type> *pdeck = random_sub_deck(deck_type);

    if ( one_chance_in(100) )
    {
        pdeck      = &deck_of_oddities;
        was_oddity = true;
    }

    card_type chosen = (*pdeck)[random2(pdeck->size())];

    // Paranoia
    if (chosen < CARD_BLANK1 || chosen >= NUM_CARDS)
        chosen = NUM_CARDS;

    return chosen;
}

static card_type random_card(const item_def& item, bool &was_oddity)
{
    return random_card(item.sub_type, was_oddity);
}

static void retry_blank_card(card_type &card, unsigned char deck_type,
                             unsigned char flags)
{
    // BLANK1 == hasn't been retried
    if (card != CARD_BLANK1)
        return;

    if (flags & (CFLAG_MARKED | CFLAG_SEEN))
    {
        // Can't retry a card which has been seen or marked.
        card = CARD_BLANK2;
        return;
    }

    const std::vector<card_type> *pdeck = random_sub_deck(deck_type);

    if (flags & CFLAG_ODDITY)
        pdeck = &deck_of_oddities;

    // High Evocations gives you another shot (but not at being punished...)
    if (pdeck != &deck_of_punishment 
        && you.skills[SK_EVOCATIONS] > random2(30))
    {
        card = (*pdeck)[random2(pdeck->size())];
    }

    // BLANK2 == retried and failed
    if (card == CARD_BLANK1)
        card = CARD_BLANK2;
}

static void retry_blank_card(card_type &card, item_def& deck,
                             unsigned char flags)
{
    retry_blank_card(card, deck.sub_type, flags);
}

static card_type draw_top_card(item_def& deck, bool message,
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

    retry_blank_card(card, deck, _flags);

    if (message)
    {
        if (_flags & CFLAG_MARKED)
            mprf("You draw %s.", card_name(card));
        else
            mprf("You draw a card... It is %s.", card_name(card));

        check_odd_card(_flags);
    }

    return card;
}

static void push_top_card(item_def& deck, card_type card,
                          unsigned char _flags)
{
    CrawlHashTable &props = deck.props;
    CrawlVector    &cards = props["cards"].get_vector();
    CrawlVector    &flags = props["card_flags"].get_vector();

    cards.push_back((char) card);
    flags.push_back((char) _flags);
}

static bool wielding_deck()
{
    if ( you.equip[EQ_WEAPON] == -1 )
        return false;
    return is_deck(you.inv[you.equip[EQ_WEAPON]]);
}

static bool check_buggy_deck(item_def& deck)
{
    std::ostream& strm = msg::streams(MSGCH_DIAGNOSTICS);
    if (!is_deck(deck))
    {
        crawl_state.zero_turns_taken();
        strm << "This isn't a deck at all!" << std::endl;
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

        if ( deck.link == you.equip[EQ_WEAPON] )
            unwield_item();

        dec_inv_item_quantity( deck.link, 1 );
        did_god_conduct(DID_CARDS, 1);

        return true;
    }

    bool problems = false;

    CrawlVector &cards = props["cards"].get_vector();
    CrawlVector &flags = props["card_flags"].get_vector();

    unsigned long num_cards = cards.size();
    unsigned long num_flags = flags.size();

    unsigned int num_buggy     = 0;
    unsigned int num_marked    = 0;

    for (unsigned long i = 0; i < num_cards; i++)
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

        if ( deck.link == you.equip[EQ_WEAPON] )
            unwield_item();

        dec_inv_item_quantity( deck.link, 1 );
        did_god_conduct(DID_CARDS, 1);

        return true;
    }

    if (static_cast<long>(num_cards) > deck.plus)
    {
        if (deck.plus == 0)
            strm << "Deck was created with zero cards???" << std::endl;
        else if (deck.plus < 0)
            strm << "Deck was created with *negative* cards?!" << std::endl;
        else
            strm << "Deck has more cards than it was created with?"
                 << std::endl;

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
        return false;

    you.wield_change = true;

    if (!yesno("Problems might not have been completely fixed; "
               "still use deck?"))
    {
        crawl_state.zero_turns_taken();
        return true;
    }
    return false;
}

// Choose a deck from inventory and return its slot (or -1.)
static int choose_inventory_deck( const char* prompt )
{
    const int slot = prompt_invent_item( prompt,
                                         MT_INVLIST, OBJ_MISCELLANY,
                                         true, true, true, 0, NULL,
                                         OPER_EVOKE );
    
    if ( slot == PROMPT_ABORT )
    {
        canned_msg(MSG_OK);
        return -1;
    }

    if ( !is_deck(you.inv[slot]) )
    {
        mpr("That isn't a deck!");
        return -1;
    }

    return slot;
}

// Select a deck from inventory and draw a card from it.
bool choose_deck_and_draw()
{
    const int slot = choose_inventory_deck( "Draw from which deck?" );

    if ( slot == -1 )
    {
        crawl_state.zero_turns_taken();
        return false;
    }

    evoke_deck(you.inv[slot]);
    return true;
}

static void deck_peek_ident(item_def& deck)
{
    if (in_inventory(deck) && !item_ident(deck, ISFLAG_KNOW_TYPE))
    {
        set_ident_flags(deck, ISFLAG_KNOW_TYPE);

        mprf("This is %s.", deck.name(DESC_NOCAP_A).c_str());

        you.wield_change = true;
    }
}

// Peek at a deck (show what the next card will be.)
// Return false if the operation was failed/aborted along the way.
bool deck_peek()
{
    if ( !wielding_deck() )
    {
        mpr("You aren't wielding a deck!");
        crawl_state.zero_turns_taken();
        return false;
    }
    item_def& deck(you.inv[you.equip[EQ_WEAPON]]);

    if (check_buggy_deck(deck))
        return false;

    if (deck.props["num_marked"].get_byte() > 0)
    {
        mpr("You can't peek into a marked deck.");
        crawl_state.zero_turns_taken();
        return false;
    }

    CrawlVector &cards     = deck.props["cards"];
    int          num_cards = cards.size();

    card_type card1, card2, card3;
    unsigned char flags1, flags2, flags3;

    card1 = get_card_and_flags(deck, 0, flags1);
    retry_blank_card(card1, deck, flags1);

    if (num_cards == 1)
    {
        deck_peek_ident(deck);

        mpr("There's only one card in the deck!");

        set_card_and_flags(deck, 0, card1, flags1 | CFLAG_SEEN | CFLAG_MARKED);
        deck.props["num_marked"]++;
        deck.plus2 = -1;
        you.wield_change = true;

        return true;
    }

    card2 = get_card_and_flags(deck, 1, flags2);
    retry_blank_card(card2, deck, flags2);

    if (num_cards == 2)
    {
        deck.props["non_brownie_draws"] = (char) 2;
        deck.plus2 = -2;

        deck_peek_ident(deck);

        mprf("Only two cards in the deck: %s and %s.",
             card_name(card1), card_name(card2));

        mpr("You shuffle the deck.");

        // If both cards are the same, then you know which card you're
        // going to draw both times.
        if (card1 == card2)
        {
            flags1 |= CFLAG_MARKED;
            flags2 |= CFLAG_MARKED;
            you.wield_change = true;
            deck.props["num_marked"] = (char) 2;
        }

        // "Shuffle" the two cards (even if they're the same, since
        // the flags might differ).
        if (coinflip())
        {
            std::swap(card1, card2);
            std::swap(flags1, flags2);
        }

        // After the first of two differing cards is drawn, you know
        // what the second card is going to be.
        if (card1 != card2)
        {
            flags1 |= CFLAG_MARKED;
            deck.props["num_marked"]++;
        }

        set_card_and_flags(deck, 0, card1, flags1 | CFLAG_SEEN);
        set_card_and_flags(deck, 1, card2, flags2 | CFLAG_SEEN);

        return true;
    }

    deck_peek_ident(deck);

    card3 = get_card_and_flags(deck, 2, flags3);
    retry_blank_card(card3, deck, flags3);

    int already_seen = 0;
    if (flags1 & CFLAG_SEEN)
        already_seen++;
    if (flags2 & CFLAG_SEEN)
        already_seen++;
    if (flags3 & CFLAG_SEEN)
        already_seen++;

    if (random2(3) < already_seen)
        deck.props["non_brownie_draws"]++;

    mprf("You draw three cards from the deck.  They are: %s, %s and %s.",
         card_name(card1), card_name(card2), card_name(card3));

    set_card_and_flags(deck, 0, card1, flags1 | CFLAG_SEEN);
    set_card_and_flags(deck, 1, card2, flags2 | CFLAG_SEEN);
    set_card_and_flags(deck, 2, card3, flags3 | CFLAG_SEEN);

    mpr("You shuffle the cards back into the deck.");
    shuffle_deck(deck);

    return true;
}

// Mark a deck: look at the next four cards, mark them, and shuffle
// them back into the deck without losing any cards.  The player won't
// know what order they're in, and the if the top card is non-marked
// then the player won't know what the next card is.
// Return false if the operation was failed/aborted along the way.
bool deck_mark()
{
    if ( !wielding_deck() )
    {
        mpr("You aren't wielding a deck!");
        crawl_state.zero_turns_taken();
        return false;
    }
    item_def& deck(you.inv[you.equip[EQ_WEAPON]]);
    if (check_buggy_deck(deck))
        return false;

    CrawlHashTable &props = deck.props;
    if (props["num_marked"].get_byte() > 0)
    {
        mpr("Deck is already marked.");
        crawl_state.zero_turns_taken();
        return false;
    }

    const int num_cards   = cards_in_deck(deck);
    const int num_to_mark = (num_cards < 4 ? num_cards : 4);

    if (num_cards == 1)
        mpr("There's only one card left!");
    else if (num_cards < 4)
        mprf("The deck only has %d cards.", num_cards);

    std::vector<std::string> names;
    for ( int i = 0; i < num_to_mark; ++i )
    {
        unsigned char flags;
        card_type     card = get_card_and_flags(deck, i, flags);

        flags |= CFLAG_SEEN | CFLAG_MARKED;
        set_card_and_flags(deck, i, card, flags);

        names.push_back(card_name(card));
    }
    mpr_comma_separated_list("You draw and mark ", names);
    props["num_marked"] = (char) num_to_mark;

    if (num_cards == 1)
        return true;

    if (num_cards < 4)
    {
        mprf("You shuffle the deck.");
        deck.plus2 = -num_cards;
    }
    else
        mprf("You shuffle the cards back into the deck.");

    shuffle_deck(deck);
    you.wield_change = true;

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
        crawl_state.zero_turns_taken();
        return false;
    }
    item_def& deck(you.inv[you.equip[EQ_WEAPON]]);
    if (check_buggy_deck(deck))
        return false;

    CrawlHashTable &props = deck.props;
    if (props["num_marked"].get_byte() > 0)
    {
        mpr("You can't stack a marked deck.");
        crawl_state.zero_turns_taken();
        return false;
    }

    const int num_cards    = cards_in_deck(deck);
    const int num_to_stack = (num_cards < 5 ? num_cards : 5);

    std::vector<card_type>     draws;
    std::vector<unsigned char> flags;
    for ( int i = 0; i < num_cards; ++i )
    {
        unsigned char _flags;
        card_type     card = draw_top_card(deck, false, _flags);

        if (i < num_to_stack)
        {
            draws.push_back(card);
            flags.push_back(_flags | CFLAG_SEEN | CFLAG_MARKED);
        }
        else
            ; // Rest of deck is discarded.
    }

    if ( num_cards == 1 )
        mpr("There's only one card left!");
    else if (num_cards < 5)
        mprf("The deck only has %d cards.", num_to_stack);
    else if (num_cards == 5)
        mpr("The deck has exactly five cards.");
    else 
        mprf("You draw the first five cards out of %d and discard the rest.",
             num_cards);
    more();

    while ( draws.size() > 1 )
    {
        mesclr();
        mpr("Order the cards (bottom to top)...", MSGCH_PROMPT);
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
        push_top_card(deck, draws[selected], flags[selected]);
        draws.erase(draws.begin() + selected);
        flags.erase(flags.begin() + selected);
    }
    mesclr();
    deck.plus2 = -num_to_stack;
    push_top_card(deck, draws[0], flags[0]);
    props["num_marked"] = (char) num_to_stack;
    you.wield_change = true;

    check_buggy_deck(deck);

    return true;
}

// Draw the next three cards, discard two and pick one.
bool deck_triple_draw()
{
    const int slot = choose_inventory_deck("Triple draw from which deck?");
    if ( slot == -1 )
    {
        crawl_state.zero_turns_taken();
        return false;
    }

    item_def& deck(you.inv[slot]);

    if (check_buggy_deck(deck))
        return false;

    CrawlHashTable &props = deck.props;
    if (props["num_marked"].get_byte() > 0)
    {
        mpr("You can't triple draw from a marked deck.");
        crawl_state.zero_turns_taken();
        return false;
    }

    const int num_cards = cards_in_deck(deck);

    if (num_cards == 1)
    {
        // only one card to draw, so just draw it
        evoke_deck(deck);
        return true;
    }

    const int num_to_draw = (num_cards < 3 ? num_cards : 3);
    std::vector<card_type>     draws;
    std::vector<unsigned char> flags;

    for ( int i = 0; i < num_to_draw; ++i )
    {
        unsigned char _flags;
        card_type     card = draw_top_card(deck, false, _flags);

        draws.push_back(card);
        flags.push_back(_flags | CFLAG_SEEN | CFLAG_MARKED);
    }

    mpr("You draw... (choose one card)");
    for ( int i = 0; i < num_to_draw; ++i )
        msg::streams(MSGCH_PROMPT) << (static_cast<char>(i + 'a')) << " - "
                                   << card_name(draws[i]) << std::endl;
    int selected = -1;
    while ( true )
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
    you.wield_change = true;

    // Make deck disappear *before* the card effect, since we
    // don't want to unwield an empty deck.
    deck_rarity_type rarity = deck_rarity(deck);
    if (cards_in_deck(deck) == 0)
    {
        mpr("The deck of cards disappears in a puff of smoke.");
        if ( slot == you.equip[EQ_WEAPON] )
            unwield_item();

        dec_inv_item_quantity( slot, 1 );
    }

    // Note that card_effect() might cause you to unwield the deck.
    card_effect(draws[selected], rarity, flags[selected], false);

    return true;
}

// This is Nemelex retribution.
void draw_from_deck_of_punishment()
{
    bool      oddity;
    card_type card = random_card(MISC_DECK_OF_PUNISHMENT, oddity);

    mpr("You draw a card...");
    card_effect(card, DECK_RARITY_COMMON);
}

static int xom_check_card(item_def &deck, card_type card,
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
        // Handled elswehre
        amusement = 0;
        break;

    case CARD_BLANK1:
    case CARD_BLANK2:
        // Boring
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

// In general, if the next cards in a deck are known, they will
// be stored in plus2 (the next card) and special (up to 4 cards
// after that, bitpacked.)
// Hidden assumption: no more than 126 cards, otherwise the 5th
// card could clobber the sign bit in special.
void evoke_deck( item_def& deck )
{
    if (check_buggy_deck(deck))
        return;

    int brownie_points = 0;
    bool allow_id = in_inventory(deck) && !item_ident(deck, ISFLAG_KNOW_TYPE);

    unsigned char    flags      = 0;
    card_type        card       = draw_top_card(deck, true, flags);
    int              amusement  = xom_check_card(deck, card, flags);
    deck_rarity_type rarity     = deck_rarity(deck);
    CrawlHashTable  &props      = deck.props;
    bool             no_brownie = (props["non_brownie_draws"].get_byte() > 0);

    // Do these before the deck item_def object is gone.
    if (flags & CFLAG_MARKED)
        props["num_marked"]--;
    if (no_brownie)
        props["non_brownie_draws"]--;

    deck.plus2++;

    // Get rid of the deck *before* the card effect because a card
    // might cause a wielded deck to be swapped out for something else,
    // in which case we don't want an empty deck to go through the
    // swapping process.
    const bool deck_gone = (cards_in_deck(deck) == 0);
    if ( deck_gone )
    {
        mpr("The deck of cards disappears in a puff of smoke.");
        dec_inv_item_quantity( deck.link, 1 );
        // Finishing the deck will earn a point, even if it
        // was marked or stacked.
        brownie_points++;
    }

    const bool fake_draw = !card_effect(card, rarity, flags, false);
    if ( fake_draw && !deck_gone )
        props["non_brownie_draws"]++;

    if (!(flags & CFLAG_MARKED))
    {
        // Could a Xom worshipper ever get a stacked deck in the first
        // place?
        xom_is_stimulated(amusement);

        // Nemelex likes gamblers.
        if (!no_brownie)
        {
            brownie_points = 1;
            if (one_chance_in(3))
                brownie_points++;
        }

        // You can't ID off a marked card
        allow_id = false;
    }

    if (!deck_gone && allow_id
        && (you.skills[SK_EVOCATIONS] > 5 + random2(35)))
    {
        mpr("Your skill with magical items lets you identify the deck.");
        set_ident_flags( deck, ISFLAG_KNOW_TYPE );
        msg::streams(MSGCH_EQUIPMENT) << deck.name(DESC_INVENTORY)
                                      << std::endl;
    }

    if ( !fake_draw )
        did_god_conduct(DID_CARDS, brownie_points);

    // Always wield change, since the number of cards used/left has
    // changed.
    you.wield_change = true;
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
    you.redraw_hit_points = true;
    you.redraw_magic_points = true;
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
        const brand_type brands[] = {
            SPWPN_FLAMING, SPWPN_FREEZING, SPWPN_VENOM, SPWPN_DRAINING,
            SPWPN_VORPAL, SPWPN_DISTORTION, SPWPN_PAIN, SPWPN_DUMMY_CRUSHING
        };

        if ( !brand_weapon(RANDOM_ELEMENT(brands), random2(power/4)) )
        {
            if ( you.equip[EQ_WEAPON] == -1 )
            {
                msg::stream << "Your " << your_hand(true) << " twitch."
                            << std::endl;
            }
            else
            {
                msg::stream << "Your weapon vibrates for a moment."
                            << std::endl;
            }
        }
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

    // Did focusing kill the player?
    kill_method_type kill_types[3] = {
        KILLED_BY_WEAKNESS,
        KILLED_BY_CLUMSINESS,
        KILLED_BY_STUPIDITY
    };

    std::string cause = "the Focus card";

    if (crawl_state.is_god_acting())
    {
        god_type which_god = crawl_state.which_god_acting();
        if (crawl_state.is_god_retribution()) {
            cause  = "the wrath of ";
            cause += god_name(which_god);
        }
        else
        {
            if (which_god == GOD_XOM)
                cause = "the capriciousness of Xom";
            else
            {
                cause = "the 'helpfullness' of ";
                cause += god_name(which_god);
            }
        }
    }
                
    for ( int i = 0; i < 3; ++i )
        if (*max_statp[i] < 1 || *base_statp[i] < 1)
            ouch(INSTANT_DEATH, 0, kill_types[i], cause.c_str(), true);

    // The player survived!
    you.redraw_strength = true;
    you.redraw_intelligence = true;
    you.redraw_dexterity = true;

    burden_change();
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
        if (crawl_state.is_god_retribution()) {
            cause  = "the wrath of ";
            cause += god_name(which_god);
        }
        else
        {
            if (which_god == GOD_XOM)
                cause = "the capriciousness of Xom";
            else
            {
                cause = "the 'helpfulness' of ";
                cause += god_name(which_god);
            }
        }
    }
                
    for ( int i = 0; i < 3; ++i )
        if (new_base[i] < 1 || new_max[i] < 1)
            ouch(INSTANT_DEATH, 0, kill_types[i], cause.c_str(), true);

    // The player survived!

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

    burden_change();
}

static void helix_card(int power, deck_rarity_type rarity)
{
    mutation_type bad_mutations[] = {
        MUT_FAST_METABOLISM, MUT_WEAK, MUT_DOPEY, MUT_CLUMSY,
        MUT_TELEPORT, MUT_DEFORMED, MUT_SCREAM, MUT_DETERIORATION,
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
    else
        mpr("You feel transcendent for a moment.");
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
        const int mapidx = random_map_for_tag("trowel_portal", false, false);
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
    {
        mpr("A genie takes forms and thunders: "
            "\"Choose your reward, mortal!\"");
        more();
        acquirement( OBJ_RANDOM, AQ_CARD_GENIE );
    }
    else
    {
        mpr("A genie takes form and thunders: "
            "\"You disturbed me, fool!\"");
        // use 41 not 40 to tell potion_effect() that this isn't
        // a real potion
        potion_effect( coinflip() ? POT_DEGENERATION : POT_DECAY, 41 );
    }
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
                                    250, false, false, false, true );
    
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

bool card_effect(card_type which_card, deck_rarity_type rarity,
                 unsigned char flags, bool tell_card)
{
    bool rc = true;
    const int power = card_power(rarity);
#ifdef DEBUG_DIAGNOSTICS
    msg::streams(MSGCH_DIAGNOSTICS) << "Card power: " << power
                                    << ", rarity: " << static_cast<int>(rarity)
                                    << std::endl;
#endif

    if (tell_card)
        msg::stream << "You have drawn " << card_name( which_card )
                    << '.' << std::endl;

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
    case CARD_BLANK1: break;
    case CARD_BLANK2: break;
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
    case CARD_TOMB:             entomb(power); break;
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
                        random2(power), 0, "a card of wild magic" );
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

    if (you.religion == GOD_XOM
        && (which_card == CARD_BLANK1 || which_card == CARD_BLANK2 || !rc))
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
        return false;

    unsigned char flags;
    get_card_and_flags(deck, -1, flags);

    return (flags & CFLAG_MARKED);
}

card_type top_card(const item_def &deck)
{
    if (!is_deck(deck))
        return NUM_CARDS;

    unsigned char flags;
    card_type card = get_card_and_flags(deck, -1, flags);

    UNUSED(flags);

    return card;
}

bool is_deck(const item_def &item)
{
    return item.base_type == OBJ_MISCELLANY
        && (item.sub_type >= MISC_DECK_OF_ESCAPE &&
            item.sub_type <= MISC_DECK_OF_DEFENSE);
}

bool bad_deck(const item_def &item)
{
    if (!is_deck(item))
        return false;

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

    props["cards"].new_vector(SV_BYTE).resize(item.plus);
    props["card_flags"].new_vector(SV_BYTE).resize(item.plus);

    for (int i = 0; i < item.plus; i++)
    {
        bool      was_odd = false;
        card_type card    = random_card(item, was_odd);

        unsigned char flags = 0;
        if (was_odd)
            flags = CFLAG_ODDITY;

        set_card_and_flags(item, i, card, flags);
    }

    ASSERT(cards_in_deck(item) == (unsigned long) item.plus);

    props["num_marked"]        = (char) 0;
    props["non_brownie_draws"] = (char) 0;

    props.assert_validity();

    item.plus2  = 0;
    item.colour = deck_rarity_to_color((deck_rarity_type) item.special);
}
