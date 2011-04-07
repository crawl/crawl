/**
 * @file
 * @brief Slow projectiles, done as monsters.
**/

#ifndef MON_PROJECT_H
#define MON_PROJECT_H

#include "beam.h"

bool cast_iood(actor *caster, int pow, bolt *beam);
bool iood_act(monster& mon, bool no_trail = false);
void iood_catchup(monster* mon, int turns);

#endif
