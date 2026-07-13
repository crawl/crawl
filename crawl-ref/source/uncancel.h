/**
 * @file
 * @brief User interactions that need to be restarted if the game is forcibly
 *        saved (via SIGHUP/window close).
**/

#pragma once

#include "uncancellable-type.h"

struct uncancellable
{
    uncancellable_type kind;
    int piety_cost_or_in_inventory;
    int mp_cost_or_item_index;
    int hp_cost;
};

bool has_uncancel();
void resume_uncancel();
bool run_uncancel(uncancellable uc);
