/*
 *  File:       food.cc
 *  Summary:    Functions for eating and butchering.
 *  Written by: Linley Henzell
 *
 *  Change History (most recent first):
 *
 *               <1>     -/--/--        LRH             Created
 */


#ifndef FOOD_H
#define FOOD_H


// last updated 19jun2000 {dlb}
/* ***********************************************************************
 * called from: acr
 * *********************************************************************** */
bool butchery(void);


// last updated 19jun2000 {dlb}
/* ***********************************************************************
 * called from: acr
 * *********************************************************************** */
void eat_food(void);


// last updated 19jun2000 {dlb}
/* ***********************************************************************
 * called from: abl-show - acr - fight - food - spell
 * *********************************************************************** */
void make_hungry(int hunger_amount, bool suppress_msg);


// last updated 19jun2000 {dlb}
/* ***********************************************************************
 * called from: acr - fight - food - it_use2 - item_use
 * *********************************************************************** */
void lessen_hunger(int statiated_amount, bool suppress_msg);


// last updated 19jun2000 {dlb}
/* ***********************************************************************
 * called from: acr - decks - food - religion
 * *********************************************************************** */
void set_hunger(int new_hunger_level, bool suppress_msg);


// last updated 10sept2000 {bwr}
/* ***********************************************************************
 * called from: delay.cc
 * *********************************************************************** */
void weapon_switch( int targ );

#endif
