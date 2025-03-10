/**
 * @file
 * @brief Divine retribution.
**/

#pragma once

#include "god-type.h"

bool divine_retribution(god_type god, bool no_bonus = false, bool force = false);
void reduce_xp_penance(god_type god, int amount);

void gozag_incite(monster *mon);
void gozag_abandon_shops_on_level();
void lucy_check_meddling();
