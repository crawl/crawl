#ifndef SPL_CLOUDS_H
#define SPL_CLOUDS_H

#include "spl-cast.h"

struct bolt;

spret_type conjure_flame(int pow, const coord_def& where, bool fail);
spret_type stinking_cloud(int pow, bolt &beam, bool fail);

void big_cloud(cloud_type cl_type, const actor *agent, const coord_def& where,
               int pow, int size, int spread_rate = -1, int colour = -1,
               string name = "", string tile = "");

spret_type cast_big_c(int pow, cloud_type cty, const actor *caster, bolt &beam,
                      bool fail);

spret_type cast_ring_of_flames(int power, bool fail);
void manage_fire_shield(int delay);

spret_type cast_corpse_rot(bool fail);
void corpse_rot(actor* caster);

int holy_flames(monster* caster, actor* defender);

void apply_control_winds(const monster* mon);
#endif
