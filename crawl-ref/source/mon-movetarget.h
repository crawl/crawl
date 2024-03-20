#pragma once

#include "coord-def.h"
#include <vector>

using std::vector;

#define PACIFY_LEAVE_FAIL_KEY "pacify_leave_fail"

class monster;

bool target_is_unreachable(monster* mon);
bool try_pathfind(monster* mon);
void check_wander_target(monster* mon, bool isPacified = false);
void set_random_target(monster* mon);
void set_random_slime_target(monster* mon);
bool find_merfolk_avatar_water_target(monster* mon);
bool mons_path_to_nearest_level_exit(monster* mon);
bool pacified_leave_level(monster* mon);

bool can_go_straight(const monster* mon, const coord_def& p1,
                     const coord_def& p2);
