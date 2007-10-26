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


#ifndef DECKS_H
#define DECKS_H

#include "enum.h"

#include "externs.h"

enum deck_rarity_type
{
    DECK_RARITY_COMMON,
    DECK_RARITY_RARE,
    DECK_RARITY_LEGENDARY
};

enum deck_type
{
    // pure decks
    DECK_OF_ESCAPE,
    DECK_OF_DESTRUCTION,
    DECK_OF_DUNGEONS,
    DECK_OF_SUMMONING,
    DECK_OF_WONDERS
};

const char* card_name(card_type card);
void evoke_deck(item_def& deck);
bool deck_triple_draw();
bool deck_peek();
bool deck_stack();
bool choose_deck_and_draw();
bool card_effect(card_type which_card, deck_rarity_type rarity);
void draw_from_deck_of_punishment();

bool is_deck(const item_def &item);
deck_rarity_type deck_rarity(const item_def &item);

#endif
