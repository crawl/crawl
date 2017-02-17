/**
 * @file
 * @brief Divine retribution.
**/

#ifndef GODWRATH_H
#define GODWRATH_H

#include "god-type.h"

bool divine_retribution(god_type god, bool no_bonus = false, bool force = false);
void reduce_xp_penance(god_type god, int amount);

void gozag_incite(monster *mon);

bool drain_wands();
#endif
