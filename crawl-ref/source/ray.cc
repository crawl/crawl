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

static int iround(double d)
{
    return static_cast<int>(round(d));
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

static bool _advance_from_non_diamond(geom::ray *r)
{
    if (!r->to_next_cell(diamonds))
    {
        ASSERT(in_diamond_int(r->start));
        return (false);
    }
    else
    {
        // r is now on a corner, going from non-diamond to non-diamond.
        ASSERT(is_corner(r->start));
        return (true);
    }
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
            return (true);
        }
    }

    // Now inside a non-diamond.
    ASSERT(!in_diamond(r.start));

    on_corner = _advance_from_non_diamond(&r);
    return (!on_corner);
}

static geom::ray _mirror(const geom::ray &rorig, const coord_def &side)
{
    geom::ray r = rorig;
    if (side.x == -1)
    {
        r.start.x = 1.0 - r.start.x;
        r.dir.x = -r.dir.x;
    }
    if (side.y == -1)
    {
        r.start.y = 1.0 - r.start.y;
        r.dir.y = -r.dir.y;
    }
    return (r);
}

void ray_def::bounce(const reflect_grid &rg)
{
#ifdef DEBUG_REFLECTION
    mprf(MSGCH_DIAGNOSTICS, "mirroring ray: (%f,%f) + t*(%f,%f)",
         r.start.x, r.start.y, r.dir.x, r.dir.y);
#endif
    ASSERT(in_diamond(r.start));

    // Translate to cell (0,0).
    geom::vector p(pos().x, pos().y);
    geom::ray rtrans;
    rtrans.start = r.start - p;
    rtrans.dir = r.dir;

    // Move to the diamond edge to determine the side.
    coord_def side;
    bool corner = rtrans.to_grid(diamonds, false);
    double d1 = diamonds.ls1.index(rtrans.start);
    if (double_is_integral(d1))
        side += iround(d1) ? coord_def(1,1) : coord_def(-1,-1);
    double d2 = diamonds.ls2.index(rtrans.start);
    if (double_is_integral(d2))
        side += iround(d2) ? coord_def(1,-1) : coord_def(-1,1);
    ASSERT(corner == (side.x == 0 || side.y == 0));

    // In the corner case, we have side == (+-2, 0) or (0, +-2); reduce:
    if (corner)
    {
        side.x = side.x / 2;
        side.y = side.y / 2;
    }

    // Mirror rtrans to have it leave through the positive side.
    geom::ray rmirr = _mirror(rtrans, side);

    // TODO: on a corner, reflect back unless we're on a diagonal.

    // Determine which of the three relevant cells are bouncy.
    const coord_def dx = coord_def(side.x, 0);
    const coord_def dy = coord_def(0, side.y);
    bool rx = (side.x != 0) && rg(rg_o + dx);
    bool ry = (side.y != 0) && rg(rg_o + dy);
    bool rxy = rg(rg_o + dx + dy);
    // One of the three neighbours on this side must be bouncy.
    ASSERT(rx || ry || rxy);

#ifdef DEBUG_REFLECTION
    mprf(MSGCH_DIAGNOSTICS, "bouncy cells: rx=%d, ry=%d, rxy=%d", rx, ry, rxy);
#endif

    // Now go through the cases separately.
    if (rx && ry && !rxy)
    {
        // Tricky case: diagonal corridor.
        // TODO: Implement this.
        rmirr.dir = -rmirr.dir;
    }
    else
    {
        // These all reduce to reflection at one line.
        geom::line l;
        if (rxy)
        {
            if (rx && ry)
            {
                // x + y = 1.5
                l.f = geom::form(1, 1);
                l.val = 1.5;
            }
            else if (!rx && !ry)
            {
                // x + y = 2.5
                l.f = geom::form(1, 1);
                l.val = 2.5;
            }
            else if (rx)
            {
                // x = 1
                l.f = geom::form(1, 0);
                l.val = 1;
            }
            else if (ry)
            {
                // y = 1
                l.f = geom::form(0, 1);
                l.val = 1;
            }
        }
        else if (rx)
        {
            // Flattened corners: y = x - 0.5
            // l.f = geom::form(1, -1);
            // l.val = 0.5;
            // Instead like rxy && rx: x = 1
            l.f = geom::form(1, 0);
            l.val = 1;
        }
        else // ry
        {
            // y = x + 0.5
            // l.f = geom::form(1, -1);
            // l.val = -0.5;
            l.f = geom::form(0, 1);
            l.val = 1;
        }
        double t = intersect(rmirr, l);
        ASSERT(double_is_zero(t) || t >= 0);
        rmirr.advance(t);
        rmirr.dir = reflect(rmirr.dir, l.f);
        // Depending on the case, we're on some diamond edge
        // or between diamonds. We'll just move safely into
        // the next one.
        rmirr.to_grid(diamonds, true);
        if (!in_diamond(rmirr.start))
            on_corner = _advance_from_non_diamond(&rmirr);
    }
    // Mirror back.
    rtrans = _mirror(rmirr, side);
    // Translate back.
    r.start = rtrans.start + p;
    r.dir = rtrans.dir;
#ifdef DEBUG_REFLECTION
    mprf(MSGCH_DIAGNOSTICS, "mirrored ray: (%f,%f) + t*(%f,%f)",
         r.start.x, r.start.y, r.dir.x, r.dir.y);
#endif
}

void ray_def::regress()
{
    r.dir = -r.dir;
    advance();
    r.dir = -r.dir;
}

