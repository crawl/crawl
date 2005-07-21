/*
 *  File:       item_use.cc
 *  Summary:    Functions for making use of inventory items.
 *  Written by: Linley Henzell
 *
 *  Change History (most recent first):
 *
 *              <2>             5/26/99         JDJ             Exposed armour_prompt. takeoff_armour takes an index argument.
 *              <1>             -/--/--         LRH             Created
 */


#ifndef ITEM_USE_H
#define ITEM_USE_H


#include <string>


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: acr - item_use
 * *********************************************************************** */
bool armour_prompt(const std::string & mesg, int *index);


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: acr - item_use - items
 * *********************************************************************** */
bool takeoff_armour(int index);


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: acr
 * *********************************************************************** */
void drink(void);


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: acr
 * *********************************************************************** */
void original_name(void);


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: acr
 * *********************************************************************** */
void puton_ring(void);


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: acr
 * *********************************************************************** */
void read_scroll(void);


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: acr
 * *********************************************************************** */
void remove_ring(void);


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: acr
 * *********************************************************************** */
int get_fire_item_index(void);
void shoot_thing(void);


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: acr
 * *********************************************************************** */
void throw_anything(void);


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: acr
 * *********************************************************************** */
void wear_armour( void );

// last updated 10Sept2001 {bwr}
/* ***********************************************************************
 * called from: acr
 * *********************************************************************** */
bool do_wear_armour( int item, bool quiet );


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: acr
 * *********************************************************************** */
void wield_weapon(bool auto_wield);


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: acr
 * *********************************************************************** */
void zap_wand(void);


// last updated 15jan2001 {gdl}
/* ***********************************************************************
 * called from: item_use food
 * *********************************************************************** */
void wield_effects(int item_wield_2, bool showMsgs);

// last updated 10sept2001 {bwr}
/* ***********************************************************************
 * called from: delay.cc item_use.cc it_use2.cc
 * *********************************************************************** */
void use_randart( unsigned char item_wield_2 );

#endif
