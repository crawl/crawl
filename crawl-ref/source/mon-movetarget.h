#ifndef MON_MOVE_TARGET_H
#define MON_MOVE_TARGET_H

struct level_exit;

bool try_pathfind(monsters *mon, const dungeon_feature_type can_move,
                          bool potentially_blocking);
bool find_wall_target(monsters *mon);
void check_wander_target(monsters *mon, bool isPacified = false,
                         dungeon_feature_type can_move = DNGN_UNSEEN);
int mons_find_nearest_level_exit(const monsters *mon,
                                 std::vector<level_exit> &e,
                                 bool reset = false);
void set_random_slime_target(monsters* mon);
bool find_siren_water_target(monsters *mon);
bool pacified_leave_level(monsters *mon, std::vector<level_exit> e,
                          int e_index);

#endif
