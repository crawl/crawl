/**
 * @file
 * @brief Misc Enchanted Forest specific functions.
**/


#ifndef FOREST_H
#define FOREST_H

// When shifting areas in the forest, shift the square containing player LOS
// plus a little extra so that the player won't be disoriented by taking a
// step backward after an forest shift.
const int FOREST_AREA_SHIFT_RADIUS = LOS_RADIUS + 2;
extern const coord_def FOREST_CENTRE;

struct forest_state
{
    coord_def major_coord;
    double phase;
    double depth;
};
void initialize_forest_state();
void forest_morph(double duration);

void maybe_shift_forest();

#endif
