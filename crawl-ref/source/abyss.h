/**
 * @file
 * @brief Misc abyss specific functions.
**/


#ifndef ABYSS_H
#define ABYSS_H

#include "externs.h"

// When shifting areas in the abyss, shift the square containing player LOS
// plus a little extra so that the player won't be disoriented by taking a
// step backward after an abyss shift.
const int ABYSS_AREA_SHIFT_RADIUS = LOS_RADIUS + 2;
extern const coord_def ABYSS_CENTRE;

struct abyss_state
{
    coord_def major_coord;
    uint32_t seed;
    uint32_t depth;
    double phase;
    level_id level;
    bool nuke_all;
};

extern abyss_state abyssal_state;

void abyss_morph(double duration);
void push_features_to_abyss();

void generate_abyss();
void maybe_shift_abyss_around_player();
void abyss_teleport(bool new_area);
void save_abyss_uniques();
bool is_level_incorruptible(bool quiet = false);
bool lugonu_corrupt_level(int power);
void run_corruption_effects(int duration);
void set_abyss_state(coord_def coord, uint32_t depth);
void destroy_abyss();

#endif
