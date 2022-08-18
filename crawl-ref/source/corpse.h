/**
 * @file
 * @brief Functions for corpse handling.
 **/

#pragma once

// aut / rot_time_factor = units on corpse "special" property
#define ROT_TIME_FACTOR 20

// # of special units until fresh corpses rot away
#define FRESHEST_CORPSE 110

void rot_corpses(int elapsedTime);

bool turn_corpse_into_skeleton(item_def &item);
