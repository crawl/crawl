/*
 *  File:       spl-book.h
 *  Summary:    Some spellbook related functions.
 *  Written by: Josh Fishman
 *
 *  Change History (most recent first):
 *
 * 22mar2000   jmf   Created
 */


#ifndef SPL_BOOK_H
#define SPL_BOOK_H

#include "externs.h"
#include "FixVec.h"


// used in dungeon.cc, it_use3.cc, spl-book.cc, spl-book.h - {dlb}
#define SPELLBOOK_SIZE 9
// used in spl-book.cc, spl-book.h - {dlb}
#define NUMBER_SPELLBOOKS 60


// updated 24may2000 {dlb}
/* ***********************************************************************
 * called from: dungeon - effects - shopping
 * *********************************************************************** */
char book_rarity(unsigned char which_book);


// updated 24may2000 {dlb}
/* ***********************************************************************
 * called from: it_use3 - item_use - spl-book
 * *********************************************************************** */
bool is_valid_spell_in_book( unsigned int splbook, int spell );


// updated 24may2000 {dlb}
/* ***********************************************************************
 * called from: it_use3 - item_use - spl-book
 * *********************************************************************** */
unsigned char read_book( item_def &item, int action );


// updated 24may2000 {dlb}
/* ***********************************************************************
 * called from: acr
 * *********************************************************************** */
bool learn_spell(void);


// updated 24may2000 {dlb}
/* ***********************************************************************
 * called from: it_use3 - item_use - spl-book
 * *********************************************************************** */
int which_spell_in_book(int sbook_type, int spl);

int staff_spell( int zap_device_2 );

bool undead_cannot_memorise(unsigned char spell, unsigned char being);

#endif
