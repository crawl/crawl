/*
 *  File:       ray.cc
 *  Summary:    Ray definition
 */

#include "AppHdr.h"
REVISION("$Rev$");

#include "ray.h"

#include <math.h>

#include "los.h"
#include "terrain.h"

int ifloor(double d)
{
    return static_cast<int>(floor(d));
}

ray_def::ray_def(double ax, double ay, double s, int q, int idx)
    : accx(ax), accy(ay), slope(s), quadrant(q), fullray_idx(idx)
{
}

int ray_def::x() const
{
    return ifloor(accx);
}

int ray_def::y() const
{
    return ifloor(accy);
}

coord_def ray_def::pos() const
{
    return coord_def(x(), y());
}

double ray_def::reflect(double p, double c) const
{
    return (c + c - p);
}

double ray_def::reflect(bool rx, double oldx, double newx) const
{
    if (rx ? fabs(slope) > 1.0 : fabs(slope) < 1.0)
        return (reflect(oldx, floor(oldx) + 0.5));

    const double flnew = floor(newx);
    const double flold = floor(oldx);
    return (reflect(oldx,
                    flnew > flold? flnew :
                    flold > flnew? flold :
                    (newx + oldx) / 2));
}

void ray_def::set_reflect_point(const double oldx, const double oldy,
                                double *newx, double *newy,
                                bool blocked_x, bool blocked_y)
{
    if (blocked_x == blocked_y)
    {
        // What to do?
        *newx = oldx;
        *newy = oldy;
        return;
    }

    if (blocked_x)
    {
        ASSERT(int(oldy) != int(*newy));
        *newy = oldy;
        *newx = reflect(true, oldx, *newx);
    }
    else
    {
        ASSERT(int(oldx) != int(*newx));
        *newx = oldx;
        *newy = reflect(false, oldy, *newy);
    }
}

void ray_def::advance_and_bounce()
{
    // 0 = down-right, 1 = down-left, 2 = up-left, 3 = up-right
    int bouncequad[4][3] =
    {
        { 1, 3, 2 }, { 0, 2, 3 }, { 3, 1, 0 }, { 2, 0, 1 }
    };
    int oldx = x(), oldy = y();
    const double oldaccx = accx, oldaccy = accy;
    int rc = advance(false);
    int newx = x(), newy = y();
    ASSERT( grid_is_solid(grd[newx][newy]) );

    const bool blocked_x = grid_is_solid(grd[oldx][newy]);
    const bool blocked_y = grid_is_solid(grd[newx][oldy]);

    if ( double_is_zero(slope) || slope > 100.0 )
        quadrant = bouncequad[quadrant][2];
    else if ( rc != 2 )
        quadrant = bouncequad[quadrant][rc];
    else
    {
        ASSERT( (oldx != newx) && (oldy != newy) );
        if ( blocked_x && blocked_y )
            quadrant = bouncequad[quadrant][rc];
        else if ( blocked_x )
            quadrant = bouncequad[quadrant][1];
        else
            quadrant = bouncequad[quadrant][0];
    }

    set_reflect_point(oldaccx, oldaccy, &accx, &accy, blocked_x, blocked_y);
}

double ray_def::get_degrees() const
{
    if (slope > 100.0)
    {
        if (quadrant == 3 || quadrant == 2)
            return (90.0);
        else
            return (270.0);
    }
    else if (double_is_zero(slope))
    {
        if (quadrant == 0 || quadrant == 3)
            return (0.0);
        else
            return (180.0);
    }

    double deg = atan(slope) * 180.0 / M_PI;

    switch (quadrant)
    {
    case 0:
        return (360.0 - deg);

    case 1:
        return (180.0 + deg);

    case 2:
        return (180.0 - deg);

    case 3:
        return (deg);
    }
    ASSERT(!"ray has illegal quadrant");
    return (0.0);
}

void ray_def::set_degrees(double deg)
{
    while (deg < 0.0)
        deg += 360.0;
    while (deg >= 360.0)
        deg -= 360.0;

    double _slope = tan(deg / 180.0 * M_PI);

    if (double_is_zero(_slope))
    {
        slope = 0.0;

        if (deg < 90.0 || deg > 270.0)
            quadrant = 0; // right/east
        else
            quadrant = 1; // left/west
    }
    else if (_slope > 0)
    {
        slope = _slope;

        if (deg >= 180.0 && deg <= 270.0)
            quadrant = 1;
        else
            quadrant = 3;
    }
    else
    {
        slope = -_slope;

        if (deg >= 90 && deg <= 180)
            quadrant = 2;
        else
            quadrant = 0;
    }

    if (slope > 1000.0)
        slope = 1000.0;
}

