/**
 * @file
 * @brief Functions for making use of inventory items.
**/

#ifndef ITEM_USE_H
#define ITEM_USE_H

#include <functional>
#include <string>

#include "enum.h"
#include "operation-types.h"

item_def* use_an_item(int item_type, operation_types oper, const char* prompt,
                      function<bool ()> allowcancel = [](){ return true; });
// Change the lambda to always_true<> when g++ 4.7 support is dropped.

bool armour_prompt(const string & mesg, int *index, operation_types oper);

bool takeoff_armour(int index);

void drink(item_def* potion = nullptr);

bool god_hates_brand(const int brand);

bool safe_to_remove(const item_def &item, bool quiet = false);

bool puton_ring(int slot = -1, bool allow_prompt = true);

void read(item_def* scroll = nullptr);
void read_scroll(item_def& scroll);
bool player_can_read();
string cannot_read_item_reason(const item_def &item);

bool remove_ring(int slot = -1, bool announce = false);

bool wear_armour(int slot = -1);

bool can_wear_armour(const item_def &item, bool verbose, bool ignore_temporary);

bool can_wield(const item_def *weapon, bool say_why = false,
               bool ignore_temporary_disability = false, bool unwield = false,
               bool only_known = true);

bool wield_weapon(bool auto_wield, int slot = -1,
                  bool show_weff_messages = true,
                  bool show_unwield_msg = true,
                  bool show_wield_msg = true,
                  bool adjust_time_taken = true);

bool item_is_worn(int inv_slot);

bool enchant_weapon(item_def &wpn, bool quiet);
bool enchant_armour(int &ac_change, bool quiet, item_def &arm);
void random_uselessness();

void prompt_inscribe_item();

#ifdef USE_TILE
void tile_item_use_floor(int idx);
void tile_item_pickup(int idx, bool part);
void tile_item_drop(int idx, bool partdrop);
void tile_item_eat_floor(int idx);
void tile_item_use(int idx);
void tile_item_use_secondary(int idx);
#endif

#endif
