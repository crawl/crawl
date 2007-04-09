/*
 *  File:       spl-util.h
 *  Summary:    data handlers for player spell list
 *  Written by: don brodale <dbrodale@bigfootinteractive.com>
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
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

struct spell_desc
{
    int id;
    const char  *title;
    unsigned int disciplines; // bitfield
    unsigned int flags;       // bitfield
    unsigned int level;
    const char  *target_prompt;

    // If a monster is casting this, does it need a tracer?
    bool         ms_needs_tracer;

    // The spell can be used no matter what the monster's foe is.
    bool         ms_utility;
};


//* * called from: acr
void init_spell_descs(void);

int get_spell_slot_by_letter( char letter );
spell_type get_spell_by_letter( char letter );

bool add_spell_to_memory( spell_type spell );
bool del_spell_from_memory_by_slot( int slot );

// * called from: spell
int spell_hunger(spell_type which_spell);

// * called from: it_use3 - spell - spells3
int spell_mana(spell_type which_spell);

// * called from: chardump - it_use3 - player - spell - spl-book -
// *              spells0 - spells3
int spell_difficulty(spell_type which_spell);
const char *get_spell_target_prompt( spell_type which_spell );

bool spell_needs_tracer(spell_type spell);
bool spell_needs_foe(spell_type spell);
int spell_levels_required(spell_type which_spell);

unsigned int get_spell_flags( spell_type which_spell );

// * called from: chardump - spell - spl-book - spells0
bool spell_typematch(int which_spell, unsigned int which_discipline);
unsigned int get_spell_type( int which_spell ); //jmf: simplification of above
int count_bits( unsigned int bits );

// * called from: chardump - command - debug - spl-book - spells0
const char *spell_title(spell_type which_spell);

const char* spelltype_short_name( int which_spelltype );

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
                      targeting_type restrict = DIR_NONE,
                      int mode = TARG_ENEMY,
                      const char *prompt = NULL );

void apply_area_cloud(int (*func) (int, int, int, int, kill_category),
                      int x, int y, int pow, int number, int ctype,
                      kill_category);

const char *spelltype_name(unsigned int which_spelltype);

int spell_type2skill (unsigned int which_spelltype);

#endif
