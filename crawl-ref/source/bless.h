/**
 * @file
 * @brief Functions for gods blessing followers.
**/

#ifndef BLESS_H
#define BLESS_H

#include "player.h"

bool bless_follower(monster* follower = NULL,
                    god_type god = you.religion,
                    bool force = false);

#endif
