/*
 *  File:       spl-book.h
 *  Summary:    Some spellbook related functions.
 *  Written by: Josh Fishman
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 */


#ifndef SPL_BOOK_H
#define SPL_BOOK_H

#include "externs.h"
#include "FixVec.h"

#define SPELLBOOK_SIZE 8

class formatted_string;

enum read_book_action_type
{
    RBOOK_USE_STAFF,
    RBOOK_MEMORISE,
    RBOOK_READ_SPELL
};

// updated 24may2000 {dlb}
/* ***********************************************************************
 * called from: dungeon - effects - shopping
 * *********************************************************************** */
int  book_rarity(unsigned char which_book);
int  spell_rarity(spell_type which_spell);
void init_spell_rarities();

bool is_valid_spell_in_book( const item_def &book, int spell );
bool is_valid_spell_in_book( int splbook, int spell );

void mark_had_book(const item_def &book);
void mark_had_book(int booktype);

// updated 24may2000 {dlb}
/* ***********************************************************************
 * called from: it_use3 - item_use - spl-book
 * *********************************************************************** */
int read_book( item_def &item, read_book_action_type action );


// updated 24may2000 {dlb}
/* ***********************************************************************
 * called from: acr
 * *********************************************************************** */
bool player_can_memorise(const item_def &book);
bool learn_spell(int book = -1);

bool player_can_read_spellbook( const item_def &book );

spell_type which_spell_in_book(const item_def &book, int spl);
spell_type which_spell_in_book(int sbook_type, int spl);

// returns amount practised (or -1 for abort)
int staff_spell( int zap_device_2 );
bool is_memorised(spell_type spell);

bool undead_cannot_memorise(spell_type spell, char being);

int spellbook_contents( item_def &book, read_book_action_type action,
                        formatted_string *fs = NULL );

int count_staff_spells(const item_def &item, bool need_id);
int rod_shield_leakage();

bool make_book_level_randart(item_def &book, int level = -1,
                             int num_spells = -1, std::string owner = "");
bool make_book_theme_randart(item_def &book,
                             int disc1 = 0, int disc2 = 0,
                             int num_spells = -1, int max_levels = -1,
                             spell_type incl_spell = SPELL_NO_SPELL,
                             std::string owner = "");
void make_book_Roxanne_special(item_def *book);

bool book_has_title(const item_def &book);
#endif
