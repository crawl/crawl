/*
 *  File:       it_use3.cc
 *  Summary:    Functions for using some of the wackier inventory items.
 *  Written by: Linley Henzell
 *
 *  Change History (most recent first):
 *
 *               <1>     -/--/--        LRH             Created
 */


#ifndef IT_USE3_H
#define IT_USE3_H


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: item_use
 * *********************************************************************** */
void skill_manual(char sc_read_2);


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: item_use - spl-book
 * *********************************************************************** */
void tome_of_power(char sc_read_2);


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: acr
 * *********************************************************************** */
bool evoke_wielded(void);


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: acr
 * *********************************************************************** */
void special_wielded(void);


#endif
