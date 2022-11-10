/**
 * @file
 * @brief Functions for making use of inventory items.
**/

#pragma once

#include <functional>
#include <string>

#include "enum.h"
#include "item-prop-enum.h"
#include "object-selector-type.h"
#include "operation-types.h"

operation_types use_an_item(item_def *&target, operation_types oper,
                int item_type=OSEL_ANY,
                const char* prompt=nullptr,
                function<bool ()> allowcancel = [](){ return true; });
// Change the lambda to always_true<> when g++ 4.7 support is dropped.

bool armour_prompt(const string & mesg, int *index, operation_types oper);

bool takeoff_armour(int index = -1, bool noask = false);

void drink(item_def* potion = nullptr);

bool god_hates_brand(const int brand);

bool safe_to_remove(const item_def &item, bool quiet = false);

bool puton_ring(int slot = -1, bool allow_prompt = true,
                bool check_for_inscriptions = true, bool noask = false);

bool puton_ring(item_def &to_puton, bool allow_prompt = true,
                bool check_for_inscriptions = true, bool noask = false);

bool scroll_hostile_check(scroll_type which_scroll);
string cannot_read_item_reason(const item_def *item);
bool scroll_has_targeter(scroll_type which_scroll);
void read(item_def* scroll = nullptr, dist *target=nullptr);

bool remove_ring(int slot = -1, bool announce = false, bool noask = false);

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

void prompt_inscribe_item();
