/**
 * @file
 * @brief Slow projectiles, done as monsters.
**/

#pragma once

#include "beam.h"
#include "spl-cast.h"

spret cast_iood(actor *caster, int pow, bolt *beam,
                     float vx = 0, float vy = 0, int foe = MHITNOT,
                     bool fail = false, bool needs_tracer = true,
                     monster_type orb_type = MONS_ORB_OF_DESTRUCTION);
void cast_iood_burst(int pow, coord_def target);
bool iood_act(monster& mon, bool no_trail = false);
void iood_catchup(monster* mon, int turns);
dice_def iood_damage(int pow, int dist, bool random = true);
