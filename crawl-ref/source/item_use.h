/**
 * @file
 * @brief Functions for making use of inventory items.
**/


#ifndef ITEM_USE_H
#define ITEM_USE_H


#include <string>
#include "externs.h"
#include "enum.h"

bool armour_prompt(const string & mesg, int *index, operation_types oper);

bool takeoff_armour(int index);

void drink(int slot = -1);

bool safe_to_remove(const item_def &item, bool quiet = false);

bool puton_ring(int slot = -1);

void read_scroll(int slot = -1);

bool remove_ring(int slot = -1, bool announce = false);

void wear_armour(int slot = -1);

bool can_wear_armour(const item_def &item, bool verbose, bool ignore_temporary);

bool do_wear_armour(int item, bool quiet);

bool can_wield(item_def *weapon, bool say_why = false,
               bool ignore_temporary_disability = false, bool unwield = false,
               bool only_known = true);

bool wield_weapon(bool auto_wield, int slot = -1,
                  bool show_weff_messages = true, bool force = false,
                  bool show_unwield_msg = true,
                  bool show_wield_msg = true);

bool item_is_worn(int inv_slot);

void zap_wand(int slot = -1);

bool enchant_weapon(item_def &wpn, int acc, int dam, const char *colour);
bool enchant_armour(int &ac_change, bool quiet, item_def &arm);

void prompt_inscribe_item();

void warn_shield_penalties();

bool stasis_blocks_effect(bool calc_unid, bool identify,
                          const char *msg, int noise = 0,
                          const char *silencedmsg = NULL);
item_def* get_only_unided_ring();

#ifdef USE_TILE
void tile_item_use_floor(int idx);
void tile_item_pickup(int idx, bool part);
void tile_item_drop(int idx, bool partdrop);
void tile_item_eat_floor(int idx);
void tile_item_use(int idx);
void tile_item_use_secondary(int idx);
#endif

#endif
