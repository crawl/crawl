#pragma once

#include "spl-cast.h"

int englaciate(coord_def where, int pow, actor *agent);
spret cast_englaciation(int pow, bool fail);
bool backlight_monster(monster* mons);

//returns true if it slowed the monster
bool do_slow_monster(monster& mon, const actor *agent, int dur = 0);
