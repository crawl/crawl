#ifndef SPL_MONENCH_H
#define SPL_MONENCH_H

#include "spl-cast.h"

int englaciate(coord_def where, int pow, actor *agent);
spret_type cast_englaciation(int pow, bool fail);
bool backlight_monster(monster* mons);

//returns true if it slowed the monster
bool do_slow_monster(monster& mon, const actor *agent, int dur = 0);

#endif
