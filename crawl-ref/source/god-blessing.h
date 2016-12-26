/**
 * @file
 * @brief Functions for gods blessing followers.
**/

#ifndef BLESS_H
#define BLESS_H

#include "player.h"

void gift_ammo_to_orc(monster* orc, bool initial_gift = false);

bool bless_follower(monster* follower = nullptr,
                    god_type god = you.religion,
                    bool force = false);

#endif
