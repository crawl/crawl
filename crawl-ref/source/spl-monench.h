#ifndef SPL_MONENCH_H
#define SPL_MONENCH_H

void cast_mass_sleep(int pow);
void cast_tame_beasts(int pow);
bool backlight_monsters(coord_def where, int pow, int garbage);

//returns true if it slowed the monster
bool do_slow_monster(monster* mon, kill_category whose_kill);

bool project_noise();

#endif
