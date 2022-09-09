#pragma once

#include "cloud-type.h"
#include "spl-cast.h"

struct bolt;
class dist;
class actor;

spret cast_dreadful_rot(int pow, bool fail);

spret kindle_blastsparks(int pow, bool fail);
void explode_blastsparks_at(coord_def p);

void big_cloud(cloud_type cl_type, const actor *agent, const coord_def& where,
               int pow, int size, int spread_rate = -1);

spret cast_big_c(int pow, spell_type spl, const actor *caster, bolt &beam,
                      bool fail);

spret cast_ring_of_flames(int power, bool fail);
void manage_fire_shield();

void holy_flames(monster* caster, actor* defender);

spret cast_cloud_cone(const actor *caster, int pow, const coord_def &pos,
                           bool fail = false);

spret scroll_of_poison(bool unknown);
