/*
 *  File:       item_use.cc
 *  Summary:    Functions for making use of inventory items.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 *
 *  Change History (most recent first):
 *
 *              <2>             5/26/99         JDJ             Exposed armour_prompt. takeoff_armour takes an index argument.
 *              <1>             -/--/--         LRH             Created
 */


#ifndef ITEM_USE_H
#define ITEM_USE_H


#include <string>
#include "externs.h"
#include "enum.h"


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: acr - item_use
 * *********************************************************************** */
bool armour_prompt(const std::string & mesg, int *index,
		   operation_types oper);


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
void examine_object(void);


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: acr
 * *********************************************************************** */
bool puton_ring(int slot = -1, bool prompt_finger = true);


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: acr
 * *********************************************************************** */
void read_scroll(void);


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: acr
 * *********************************************************************** */
bool remove_ring(int slot = -1);


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

struct item_def;
// last updated 30May2003 {ds}
/* ***********************************************************************
 * called from: food
 * *********************************************************************** */
bool can_wield(const item_def *weapon, bool say_why = false);

// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: acr
 * *********************************************************************** */
bool wield_weapon(bool auto_wield, int slot = -1, bool show_we_messages = true);


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

bool puton_item(int slot, bool prompt_finger = true);

bool enchant_weapon( int which_stat, bool quiet = false );

bool throw_it(struct bolt &pbolt, int throw_2, monsters *dummy_target = NULL);

void inscribe_item();
int launcher_shield_slowdown(const item_def &launcher);

#endif
