/**
 * @file
 * @brief Moving between levels.
**/

#ifndef STAIRS_H
#define STAIRS_H

bool check_annotation_exclusion_warning();
void down_stairs(dungeon_feature_type force_stair = DNGN_UNSEEN);
void up_stairs(dungeon_feature_type force_stair = DNGN_UNSEEN);
void new_level(bool restore = false);

dungeon_feature_type random_stair(bool do_place_check = true);
#endif
