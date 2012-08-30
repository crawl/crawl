/**
 * @file
 * @brief Moving between levels.
**/

#ifndef STAIRS_H
#define STAIRS_H

bool check_annotation_exclusion_warning();
level_id stair_destination(dungeon_feature_type feat, const string &dst,
                           bool for_real = false);
level_id stair_destination(coord_def pos, bool for_real = false);
void down_stairs(dungeon_feature_type force_stair = DNGN_UNSEEN);
void up_stairs(dungeon_feature_type force_stair = DNGN_UNSEEN);
void new_level(bool restore = false);
#endif
