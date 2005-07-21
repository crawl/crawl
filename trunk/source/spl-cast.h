/*
 *  File:       spl-cast.cc
 *  Summary:    Spell casting functions.
 *  Written by: Linley Henzell
 *
 *  Change History (most recent first):
 *
 *               <1>     -/--/--        LRH             Created
 */


#ifndef SPL_CAST_H
#define SPL_CAST_H

char list_spells( void );
int spell_fail( int spell );
int calc_spell_power( int spell, bool apply_intel );
int spell_enhancement( unsigned int typeflags );

// last updaetd 12may2000 {dlb}
/* ***********************************************************************
 * called from: it_use3 - spell
 * *********************************************************************** */
void exercise_spell( int spell_ex, bool spc, bool divide );


// last updaetd 12may2000 {dlb}
/* ***********************************************************************
 * called from: acr
 * *********************************************************************** */
bool cast_a_spell( void );


// last updaetd 12may2000 {dlb}
/* ***********************************************************************
 * called from: ability - debug - it_use3 - spell
 * *********************************************************************** */
bool your_spells( int spc2, int powc = 0, bool allow_fail = true );

// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: acr - decks - fight - it_use2 - it_use3 - item_use - items -
 *              misc - mstuff2 - religion - spell - spl-book - spells4
 * *********************************************************************** */
bool miscast_effect( unsigned int sp_type, int mag_pow, int mag_fail, 
                     int force_effect, const char *cause = NULL );


#endif
