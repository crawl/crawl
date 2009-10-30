#include "AppHdr.h"

#include "coord.h"

#include "directn.h"
#include "random.h"
#include "state.h"

//////////////////////////////////////////////////////////////////////////
// coord_def
int coord_def::distance_from(const coord_def &other) const
{
    return (grid_distance(*this, other));
}

int grid_distance( const coord_def& p1, const coord_def& p2 )
{
    return grid_distance(p1.x, p1.y, p2.x, p2.y);
}

// More accurate than distance() given the actual movement geometry -- bwr
int grid_distance( int x, int y, int x2, int y2 )
{
    const int dx = abs( x - x2 );
    const int dy = abs( y - y2 );

    // returns distance in terms of moves:
    return ((dx > dy) ? dx : dy);
}

int distance( const coord_def& p1, const coord_def& p2 )
{
    return distance(p1.x, p1.y, p2.x, p2.y);
}

int distance( int x, int y, int x2, int y2 )
{
    //jmf: now accurate, but remember to only compare vs. pre-squared distances
    //     thus, next to == (distance(m1.x,m1.y, m2.x,m2.y) <= 2)
    const int dx = x - x2;
    const int dy = y - y2;

    return ((dx * dx) + (dy * dy));
}

bool adjacent( const coord_def& p1, const coord_def& p2 )
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

coord_def random_in_bounds()
{
    if (crawl_state.arena)
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


