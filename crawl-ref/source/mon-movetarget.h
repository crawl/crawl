#ifndef MON_MOVE_TARGET_H
#define MON_MOVE_TARGET_H

struct level_exit;

bool target_is_unreachable(monster* mon);
bool try_pathfind(monster* mon);
void check_wander_target(monster* mon, bool isPacified = false);
int mons_find_nearest_level_exit(const monster* mon, vector<level_exit> &e,
                                 bool reset = false);
void set_random_slime_target(monster* mon);
bool find_siren_water_target(monster* mon);
bool pacified_leave_level(monster* mon, vector<level_exit> e, int e_index);

#endif
