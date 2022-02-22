#pragma once

#include "spl-cast.h"

struct bolt;
class actor;
class monster;

#define VILE_CLUTCH_POWER_KEY "vile_clutch_power"

int englaciate(coord_def where, int pow, actor *agent);
spret cast_englaciation(int pow, bool fail);
bool backlight_monster(monster* mons);

//returns true if it slowed the monster
bool do_slow_monster(monster& mon, const actor *agent, int dur = 0);
bool enfeeble_monster(monster &mon, int pow);
spret cast_simulacrum(coord_def target, int pow, bool fail);
spret cast_vile_clutch(int pow, bolt &beam, bool fail);
