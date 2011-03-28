/**
 * @file
 * @brief Misc abyss specific functions.
**/


#ifndef ABYSS_H
#define ABYSS_H

// When shifting areas in the abyss, shift the square containing player LOS
// plus a little extra so that the player won't be disoriented by taking a
// step backward after an abyss shift.
const int ABYSS_AREA_SHIFT_RADIUS = LOS_RADIUS + 2;
const coord_def ABYSS_CENTRE(GXM / 2, GYM / 2);

void generate_abyss();
void abyss_area_shift();
void abyss_teleport(bool new_area);
void save_abyss_uniques();
bool is_level_incorruptible();
bool lugonu_corrupt_level(int power);
void run_corruption_effects(int duration);

#endif
