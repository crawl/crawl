/**
 * @file
 * @brief Functions for making use of inventory items.
**/

#pragma once

#include <functional>
#include <string>

#include "enum.h"
#include "equipment-slot.h"
#include "item-prop-enum.h"
#include "object-selector-type.h"
#include "operation-types.h"
#include "player-equip.h"
#include "unwind.h"

const int ARMOUR_EQUIP_DELAY = 5;

operation_types use_an_item_menu(item_def *&target, operation_types oper,
                int item_type=OSEL_ANY,
                const char* prompt=nullptr,
                function<bool ()> allowcancel = [](){ return true; });
// Change the lambda to always_true<> when g++ 4.7 support is dropped.

bool oni_drunken_swing();
bool drink(item_def* potion = nullptr);

bool god_hates_brand(const int brand);

bool scroll_hostile_check(scroll_type which_scroll);
bool scroll_has_targeter(scroll_type which_scroll);
bool read(item_def* scroll = nullptr, dist *target=nullptr);

bool auto_wield();

bool use_an_item(operation_types oper, item_def *target=nullptr);

bool item_is_worn(int inv_slot);

bool enchant_weapon(item_def &wpn, bool quiet);
bool enchant_armour(item_def &arm, bool quiet);

void prompt_inscribe_item();

bool has_drunken_brawl_targets();

string item_equip_verb(const item_def& item);
string item_unequip_verb(const item_def& item);

bool handle_chain_removal(vector<item_def*>& to_remove, bool interactive);
bool try_equip_item(item_def& item);
bool try_unequip_item(item_def& item);
bool warn_about_changing_gear(const vector<item_def*>& to_remove,
                              item_def* to_equip = nullptr);

void do_equipment_change(item_def* to_equip, equipment_slot equip_slot,
                         vector<item_def*> to_remove);
