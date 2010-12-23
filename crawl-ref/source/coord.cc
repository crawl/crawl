#include "AppHdr.h"

#include "coord.h"

#include "random.h"

//////////////////////////////////////////////////////////////////////////
// coord_def
int coord_def::distance_from(const coord_def &other) const
{
    return (grid_distance(*this, other));
}

int grid_distance(const coord_def& p1, const coord_def& p2)
{
    return ((p2 - p1).rdist());
}

int distance(const coord_def& p1, const coord_def& p2)
{
    return ((p2 - p1).abs());
}

bool adjacent(const coord_def& p1, const coord_def& p2)
{
    return grid_distance(p1, p2) <= 1;
}

bool in_bounds_x(int x)
{
    return (x > X_BOUND_1 && x < X_BOUND_2);
}

bool in_bounds_y(int y)
{
    return (y > Y_BOUND_1 && y < Y_BOUND_2);
}

// Returns true if inside the area the player can move and dig (ie exclusive).
bool in_bounds(int x, int y)
{
    return (in_bounds_x(x) && in_bounds_y(y));
}

bool map_bounds_x(int x)
{
    return (x >= X_BOUND_1 && x <= X_BOUND_2);
}

bool map_bounds_y(int y)
{
    return (y >= Y_BOUND_1 && y <= Y_BOUND_2);
}

// Returns true if inside the area the player can map (ie inclusive).
// Note that terrain features should be in_bounds() leaving an outer
// ring of rock to frame the level.
bool map_bounds(int x, int y)
{
    return (map_bounds_x(x) && map_bounds_y(y));
}

bool map_bounds_with_margin(coord_def p, int margin)
{
    return (p.x >= X_BOUND_1 + margin && p.x <= X_BOUND_2 - margin
            && p.y >= Y_BOUND_1 + margin && p.y <= Y_BOUND_2 - margin);
}

coord_def random_in_bounds()
{
    return coord_def(random_range(MAPGEN_BORDER, GXM - MAPGEN_BORDER - 1),
                     random_range(MAPGEN_BORDER, GYM - MAPGEN_BORDER - 1));
}
