#include "AppHdr.h"

#include "coord.h"

#include "random.h"
#include "state.h"
#include "viewgeom.h"

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

bool map_bounds_with_margin(coord_def p, int margin)
{
    return (p.x >= X_BOUND_1 + margin && p.x <= X_BOUND_2 - margin
            && p.y >= Y_BOUND_1 + margin && p.y <= Y_BOUND_2 - margin);
}

coord_def random_in_bounds()
{
    if (crawl_state.game_is_arena())
    {
        const coord_def &ul = crawl_view.glos1; // Upper left
        const coord_def &lr = crawl_view.glos2; // Lower right

        return coord_def(random_range(ul.x, lr.x - 1),
                         random_range(ul.y, lr.y - 1));
    }
    else
        return coord_def(random_range(MAPGEN_BORDER, GXM - MAPGEN_BORDER - 1),
                         random_range(MAPGEN_BORDER, GYM - MAPGEN_BORDER - 1));
}

// Coordinate system conversions.

coord_def player2grid(const coord_def &pc)
{
    return (pc + you.pos());
}

coord_def grid2player(const coord_def &gc)
{
    return (gc - you.pos());
}
