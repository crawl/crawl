#ifndef SPL_MONENCH_H
#define SPL_MONENCH_H

#include "spl-cast.h"

int englaciate(coord_def where, int pow, int, actor *agent);
spret_type cast_englaciation(int pow, bool fail);
void cast_tame_beasts(int pow);
bool backlight_monsters(coord_def where, int pow, int garbage);

//returns true if it slowed the monster
bool do_slow_monster(monster* mon, const actor *agent, int dur = 0);

#endif
