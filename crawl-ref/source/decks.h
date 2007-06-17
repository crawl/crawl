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

const char* card_name(card_type card);
void evoke_deck(item_def& deck);
bool deck_triple_draw();
bool deck_peek();
bool deck_stack();
void card_effect(card_type which_card, deck_rarity_type rarity);
void draw_from_deck_of_punishment();

#endif
