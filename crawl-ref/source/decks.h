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
    CARD_PORTAL,              // teleport, maybe controlled
    CARD_WARP,                // blink, maybe controlled
#endif
    CARD_SWAP,                // player and monster position
    CARD_VELOCITY,            // remove slow, alter others' speeds

    CARD_TOMB,                // a ring of rock walls
    CARD_BANSHEE,             // cause fear and drain
    CARD_DAMNATION,           // banish others, maybe self
    CARD_SOLITUDE,            // dispersal
    CARD_WARPWRIGHT,          // create teleport trap
    CARD_SHAFT,               // under the user, maybe others

    CARD_VITRIOL,             // acid damage
    CARD_CLOUD,               // encage enemies in rings of clouds
    CARD_HAMMER,              // straightforward earth conjurations
    CARD_VENOM,               // poison damage, maybe poison vuln
    CARD_FORTITUDE,           // strength and damage shaving
    CARD_STORM,               // wind and rain
    CARD_PAIN,                // necromancy, manipulating life itself
    CARD_TORMENT,             // symbol of
    CARD_ORB,                 // pure bursts of energy

    CARD_ELIXIR,              // restoration of hp and mp
#if TAG_MAJOR_VERSION == 34
    CARD_BATTLELUST,          // melee boosts
    CARD_METAMORPHOSIS,       // transmutations
#endif
    CARD_HELM,                // defence boosts
    CARD_BLADE,               // cleave status
    CARD_SHADOW,              // stealth and darkness
    CARD_MERCENARY,           // costly perma-ally

    CARD_CRUSADE,             // aura of abjuration and mass enslave
#if TAG_MAJOR_VERSION == 34
    CARD_SUMMON_ANIMAL,       // scattered herd
#endif
    CARD_SUMMON_DEMON,        // dual demons
    CARD_SUMMON_WEAPON,       // a dance partner
    CARD_SUMMON_FLYING,       // swarms from the swamp
#if TAG_MAJOR_VERSION == 34
    CARD_SUMMON_SKELETON,     // bones, bones, bones
#endif
    CARD_SUMMON_UGLY,         // or very, or both

    CARD_POTION,              // random boost, probably also for allies
    CARD_FOCUS,               // lowest stat down, highest stat up
#if TAG_MAJOR_VERSION == 34
    CARD_SHUFFLE,             // stats, specifically
    CARD_EXPERIENCE,          // like the potion
#endif
    CARD_WILD_MAGIC,          // miscasts for everybody
#if TAG_MAJOR_VERSION == 34
    CARD_SAGE,                // skill training
#endif
    CARD_HELIX,               // precision mutation alteration
    CARD_ALCHEMIST,           // health / mp for gold

#if TAG_MAJOR_VERSION == 34
    CARD_WATER,               // flood squares
    CARD_GLASS,               // make walls transparent
#endif
    CARD_DOWSING,             // mapping/detect traps/items/monsters
#if TAG_MAJOR_VERSION == 34
    CARD_TROWEL,              // create altars, statues, portal
    CARD_MINEFIELD,           // plant traps
#endif
    CARD_STAIRS,              // moves stairs around

#if TAG_MAJOR_VERSION == 34
    CARD_GENIE,               // acquirement or rotting/deterioration
    CARD_BARGAIN,             // shopping discount
#endif
    CARD_WRATH,               // random godly wrath
    CARD_WRAITH,              // drain XP
    CARD_XOM,                 // 's attention turns to you
    CARD_FEAST,               // engorged
    CARD_FAMINE,              // starving
    CARD_CURSE,               // curse your items
    CARD_SWINE,               // *oink*

    CARD_ILLUSION,            // a copy of the player
    CARD_DEGEN,               // polymorph hostiles down hd, malmutate
    CARD_ELEMENTS,            // primal animals of the elements
    CARD_RANGERS,             // sharpshooting
    CARD_PLACID_MAGIC,        // cancellation and antimagic

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
