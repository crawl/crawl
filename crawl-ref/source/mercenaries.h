/**
 * @file
 * @brief Tracking permaallies granted by Yred and Beogh.
**/

#pragma once

#include <list>
#include <map>

#include "monster.h"
#include "monster-type.h"
#include "mon-transit.h"

void try_to_spawn_mercenary(int merc_type = -1);
bool is_caravan_companion(monster mon);
bool is_caravan_companion_for_type(monster_type mon_type);
bool caravan_gift_item();
bool set_spell_witch(monster* mons, item_def* staff, bool slience);
