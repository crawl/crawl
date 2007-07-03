/*
 *  File:       spl-cast.cc
 *  Summary:    Spell casting functions.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 *
 *  Change History (most recent first):
 *
 *               <1>     -/--/--        LRH             Created
 */


#ifndef SPL_CAST_H
#define SPL_CAST_H

#include "enum.h"

class formatted_string;

char list_spells( void );
int spell_fail( spell_type spell );
int calc_spell_power(spell_type spell, bool apply_intel,
                     bool fail_rate_chk = false );
int spell_enhancement( unsigned int typeflags );

// last updaetd 12may2000 {dlb}
/* ***********************************************************************
 * called from: it_use3 - spell
 * *********************************************************************** */
void exercise_spell( spell_type spell_ex, bool spc, bool divide );


// last updaetd 12may2000 {dlb}
/* ***********************************************************************
 * called from: acr
 * *********************************************************************** */
bool cast_a_spell( void );

void inspect_spells();


// last updaetd 12may2000 {dlb}
/* ***********************************************************************
 * called from: ability - debug - it_use3 - spell
 * *********************************************************************** */
spret_type your_spells( spell_type spell, int powc = 0,
                        bool allow_fail = true );

// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: acr - decks - fight - it_use2 - it_use3 - item_use - items -
 *              misc - mstuff2 - religion - spell - spl-book - spells4
 * *********************************************************************** */
bool miscast_effect( unsigned int sp_type, int mag_pow, int mag_fail, 
                     int force_effect, const char *cause = NULL );

const char* failure_rate_to_string( int fail );

int spell_power_colour(spell_type spell);
int spell_power_bars(spell_type spell);
formatted_string spell_power_string(spell_type spell);

const char* spell_hunger_string( spell_type spell );

#endif
