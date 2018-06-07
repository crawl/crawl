#pragma once

#include <vector>

#include "env.h"
#include "fixedarray.h"

// Sets height to the given height, iff the env.heightmap is already initialized
void dgn_height_set_at(const coord_def &c, int height = 0);

// The caller is responsible for ensuring that env.heightmap is set.
static inline short &dgn_height_at(const coord_def &c)
{
    return (*env.heightmap)(c);
}

typedef FixedArray<bool, GXM, GYM> grid_bool;
typedef FixedArray<short, GXM, GYM> grid_short;
typedef pair<int, int> int_range;

const int DGN_UNDEFINED_HEIGHT = -10000;

int resolve_range(int_range range, int nrolls = 1);

void dgn_initialise_heightmap(int initial_height = 0);
void dgn_smooth_height_at(coord_def c, int radius = 1,
                          int max_height = DGN_UNDEFINED_HEIGHT);
void dgn_smooth_heights(int radius = 1, int npasses = 1);

void dgn_island_centred_at(const coord_def &c,
                           // Number of times to raise heights of
                           // points near c.
                           int n_points,

                           // Radius within which all height
                           // increments are applied.
                           int radius,

                           // Lower and upper limits to the height
                           // delta per perturbation.
                           int_range height_delta_range,

                           int border_margin = 6,

                           // If make_atoll is set, all points chosen for
                           // height increment will be close to the specified
                           // radius from c, thus producing a ragged ring.
                           bool make_atoll = false);

struct dgn_island_plan
{
public:
    // Number of squares of border to leave around the level.
    int level_border_depth;

    // Number of auxiliary high points for each island.
    int_range n_aux_centres;

    // Distance from the island centre where the aux high points are placed.
    int_range aux_centre_offset_range;

    // Percentage of island (aux) centres that are built as atolls.
    int atoll_roll;

    // The positions of the primary centre of each island.
    vector<coord_def> islands;

    // The square of the minimum distance that must separate any two
    // island centres. This is not intended to prevent island overlap, only
    // to prevent too much clumping of islands.
    int island_separation_dist2;

    // Number of points near each island centre that will be raised by
    // the island builder.
    int_range n_island_centre_delta_points;

    // Range of radii for island primary centres.
    int_range island_centre_radius_range;

    int_range island_centre_point_height_increment;

    // Number of points near secondary island centres that will be
    // raised by the island builder.
    int_range n_island_aux_delta_points;

    // Range of radii for island aux centres.
    int_range island_aux_radius_range;

    int_range island_aux_point_height_increment;

public:
    void build(int nislands);
    coord_def pick_and_remove_random_island();

private:
    coord_def pick_island_spot();
    void build_island();
};
