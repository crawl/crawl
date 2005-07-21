/*
 *  File:       mutation.cc
 *  Summary:    Functions for handling player mutations.
 *  Written by: Linley Henzell
 *
 *  Change History (most recent first):
 *
 *               <1>     -/--/--        LRH             Created
 */


#ifndef MUTATION_H
#define MUTATION_H


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: acr - decks - effects - fight - food - it_use2 - items -
 *              mutation - religion - spell - spells
 * *********************************************************************** */
bool mutate(int which_mutation, bool failMsg = true);


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: acr
 * *********************************************************************** */
void display_mutations(void);


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: decks - it_use2 - mutation - spells
 * *********************************************************************** */
bool delete_mutation(char which_mutation);


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: chardump
 * *********************************************************************** */
// default of level == -1, means to use the player's current level
const char *mutation_name( char which_mutat, int level = -1 );


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: religion
 * *********************************************************************** */
bool give_good_mutation( bool failMsg = true );

// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: items - religion
 * *********************************************************************** */
bool give_cosmetic_mutation( void );

// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: items - spells
 * *********************************************************************** */
bool give_bad_mutation( bool forceMutation = false, bool failMsg = true );

// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: player
 * *********************************************************************** */
void demonspawn(void);


#endif
