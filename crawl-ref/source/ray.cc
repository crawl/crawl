/*
 *  File:       ray.cc
 *  Summary:    Ray definition
 */

#include "AppHdr.h"

#include "ray.h"

#include <math.h>

#include "los.h"
#include "terrain.h"

static int ifloor(double d)
{
    return static_cast<int>(floor(d));
}

ray_def::ray_def(double ax, double ay, double s, int qx, int qy, int idx)
    : accx(ax), accy(ay), slope(s), quadx(qx), quady(qy), cycle_idx(idx)
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
    int oldx = x(), oldy = y();
    const double oldaccx = accx, oldaccy = accy;
    adv_type rc = advance();
    int newx = x(), newy = y();
    ASSERT(feat_is_solid(grd[newx][newy]));

    const bool blocked_x = feat_is_solid(grd[oldx][newy]);
    const bool blocked_y = feat_is_solid(grd[newx][oldy]);

    if (double_is_zero(slope) || slope > 100.0)
    {
        quadx = -quadx;
        quady = -quady;
    }
    else if (rc == ADV_X)
        quadx = -quadx;
    else if (rc == ADV_Y)
        quady = -quady;
    else // rc == ADV_XY
    {
        ASSERT((oldx != newx) && (oldy != newy));
        if (blocked_x && blocked_y)
        {
            quadx = -quadx;
            quady = -quady;
        }
        else if (blocked_x)
            quady = -quady;
        else
            quadx = -quadx;
    }

    set_reflect_point(oldaccx, oldaccy, blocked_x, blocked_y);
}

double ray_def::get_degrees() const
{
    if (slope > 100.0)
        return (quadx < 0 ? 90.0 : 270.0);
    else if (double_is_zero(slope))
        return (quady > 0 ? 0.0  : 180.0);

    // 0 < deg < 90
    double deg = atan(slope) * 180.0 / M_PI;
    if (quadx < 0)
        deg = 180.0 - deg;
    if (quady < 0)
        deg = 360.0 - deg;
    return (deg);
}

void ray_def::set_degrees(double deg)
{
    while (deg < 0.0)
        deg += 360.0;
    while (deg >= 360.0)
        deg -= 360.0;

    if (deg > 180.0)
    {
        quady = -1;
        deg = 360 - deg;
    }
    if (deg > 90.0)
    {
        quadx = -1;
        deg = 180 - deg;
    }

    slope = tan(deg / 180.0 * M_PI);
    if (double_is_zero(slope))
        slope = 0.0;
    if (slope > 1000.0)
        slope = 1000.0;
}

void ray_def::regress()
{
    quadx = -quadx; quady= -quady;
    advance();
    quadx = -quadx; quady= -quady;
}

// Advance a ray in the positive quadrant.
// note that slope must be nonnegative!
// returns 0 if the advance was in x, 1 if it was in y, 2 if it was
// the diagonal
adv_type ray_def::raw_advance_pos()
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
 * Mirror ray into positive quadrant or back.
 * this.quad{x,y} are not touched (used for the flip back).
 */
void ray_def::flip()
{
    accx = 0.5 + quadx * (accx - 0.5);
    accy = 0.5 + quady * (accy - 0.5);
}

adv_type ray_def::advance()
{
    adv_type rc;
    flip();
    rc = raw_advance_pos();
    flip();
    return rc;
}
