/*
 *  File:       item_use.h
 *  Summary:    Functions for making use of inventory items.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 */


#ifndef ITEM_USE_H
#define ITEM_USE_H


#include <string>
#include "externs.h"
#include "enum.h"

struct bolt;

enum enchant_stat_type
{
    ENCHANT_TO_HIT,
    ENCHANT_TO_DAM
};

enum fire_type
{
    FIRE_NONE      = 0x0000,
    FIRE_LAUNCHER  = 0x0001,
    FIRE_DART      = 0x0002,
    FIRE_STONE     = 0x0004,
    FIRE_DAGGER    = 0x0008,
    FIRE_JAVELIN   = 0x0010,
    FIRE_SPEAR     = 0x0020,
    FIRE_HAND_AXE  = 0x0040,
    FIRE_CLUB      = 0x0080,
    FIRE_ROCK      = 0x0100,
    FIRE_NET       = 0x0200,
    FIRE_RETURNING = 0x0400,
    FIRE_INSCRIBED = 0x0800    // Only used for _get_fire_order
};

struct bolt;
struct dist;

// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: acr - item_use
 * *********************************************************************** */
bool armour_prompt(const std::string & mesg, int *index, operation_types oper);


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: acr - item_use - items
 * *********************************************************************** */
bool takeoff_armour(int index);


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: acr
 * *********************************************************************** */
void drink(int slot = -1);

bool elemental_missile_beam(int launcher_brand, int ammo_brand);

bool safe_to_remove_or_wear(const item_def &item, bool remove,
                            bool quiet = false);

// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: acr
 * *********************************************************************** */
void examine_object(void);


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: acr
 * *********************************************************************** */
bool puton_ring(int slot = -1);
void jewellery_remove_effects(item_def &item, bool mesg = true);

// called from: transfor
void jewellery_wear_effects(item_def &item);

// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: acr
 * *********************************************************************** */
void read_scroll(int slot = -1);


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: acr
 * *********************************************************************** */
bool remove_ring(int slot = -1, bool announce = false);


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: acr
 * *********************************************************************** */
bool item_is_quivered(const item_def &item);
int get_next_fire_item(int current, int offset);
int get_ammo_to_shoot(int item, dist &target, bool teleport = false);
void fire_thing(int item = -1);
void throw_item_no_quiver(void);

// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: acr
 * *********************************************************************** */
void wear_armour(int slot = -1);

bool can_wear_armour(const item_def &item, bool verbose, bool ignore_temporary);

// last updated 10Sept2001 {bwr}
/* ***********************************************************************
 * called from: acr
 * *********************************************************************** */
bool do_wear_armour(int item, bool quiet);

struct item_def;
// last updated 30May2003 {ds}
/* ***********************************************************************
 * called from: food
 * *********************************************************************** */
bool can_wield(item_def *weapon, bool say_why = false,
               bool ignore_temporary_disability = false);

// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: acr
 * *********************************************************************** */
bool wield_weapon(bool auto_wield, int slot = -1, bool show_we_messages = true,
                  bool force = false);


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: acr
 * *********************************************************************** */
void zap_wand(int slot = -1);


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
void use_randart(item_def &item, bool unmeld = false);

bool puton_item(int slot);

bool enchant_weapon(enchant_stat_type which_stat, bool quiet, item_def &wpn);
bool enchant_armour(int &ac_change, bool quiet, item_def &arm);

void setup_missile_beam(const actor *actor, bolt &beam, item_def &item,
                        std::string &ammo_name, bool &returning);

bool throw_it(bolt &pbolt, int throw_2, bool teleport = false,
              int acc_bonus = 0, dist *target = NULL);

bool thrown_object_destroyed(item_def *item, const coord_def& where,
                              bool returning);

void prompt_inscribe_item();
int launcher_shield_slowdown(const item_def &launcher,
                             const item_def *shield);
int launcher_final_speed(const item_def &launcher,
                         const item_def *shield);

void warn_shield_penalties();

int item_special_wield_effect(const item_def &item);

bool wearing_slot(int inv_slot);

#ifdef USE_TILE
void tile_item_use_floor(int idx);
void tile_item_pickup(int idx);
void tile_item_drop(int idx);
void tile_item_eat_floor(int idx);
void tile_item_use(int idx);
void tile_item_use_secondary(int idx);
#endif

#endif
