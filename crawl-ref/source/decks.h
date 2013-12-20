/**
 * @file
 * @brief Functions with decks of cards.
**/

#ifndef DECKS_H
#define DECKS_H

#include "enum.h"

#include "externs.h"

// DECK STRUCTURE: deck.plus is the number of cards the deck *started*
// with, deck.plus2 is the number of cards drawn, deck.special is the
// deck rarity, deck.props["cards"] holds the list of cards (with the
// highest index card being the top card, and index 0 being the bottom
// card), deck.props.["card_flags"] holds the flags for each card,
// deck.props["num_marked"] is the number of marked cards left in the
// deck.
//
// The card type and per-card flags are each stored as unsigned bytes,
// for a maximum of 256 different kinds of cards and 8 bits of flags.

enum deck_rarity_type
{
    DECK_RARITY_RANDOM,
    DECK_RARITY_COMMON,
    DECK_RARITY_RARE,
    DECK_RARITY_LEGENDARY,
};

enum deck_type
{
    // pure decks
    DECK_OF_ESCAPE,
    DECK_OF_DESTRUCTION,
    DECK_OF_DUNGEONS,
    DECK_OF_SUMMONING,
    DECK_OF_WONDERS,
};

enum card_flags_type
{
    CFLAG_ODDITY = (1 << 0),
    CFLAG_SEEN   = (1 << 1),
    CFLAG_MARKED = (1 << 2),
    CFLAG_DEALT  = (1 << 4),
};

enum card_type
{
    CARD_PORTAL,                // "the mover"
    CARD_WARP,                  // "the jumper"
    CARD_SWAP,                  // "swap"
    CARD_VELOCITY,              // "the runner"

    CARD_TOMB,                  // "the wall"
    CARD_BANSHEE,               // "the scream"
    CARD_DAMNATION,             // banishment
    CARD_SOLITUDE,              // dispersal
    CARD_WARPWRIGHT,            // create teleport trap
    CARD_FLIGHT,

    CARD_VITRIOL,               // acid damage
    CARD_FLAME,                 // fire damage
    CARD_FROST,                 // cold damage
    CARD_VENOM,                 // poison damage
    CARD_HAMMER,                // pure damage
    CARD_SPARK,                 // lightning damage
    CARD_PAIN,                  // single target, like spell of agony
    CARD_TORMENT,               // Symbol of Torment
    CARD_ORB,

    CARD_ELIXIR,                // healing
    CARD_BATTLELUST,            // melee boosts
    CARD_METAMORPHOSIS,         // transformation
    CARD_HELM,                  // defence
    CARD_BLADE,                 // weapon boosts
    CARD_SHADOW,                // assassin skills
    CARD_MERCENARY,

    CARD_CRUSADE,
    CARD_SUMMON_ANIMAL,
    CARD_SUMMON_DEMON,
    CARD_SUMMON_WEAPON,
    CARD_SUMMON_FLYING,         // wisps and butterflies
    CARD_SUMMON_SKELETON,
    CARD_SUMMON_UGLY,

    CARD_POTION,
    CARD_FOCUS,
    CARD_SHUFFLE,
    CARD_EXPERIENCE,
    CARD_WILD_MAGIC,
    CARD_SAGE,                  // skill training
    CARD_HELIX,                 // remove one *bad* mutation
    CARD_ALCHEMIST,

    CARD_WATER,                 // flood squares
    CARD_GLASS,                 // make walls transparent
    CARD_DOWSING,               // mapping/detect SD/traps/items/monsters
    CARD_TROWEL,                // create feature/vault
    CARD_MINEFIELD,             // plant traps
    CARD_STAIRS,                // moves stairs around

    CARD_GENIE,                 // acquirement OR rotting/deterioration
    CARD_BARGAIN,               // shopping discount
    CARD_WRATH,                 // Godly wrath
    CARD_WRAITH,                // drain XP
    CARD_XOM,
    CARD_FEAST,
    CARD_FAMINE,
    CARD_CURSE,                 // Curse your items
    CARD_SWINE,                 // *oink*

    NUM_CARDS
};

const char* card_name(card_type card);
void evoke_deck(item_def& deck);
bool deck_triple_draw();
bool deck_peek();
bool deck_deal();
bool deck_stack();
bool choose_deck_and_draw();
void nemelex_shuffle_decks();
void shuffle_all_decks_on_level();

bool draw_three(int slot);
bool stack_five(int slot);
bool recruit_mercenary(int mid);

void card_effect(card_type which_card, deck_rarity_type rarity,
                 uint8_t card_flags = 0, bool tell_card = true);
void draw_from_deck_of_punishment(bool deal = false);

bool      top_card_is_known(const item_def &item);
card_type top_card(const item_def &item);

bool is_deck(const item_def &item);
bool bad_deck(const item_def &item);
deck_rarity_type deck_rarity(const item_def &item);
colour_t deck_rarity_to_colour(deck_rarity_type rarity);
void init_deck(item_def &item);

int cards_in_deck(const item_def &deck);
card_type get_card_and_flags(const item_def& deck, int idx,
                             uint8_t& _flags);

// Used elsewhere in ZotDef.
void sage_card(int power, deck_rarity_type rarity);
void create_pond(const coord_def& center, int radius, bool allow_deep);

const vector<card_type> get_drawn_cards(const item_def& deck);
// see and mark the first card with a scroll.
bool deck_identify_first(int slot);

#endif
