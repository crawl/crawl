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


// last updaetd 12may2000 {dlb}
/* ***********************************************************************
 * called from: ability - debug - it_use3 - spell
 * *********************************************************************** */
spret_type your_spells( spell_type spc2, int powc = 0, bool allow_fail = true );

// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: acr - decks - fight - it_use2 - it_use3 - item_use - items -
 *              misc - mstuff2 - religion - spell - spl-book - spells4
 * *********************************************************************** */
bool miscast_effect( unsigned int sp_type, int mag_pow, int mag_fail, 
                     int force_effect, const char *cause = NULL );

const char* failure_rate_to_string( int fail );
const char* spell_power_string( spell_type spell );

#endif
