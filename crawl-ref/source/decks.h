/**
 * @file
 * @brief Functions with decks of cards.
**/

#ifndef DECKS_H
#define DECKS_H

#include "enum.h"

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

enum deck_type
{
    // pure decks
    DECK_OF_ESCAPE,
    DECK_OF_DESTRUCTION,
#if TAG_MAJOR_VERSION == 34
    DECK_OF_DUNGEONS,
#endif
    DECK_OF_SUMMONING,
    DECK_OF_WONDERS,
};

enum card_flags_type
{
    CFLAG_ODDITY     = (1 << 0),
    CFLAG_SEEN       = (1 << 1),
    CFLAG_MARKED     = (1 << 2),
    CFLAG_PUNISHMENT = (1 << 3),
    CFLAG_DEALT      = (1 << 4),
};

enum card_type
{
#if TAG_MAJOR_VERSION == 34
    CARD_PORTAL,                // "the mover"
    CARD_WARP,                  // "the jumper"
#endif
    CARD_SWAP,                  // "swap"
    CARD_VELOCITY,              // "the runner"

    CARD_TOMB,                  // "the wall"
    CARD_BANSHEE,               // "the scream"
    CARD_DAMNATION,             // banishment
    CARD_SOLITUDE,              // dispersal
    CARD_WARPWRIGHT,            // create teleport trap
    CARD_SHAFT,

    CARD_VITRIOL,               // acid damage
    CARD_CLOUD,                 // fire/cold clouds
    CARD_HAMMER,                // plain damage
    CARD_VENOM,                 // poison damage
    CARD_FORTITUDE,             // strength and damage shaving
    CARD_STORM,
    CARD_PAIN,                  // single target, like spell of agony
    CARD_TORMENT,               // Symbol of Torment
    CARD_ORB,

    CARD_ELIXIR,                // healing
#if TAG_MAJOR_VERSION == 34
    CARD_BATTLELUST,            // melee boosts
    CARD_METAMORPHOSIS,         // transformation
#endif
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
#if TAG_MAJOR_VERSION == 34
    CARD_SHUFFLE,
    CARD_EXPERIENCE,
#endif
    CARD_WILD_MAGIC,
#if TAG_MAJOR_VERSION == 34
    CARD_SAGE,                  // skill training
#endif
    CARD_HELIX,                 // remove one *bad* mutation
    CARD_ALCHEMIST,

#if TAG_MAJOR_VERSION == 34
    CARD_WATER,                 // flood squares
    CARD_GLASS,                 // make walls transparent
#endif
    CARD_DOWSING,               // mapping/detect SD/traps/items/monsters
#if TAG_MAJOR_VERSION == 34
    CARD_TROWEL,                // create feature/vault
    CARD_MINEFIELD,             // plant traps
#endif
    CARD_STAIRS,                // moves stairs around

#if TAG_MAJOR_VERSION == 34
    CARD_GENIE,                 // acquirement OR rotting/deterioration
    CARD_BARGAIN,               // shopping discount
#endif
    CARD_WRATH,                 // Godly wrath
    CARD_WRAITH,                // drain XP
    CARD_XOM,
    CARD_FEAST,
    CARD_FAMINE,
    CARD_CURSE,                 // Curse your items
    CARD_SWINE,                 // *oink*
    CARD_ILLUSION,
    CARD_DEGEN,

    NUM_CARDS
};

const char* card_name(card_type card);
card_type name_to_card(string name);
const string deck_contents(uint8_t deck_type);
void evoke_deck(item_def& deck);
bool deck_triple_draw();
bool deck_deal();
string which_decks(card_type card);
bool deck_stack();
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
colour_t deck_rarity_to_colour(deck_rarity_type rarity);
void init_deck(item_def &item);

int cards_in_deck(const item_def &deck);
card_type get_card_and_flags(const item_def& deck, int idx,
                             uint8_t& _flags);

const vector<card_type> get_drawn_cards(const item_def& deck);
// see and mark the first card with a scroll.
bool deck_identify_first(int slot);

#endif
