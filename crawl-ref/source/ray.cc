/*
 *  File:       ray.cc
 *  Summary:    Diamond grid wrapper around geom::ray.
 *
 * The geom::grid diamonds is a checkerboard grid rotated
 * by 45 degrees such that the black cells ("diamonds") lie just
 * within the normal crawl map cells.
 *
 * ray_def provides for advancing and reflecting rays in
 * map coordinates, where a ray touches a given cell if it
 * meets the diamond.
 */

#include "AppHdr.h"

#include <cmath>

#include "los.h"
#include "ray.h"
#include "geom2d.h"

static geom::grid diamonds(geom::lineseq(1, 1, 0.5, 1),
                           geom::lineseq(1, -1, -0.5, 1));

static int ifloor(double d)
{
    return static_cast<int>(floor(d));
}

static bool double_is_integral(double d)
{
    return (double_is_zero(d - round(d)));
}


// Is v in a diamond?
static bool in_diamond(const geom::vector &v)
{
    int i1 = ifloor(diamonds.ls1.index(v));
    int i2 = ifloor(diamonds.ls2.index(v));
    return ((i1 + i2) % 2 == 0);
}

// Is v in the interiour of a diamond?
static bool in_diamond_int(const geom::vector &v)
{
    double d1 = diamonds.ls1.index(v);
    double d2 = diamonds.ls2.index(v);
    return (!double_is_integral(d1) && !double_is_integral(d2));
}

// Is v an intersection of grid lines?
static bool is_corner(const geom::vector &v)
{
    double d1 = diamonds.ls1.index(v);
    double d2 = diamonds.ls2.index(v);
    return (double_is_integral(d1) && double_is_integral(d2));
}

coord_def ray_def::pos() const
{
    // XXX: pretty arbitrary if we're just on a corner.
    int x = ifloor(r.start.x);
    int y = ifloor(r.start.y);
    return (coord_def(x, y));
}

// Return true if we didn't hit a corner, hence if this
// is a good ray so far.
bool ray_def::advance()
{
    ASSERT(on_corner || in_diamond_int(r.start));
    if (on_corner)
    {
        ASSERT (is_corner(r.start));
        on_corner = false;
        r.to_grid(diamonds, true);
    }
    else
    {
        // Starting inside a diamond.
        bool c = r.to_next_cell(diamonds);

        if (c)
        {
            // r is now on a corner, going from diamond to diamond.
            r.to_grid(diamonds, true);
            return (false);
        }
    }

    // Now inside a non-diamond.
    ASSERT(!in_diamond(r.start));

    if (!r.to_next_cell(diamonds))
    {
        ASSERT(in_diamond_int(r.start));
        return (true);
    }
    else
    {
        // r is now on a corner, going from non-diamond to non-diamond.
        ASSERT(is_corner(r.start));
        on_corner = true;
        return (false);
    }
}

void ray_def::bounce(const reflect_grid &rg)
{
    ASSERT(in_diamond(r.start));
    // Find out which side of the diamond r leaves through.
    geom::ray copy = r;

    r.dir = -r.dir;
}

void ray_def::regress()
{
    r.dir = -r.dir;
    advance();
    r.dir = -r.dir;
}

