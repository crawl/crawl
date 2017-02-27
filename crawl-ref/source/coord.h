#pragma once

coord_def random_in_bounds();

static inline bool in_bounds_x(int x)
{
    return x > X_BOUND_1 && x < X_BOUND_2;
}

static inline bool in_bounds_y(int y)
{
    return y > Y_BOUND_1 && y < Y_BOUND_2;
}

// Returns true if inside the area the player can move and dig (ie exclusive).
static inline bool in_bounds(int x, int y)
{
    return x > X_BOUND_1 && x < X_BOUND_2 && y > Y_BOUND_1 && y < Y_BOUND_2;
}

// Returns true if inside the area the player can map (ie inclusive).
// Note that terrain features should be in_bounds() leaving an outer
// ring of rock to frame the level.
static inline bool map_bounds(int x, int y)
{
    return x >= X_BOUND_1 && x <= X_BOUND_2 && y >= Y_BOUND_1 && y <= Y_BOUND_2;
}

static inline bool in_bounds(const coord_def &p)
{
    return in_bounds(p.x, p.y);
}

static inline bool map_bounds(const coord_def &p)
{
    return map_bounds(p.x, p.y);
}

// Convert an invalid coordinate to a map_bounds coordinate by truncation.
coord_def clip(const coord_def &p);

// Checks that a given point is within the map, excluding 'margin' squares at
// the edge of the map.
bool map_bounds_with_margin(coord_def p, int margin) PURE;

// Determines if the coordinate is within bounds of an LOS array.
static inline bool show_bounds(const coord_def &p)
{
    return p.x >= 0 && p.x < ENV_SHOW_DIAMETER
           && p.y >= 0 && p.y < ENV_SHOW_DIAMETER;
}

int grid_distance(const coord_def& p1, const coord_def& p2) PURE;
int distance2(const coord_def& p1, const coord_def& p2) PURE;
bool adjacent(const coord_def& p1, const coord_def& p2) PURE;

// Conversion between different coordinate systems.
// XXX: collect all of these here?

coord_def player2grid(const coord_def& pc) PURE;
coord_def grid2player(const coord_def& pc) PURE;
coord_def rotate_adjacent(coord_def vector, int direction) PURE;

coord_def clamp_in_bounds(const coord_def &p) PURE;

#ifdef ASSERTS
#  define ASSERT_IN_BOUNDS(where)                                           \
     ASSERTM(in_bounds(where), "%s = (%d,%d)", #where, (where).x, (where).y)
#  define ASSERT_IN_BOUNDS_OR_ORIGIN(where)               \
     ASSERTM(in_bounds(where) || (where).origin(),        \
            "%s = (%d,%d)", #where, (where).x, (where).y)
#else
#  define ASSERT_IN_BOUNDS(where)           ((void) 0)
#  define ASSERT_IN_BOUNDS_OR_ORIGIN(where) ((void) 0)
#endif