void ray_def::regress()
{
    int opp_quadrant[4] = { 2, 3, 0, 1 };
    quadrant = opp_quadrant[quadrant];
    advance(false);
    quadrant = opp_quadrant[quadrant];
}

int ray_def::advance_through(const coord_def &target)
{
    return (advance(true, &target));
}

int ray_def::advance(bool shortest_possible, const coord_def *target)
{
    if (!shortest_possible)
        return (raw_advance());

    // If we want to minimise the number of moves on the ray, look one
    // step ahead and see if we can get a diagonal.

    const coord_def old = pos();
    const int ret = raw_advance();

    if (ret == 2 || (target && pos() == *target))
        return (ret);

    const double maccx = accx, maccy = accy;
    if (raw_advance() != 2)
    {
        const coord_def second = pos();
        // If we can convert to a diagonal, do so.
        if ((second - old).abs() == 2)
            return (2);
    }

    // No diagonal, so roll back.
    accx = maccx;
    accy = maccy;

    return (ret);
}

// Advance a ray in quadrant 0.
// note that slope must be nonnegative!
// returns 0 if the advance was in x, 1 if it was in y, 2 if it was
// the diagonal
int ray_def::_find_next_intercept()
{
    // handle perpendiculars
    if (double_is_zero(slope))
    {
        accx += 1.0;
        return 0;
    }
    if (slope > 100.0)
    {
        accy += 1.0;
        return 1;
    }

    const double xtarget = ifloor(accx) + 1;
    const double ytarget = ifloor(accy) + 1;
    const double xdistance = xtarget - accx;
    const double ydistance = ytarget - accy;
    double distdiff = xdistance * slope - ydistance;

    // exact corner
    if (double_is_zero(distdiff))
    {
        // move somewhat away from the corner
        if (slope > 1.0)
        {
            accx = xtarget + EPSILON_VALUE * 2;
            accy = ytarget + EPSILON_VALUE * 2 * slope;
        }
        else
        {
            accx = xtarget + EPSILON_VALUE * 2 / slope;
            accy = ytarget + EPSILON_VALUE * 2;
        }
        return 2;
    }

    // move to the boundary
    double traveldist;
    int rc = -1;
    if (distdiff > 0.0)
    {
        traveldist = ydistance / slope;
        rc = 1;
    }
    else
    {
        traveldist = xdistance;
        rc = 0;
    }

    // and a little into the next cell, taking care
    // not to go too far
    if (distdiff < 0.0)
        distdiff = -distdiff;
    traveldist += std::min(EPSILON_VALUE * 10.0, 0.5 * distdiff / slope);

    accx += traveldist;
    accy += traveldist * slope;
    return rc;
}

int ray_def::raw_advance()
{
    int rc;
    switch (quadrant)
    {
    case 0:
        // going down-right
        rc = _find_next_intercept();
        break;
    case 1:
        // going down-left
        accx = 100.0 - EPSILON_VALUE/10.0 - accx;
        rc   = _find_next_intercept();
        accx = 100.0 - EPSILON_VALUE/10.0 - accx;
        break;
    case 2:
        // going up-left
        accx = 100.0 - EPSILON_VALUE/10.0 - accx;
        accy = 100.0 - EPSILON_VALUE/10.0 - accy;
        rc   = _find_next_intercept();
        accx = 100.0 - EPSILON_VALUE/10.0 - accx;
        accy = 100.0 - EPSILON_VALUE/10.0 - accy;
        break;
    case 3:
        // going up-right
        accy = 100.0 - EPSILON_VALUE/10.0 - accy;
        rc   = _find_next_intercept();
        accy = 100.0 - EPSILON_VALUE/10.0 - accy;
        break;
    default:
        rc = -1;
    }
    return rc;
}

// Shoot a ray from the given start point (accx, accy) with the given
// slope, bounded by the given pre-squared LOS radius.
// Store the visited cells in xpos[] and ypos[], and
// return the number of cells visited.
int ray_def::footprint(int radius2, coord_def cpos[]) const
{
    ray_def copy = *this;
    coord_def c;
    int cellnum;
    for (cellnum = 0; true; ++cellnum)
    {
        copy._find_next_intercept();
        c = copy.pos();
        if (c.abs() > radius2)
            break;

        cpos[cellnum] = c;
    }
    return cellnum;
}
