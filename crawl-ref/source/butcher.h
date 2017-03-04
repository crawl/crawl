/**
 * @file
 * @brief Functions for corpse butchering & bottling.
 **/

#pragma once

#include "maybe-bool.h"

void butchery(item_def* specific_corpse = nullptr);
void finish_butchering(item_def& corpse, bool bottling);

void maybe_drop_monster_hide(const item_def &corpse);
bool turn_corpse_into_skeleton(item_def &item);
void turn_corpse_into_chunks(item_def &item, bool bloodspatter = true);
void butcher_corpse(item_def &item, maybe_bool skeleton = MB_MAYBE,
                    bool chunks = true);
bool can_bottle_blood_from_corpse(monster_type mons_class);
int num_blood_potions_from_corpse(monster_type mons_class);
void turn_corpse_into_blood_potions(item_def &item);
void turn_corpse_into_skeleton_and_blood_potions(item_def &item);
