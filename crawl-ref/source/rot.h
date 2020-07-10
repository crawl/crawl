/**
 * @file
 * @brief Functions for blood & chunk rot.
 **/

#pragma once

// aut / rot_time_factor = units on corpse "special" property
#define ROT_TIME_FACTOR 20

// # of special units until fresh corpses rot away
#define FRESHEST_CORPSE 110

void rot_corpses(int elapsedTime);
