/**
 * @file
 * @brief Functions for corpse butchering.
 **/

#pragma once

bool turn_corpse_into_skeleton(item_def &item);
void turn_corpse_into_chunks(item_def &item, bool bloodspatter = true);
void butcher_corpse(item_def &item, bool skeleton = true,
                    bool chunks = true);
