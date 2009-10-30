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
    // TODO: Assert we're in a central diamond.
    int x = ifloor(r.start.x);
    int y = ifloor(r.start.y);
    return (coord_def(x, y));
}

bool ray_def::advance()
{
    return (geom::nextcell(r, diamonds, true)
            && geom::nextcell(r, diamonds, false));
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

