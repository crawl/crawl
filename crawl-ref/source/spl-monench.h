#ifndef SPL_MONENCH_H
#define SPL_MONENCH_H

#include "spl-cast.h"

spret_type cast_mass_sleep(int pow, bool fail);
void cast_tame_beasts(int pow);
bool backlight_monsters(coord_def where, int pow, int garbage);

//returns true if it slowed the monster
bool do_slow_monster(monster* mon, const actor *agent);

spret_type project_noise(bool fail);

#endif
