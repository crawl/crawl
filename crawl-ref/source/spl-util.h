/*
 *  File:       spl-util.h
 *  Summary:    data handlers for player spell list
 *  Written by: don brodale <dbrodale@bigfootinteractive.com>
 *
 *  Changelog(most recent first):
 *
 *           24jun2000     jmf     simplified structures
 *  <00>     12jun2000     dlb     created after much thought
 */


#ifndef SPL_UTIL_H
#define SPL_UTIL_H

#include "enum.h"    // just for NUM_SPELL_TYPES and TARG_ENEMY
#include "direct.h"  // just for DIR_NONE 

struct playerspell
{
  int id;
  const char *title;
  unsigned int disciplines; //jmf: a bitfield
  unsigned int level;
};


//* * called from: acr
void init_playerspells(void);

int get_spell_slot_by_letter( char letter );
int get_spell_by_letter( char letter );

bool add_spell_to_memory( int spell );
bool del_spell_from_memory_by_slot( int slot );

// * called from: spell
int spell_hunger(int which_spell);

// * called from: it_use3 - spell - spells3
int spell_mana(int which_spell);

// * called from: chardump - it_use3 - player - spell - spl-book -
// *              spells0 - spells3
int spell_difficulty(int which_spell);

int spell_levels_required(int which_spell);

// * called from: chardump - spell - spl-book - spells0
bool spell_typematch(int which_spell, unsigned int which_discipline);
unsigned int spell_type( int which_spell ); //jmf: simplification of above
int count_bits( unsigned int bits );

// * called from: chardump - command - debug - spl-book - spells0
const char *spell_title(int which_spell);

//int spell_restriction(int which_spell, int which_restriction);

int apply_area_visible(int (*func) (int, int, int, int), int power);

int apply_area_square(int (*func) (int, int, int, int),
                              int cx, int cy, int power);

int apply_area_around_square( int (*func) (int, int, int, int),
                              int targ_x, int targ_y, int power );

int apply_random_around_square( int (*func) (int, int, int, int),
                                int targ_x, int targ_y, bool hole_in_middle, 
                                int power, int max_targs );

int apply_one_neighbouring_square(int (*func) (int, int, int, int),
                                          int power);

int apply_area_within_radius(int (*func) (int, int, int, int),
                              int x, int y, int pow, int radius, int ctype);

char spell_direction( struct dist &spelld, struct bolt &pbolt,
                              int restrict = DIR_NONE, int mode = TARG_ENEMY );

void apply_area_cloud(int (*func) (int, int, int, int), int x, int y,
                      int pow, int number, int ctype);

const char *spelltype_name(unsigned int which_spelltype);

int spell_type2skill (unsigned int which_spelltype);

#endif
