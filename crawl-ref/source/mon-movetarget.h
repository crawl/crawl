#ifndef MON_MOVE_TARGET_H
#define MON_MOVE_TARGET_H

struct level_exit;

bool try_pathfind(monster* mon, const dungeon_feature_type can_move);
bool find_wall_target(monster* mon);
void check_wander_target(monster* mon, bool isPacified = false,
                         dungeon_feature_type can_move = DNGN_UNSEEN);
int mons_find_nearest_level_exit(const monster* mon,
                                 std::vector<level_exit> &e,
                                 bool reset = false);
void set_random_slime_target(monster* mon);
bool find_siren_water_target(monster* mon);
bool pacified_leave_level(monster* mon, std::vector<level_exit> e,
                          int e_index);

#endif
