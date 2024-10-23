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

const int ARMOUR_EQUIP_DELAY = 5;

operation_types use_an_item_menu(item_def *&target, operation_types oper,
                int item_type=OSEL_ANY,
                const char* prompt=nullptr,
                function<bool ()> allowcancel = [](){ return true; });
// Change the lambda to always_true<> when g++ 4.7 support is dropped.

bool armour_prompt(const string & mesg, int *index, operation_types oper);

bool takeoff_armour(int index, bool noask = false);

bool oni_drunken_swing();
bool drink(item_def* potion = nullptr);

bool god_hates_brand(const int brand);

bool safe_to_remove(const item_def &item, bool quiet = false);

bool puton_ring(item_def &to_puton, bool allow_prompt = true,
                bool check_for_inscriptions = true, bool noask = false);

bool scroll_hostile_check(scroll_type which_scroll);
bool scroll_has_targeter(scroll_type which_scroll);
bool read(item_def* scroll = nullptr, dist *target=nullptr);

bool remove_ring(int slot, bool announce = false, bool noask = false);

bool wear_armour(int slot);

bool can_wear_armour(const item_def &item, bool verbose, bool ignore_temporary);

bool can_wield(const item_def *weapon, bool say_why = false,
               bool ignore_temporary_disability = false, bool unwield = false);

bool auto_wield();
bool wield_weapon(int slot);
bool unwield_weapon(const item_def &wpn);

bool use_an_item(operation_types oper, item_def *target=nullptr);

bool item_is_worn(int inv_slot);

bool enchant_weapon(item_def &wpn, bool quiet);
bool enchant_armour(item_def &arm, bool quiet);

void prompt_inscribe_item();

bool has_drunken_brawl_targets();

string item_equip_verb(const item_def& item);
string item_unequip_verb(const item_def& item);
