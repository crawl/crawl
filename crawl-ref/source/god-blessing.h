/**
 * @file
 * @brief Functions for gods blessing followers.
**/

#pragma once

#include "player.h"

bool bless_follower(monster* follower = nullptr,
                    god_type god = you.religion,
                    bool force = false);
