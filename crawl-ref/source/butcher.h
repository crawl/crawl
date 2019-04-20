/**
 * @file
 * @brief Functions for corpse butchering & bottling.
 **/

#pragma once

#include "maybe-bool.h"

void butchery(item_def* specific_corpse = nullptr);
void finish_butchering(item_def& corpse);

void maybe_drop_monster_hide(const item_def &corpse);
bool turn_corpse_into_skeleton(item_def &item);
void turn_corpse_into_chunks(item_def &item, bool bloodspatter = true);
void butcher_corpse(item_def &item, bool skeleton = true,
                    bool chunks = true);
