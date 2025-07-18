/**
 * @file
 * @brief Functions for letting the player adjust letter assignments.
 **/

#pragma once

#include "operation-types.h"

struct item_def;

void adjust();
void adjust_item(operation_types oper = OPER_ANY, item_def* to_adjust = nullptr);
void swap_inv_slots(item_def& to_adjust, int new_letter, bool verbose);
