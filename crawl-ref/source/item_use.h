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

enum enchant_stat_type
{
    ENCHANT_TO_HIT,
    ENCHANT_TO_DAM
};

enum fire_type
{
    FIRE_NONE     = 0x0000,
    FIRE_LAUNCHER = 0x0001,
    FIRE_DART     = 0x0002,
    FIRE_STONE    = 0x0004,
    FIRE_DAGGER   = 0x0008,
    FIRE_JAVELIN  = 0x0010,
    FIRE_SPEAR    = 0x0020,
    FIRE_HAND_AXE = 0x0040,
    FIRE_CLUB     = 0x0080,
    FIRE_ROCK     = 0x0100,
    FIRE_NET      = 0x0200
};

struct bolt;
struct dist;

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

bool elemental_missile_beam(int launcher_brand, int ammo_brand);

bool safe_to_remove_or_wear(const item_def &item, bool remove);

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
bool remove_ring(int slot = -1, bool announce = false);


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: acr
 * *********************************************************************** */
int get_fire_item_index(int start_from = 0, bool forward = true,
                        bool check_quiver = true);
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

bool can_wear_armour(const item_def &item, bool verbose, bool ignore_temporary);

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
bool can_wield(const item_def *weapon, bool say_why = false,
               bool ignore_temporary_disability = false);

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
void use_randart(item_def &item);

bool puton_item(int slot, bool prompt_finger = true);

bool enchant_weapon( enchant_stat_type which_stat, bool quiet = false );

bool throw_it(bolt &pbolt, int throw_2, bool teleport=false, int acc_bonus=0,
              dist *target = NULL);

void inscribe_item();
int launcher_shield_slowdown(const item_def &launcher, 
                             const item_def *shield);
int launcher_final_speed(const item_def &launcher, 
                         const item_def *shield);

void warn_shield_penalties();

int item_special_wield_effect(const item_def &item);

bool wearing_slot(int inv_slot);

#endif
