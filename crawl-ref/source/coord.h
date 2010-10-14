#ifndef COORD_H
#define COORD_H

bool in_bounds_x(int x);
bool in_bounds_y(int y);
bool in_bounds(int x, int y);
bool map_bounds_x(int x);
bool map_bounds_y(int y);
bool map_bounds(int x, int y);
coord_def random_in_bounds();

inline bool in_bounds(const coord_def &p)
{
    return in_bounds(p.x, p.y);
}

inline bool map_bounds(const coord_def &p)
{
    return map_bounds(p.x, p.y);
}

// Checks that a given point is within the map, excluding 'margin' squares at
// the edge of the map.
bool map_bounds_with_margin(coord_def p, int margin);

// Determines if the coordinate is within bounds of an LOS array.
inline bool show_bounds(const coord_def &p)
{
    return (p.x >= 0 && p.x < ENV_SHOW_DIAMETER
            && p.y >= 0 && p.y < ENV_SHOW_DIAMETER);
}

int grid_distance(const coord_def& p1, const coord_def& p2);
int distance(const coord_def& p1, const coord_def& p2);
bool adjacent(const coord_def& p1, const coord_def& p2);

// Conversion between different coordinate systems.
// XXX: collect all of these here?

coord_def player2grid(const coord_def& pc);
coord_def grid2player(const coord_def& pc);

#endif
