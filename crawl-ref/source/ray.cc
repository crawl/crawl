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

static int ifloor(double d)
{
    return static_cast<int>(floor(d));
}

ray_def::ray_def(double ax, double ay, double s, quad_type q, int idx)
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

static double _reflect(double p, double c)
{
    return (c + c - p);
}

double ray_def::reflect(bool rx, double oldx, double newx) const
{
    if (rx ? fabs(slope) > 1.0 : fabs(slope) < 1.0)
        return (_reflect(oldx, floor(oldx) + 0.5));

    const double flnew = floor(newx);
    const double flold = floor(oldx);
    return (_reflect(oldx,
                     flnew > flold ? flnew :
                     flold > flnew ? flold :
                     (newx + oldx) / 2));
}

void ray_def::set_reflect_point(const double oldx, const double oldy,
                                bool blocked_x, bool blocked_y)
{
    if (blocked_x == blocked_y)
    {
        // What to do?
        accx = oldx;
        accy = oldy;
        return;
    }

    if (blocked_x)
    {
        ASSERT(int(oldy) != int(accy));
        accy = oldy;
        accx = reflect(true, oldx, accx);
    }
    else
    {
        ASSERT(int(oldx) != int(accx));
        accx = oldx;
        accy = reflect(false, oldy, accy);
    }
}

void ray_def::advance_and_bounce()
{
    quad_type bouncequad[4][3] =
    {
    //    ADV_X    ADV_Y    ADV_XY
        { QUAD_SW, QUAD_NE, QUAD_NW }, // QUAD_SE
        { QUAD_SE, QUAD_NW, QUAD_NE }, // QUAD_SW
        { QUAD_NE, QUAD_SW, QUAD_SE }, // QUAD_NW
        { QUAD_NW, QUAD_SE, QUAD_SW }  // QUAD_NE
    };
    int oldx = x(), oldy = y();
    const double oldaccx = accx, oldaccy = accy;
    adv_type rc = advance(false);
    int newx = x(), newy = y();
    ASSERT(grid_is_solid(grd[newx][newy]));

    const bool blocked_x = grid_is_solid(grd[oldx][newy]);
    const bool blocked_y = grid_is_solid(grd[newx][oldy]);

    if (double_is_zero(slope) || slope > 100.0)
        quadrant = bouncequad[quadrant][ADV_XY];
    else if (rc != ADV_XY)
        quadrant = bouncequad[quadrant][rc];
    else
    {
        ASSERT((oldx != newx) && (oldy != newy));
        if (blocked_x && blocked_y)
            quadrant = bouncequad[quadrant][rc];
        else if (blocked_x)
            quadrant = bouncequad[quadrant][ADV_Y];
        else
            quadrant = bouncequad[quadrant][ADV_X];
    }

    set_reflect_point(oldaccx, oldaccy, blocked_x, blocked_y);
}

double ray_def::get_degrees() const
{
    if (slope > 100.0)
    {
        if (quadrant == QUAD_NW || quadrant == QUAD_NE)
            return (90.0);
        else
            return (270.0);
    }
    else if (double_is_zero(slope))
    {
        if (quadrant == QUAD_SE || quadrant == QUAD_NE)
            return (0.0);
        else
            return (180.0);
    }

    double deg = atan(slope) * 180.0 / M_PI; // 0 < deg < 90

    switch (quadrant)
    {
    case QUAD_SE:
        return (360.0 - deg);
    case QUAD_SW:
        return (180.0 + deg);
    case QUAD_NW:
        return (180.0 - deg);
    case QUAD_NE:
    default:
        return (deg);
    }
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
            quadrant = QUAD_SE;
        else
            quadrant = QUAD_SW;
    }
    else if (_slope > 0)
    {
        slope = _slope;

        if (deg >= 180.0 && deg <= 270.0)
            quadrant = QUAD_SW;
        else
            quadrant = QUAD_NE;
    }
    else
    {
        slope = -_slope;

        if (deg >= 90 && deg <= 180)
            quadrant = QUAD_NW;
        else
            quadrant = QUAD_SE;
    }

    if (slope > 1000.0)
        slope = 1000.0;
}

void ray_def::regress()
{
    quad_type opp_quadrant[4] = { QUAD_NW, QUAD_NE, QUAD_SE, QUAD_SW };
    quadrant = opp_quadrant[quadrant];
    advance(false);
    quadrant = opp_quadrant[quadrant];
}

adv_type ray_def::advance_through(const coord_def &target)
{
    return (advance(true, &target));
}

adv_type ray_def::advance(bool shortest_possible, const coord_def *target)
{
    if (!shortest_possible)
        return (raw_advance());

    // If we want to minimise the number of moves on the ray, look one
    // step ahead and see if we can get a diagonal.

    const coord_def old = pos();
    const adv_type ret = raw_advance();

    if (ret == ADV_XY || (target && pos() == *target))
        return (ret);

    const double maccx = accx, maccy = accy;
    if (raw_advance() != ADV_XY)
    {
        const coord_def second = pos();
        // If we can convert to a diagonal, do so.
        if ((second - old).abs() == 2)
            return (ADV_XY);
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
adv_type ray_def::raw_advance_0()
{
    // handle perpendiculars
    if (double_is_zero(slope))
    {
        accx += 1.0;
        return ADV_X;
    }
    if (slope > 100.0)
    {
        accy += 1.0;
        return ADV_Y;
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
        return ADV_XY;
    }

    // move to the boundary
    double traveldist;
    adv_type rc;
    if (distdiff > 0.0)
    {
        traveldist = ydistance / slope;
        rc = ADV_Y;
    }
    else
    {
        traveldist = xdistance;
        rc = ADV_X;
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

/*
 * Mirror ray into quadrant 0 or back.
 * this.quadrant itself is not touched (used for the flip back).
 */
void ray_def::flip()
{
    int signx[] = {1, -1, -1,  1};
    int signy[] = {1,  1, -1, -1};
    accx = 0.5 + signx[quadrant] * (accx - 0.5);
    accy = 0.5 + signy[quadrant] * (accy - 0.5);
}

adv_type ray_def::raw_advance()
{
    adv_type rc;
    flip();
    rc = raw_advance_0();
    flip();
    return rc;
}

// Shoot a ray from the given start point (accx, accy) with the given
// slope, bounded by the given pre-squared LOS radius.
// Store the visited cells in pos[], and
// return the number of cells visited.
int ray_def::footprint(int radius2, coord_def cpos[]) const
{
    ray_def copy = *this;
    coord_def c;
    int cellnum;
    for (cellnum = 0; true; ++cellnum)
    {
        copy.raw_advance();
        c = copy.pos();
        if (c.abs() > radius2)
            break;

        cpos[cellnum] = c;
    }
    return cellnum;
}
