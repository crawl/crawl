/*
 *  File:       .cc
 *  Summary:
 */

#include "AppHdr.h"

#include <cmath>

#include "ray.h"
#include "geom2d.h"

static geom::grid diamonds(geom::lineseq(1, 1, 0.5, 1),
                           geom::lineseq(1, -1, -0.5, 1));

static int ifloor(double d)
{
    return static_cast<int>(floor(d));
}

coord_def ray_def::pos() const
{
    // XXX: pretty arbitrary if we're just on a corner.
    int x = ifloor(r.start.x);
    int y = ifloor(r.start.y);
    return (coord_def(x, y));
}

// Return false if we passed or hit a corner.
bool ray_def::advance()
{
    if (on_corner)
    {
        on_corner = false;
        geom::movehalfcell(r, diamonds);
    }
    else
    {
        // Starting inside a diamond.
        bool c = !geom::nextcell(r, diamonds);

        if (c)
        {
            // r is now on a corner, going from diamond to diamond.
            geom::movehalfcell(r, diamonds);
            return (false);
        }
    }
    // Now inside a non-diamond.

    if (geom::nextcell(r, diamonds))
        return (true);
    else
    {
        // r is now on a corner, going from non-diamond to non-diamond.
        on_corner = true;
        return (false);
    }
}

void ray_def::advance_and_bounce()
{
    // XXX
    r.dir *= -1;
}

void ray_def::regress()
{
    r.dir *= -1;
    advance();
    r.dir *= -1;
}

