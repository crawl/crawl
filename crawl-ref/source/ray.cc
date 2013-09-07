/**
 * @file
 * @brief Diamond grid wrapper around geom::ray.
 *
 * The geom::grid diamonds is a checkerboard grid rotated
 * by 45 degrees such that the black cells ("diamonds") lie just
 * within the normal crawl map cells.
 *
 * ray_def provides for advancing and reflecting rays in
 * map coordinates, where a ray touches a given cell if it
 * meets the diamond.
**/

#include "AppHdr.h"

#include <cmath>

#include "los.h"
#include "ray.h"
#include "geom2d.h"

static geom::grid diamonds(geom::lineseq(1, 1, 0.5, 1),
                           geom::lineseq(1, -1, -0.5, 1));

static int _ifloor(double d)
{
    return static_cast<int>(floor(d));
}

static int iround(double d)
{
    return static_cast<int>(round(d));
}

static int ifloor(double d)
{
    int r = iround(d);
    if (double_is_zero(d - r))
        return r;
    else
        return _ifloor(d);
}

static bool double_is_integral(double d)
{
    return double_is_zero(d - round(d));
}

// Is v in the interiour of a diamond?
static bool in_diamond_int(const geom::vector &v)
{
    double d1 = diamonds.ls1.index(v);
    double d2 = diamonds.ls2.index(v);
    return (!double_is_integral(d1) && !double_is_integral(d2)
            && (ifloor(d1) + ifloor(d2)) % 2 == 0);
}

// Is v on a grid line?
static bool on_line(const geom::vector &v)
{
    double d1 = diamonds.ls1.index(v);
    double d2 = diamonds.ls2.index(v);
    return (double_is_integral(d1) || double_is_integral(d2));
}

// Is v an intersection of grid lines?
static bool is_corner(const geom::vector &v)
{
    double d1 = diamonds.ls1.index(v);
    double d2 = diamonds.ls2.index(v);
    return (double_is_integral(d1) && double_is_integral(d2));
}

// Is v in a diamond?
static bool in_diamond(const geom::vector &v)
{
    return (in_diamond_int(v) || is_corner(v));
}

// Is v in the interiour of a non-diamond?
static bool in_non_diamond_int(const geom::vector &v)
{
    // This could instead be done like in_diamond_int.
    return (!in_diamond(v) && !on_line(v));
}

static bool _to_grid(geom::ray *r, bool half);
// Is r on a corner and heading into a non-diamond?
static bool bad_corner(const geom::ray &r)
{
    if (!is_corner(r.start))
        return false;
    geom::ray copy = r;
    _to_grid(&copy, true);
    return in_non_diamond_int(copy.start);
}

static coord_def floor_vec(const geom::vector &v)
{
    int x = ifloor(v.x);
    int y = ifloor(v.y);
    return coord_def(x, y);
}

coord_def ray_def::pos() const
{
    ASSERT(_valid());
    // XXX: pretty arbitrary if we're just on a corner.
    return floor_vec(r.start);
}

static void _round_to_corner(geom::ray *r)
{
    geom::vector v = 2.0 * r->start;
    v.x = round(v.x);
    v.y = round(v.y);
    ASSERT((iround(v.x) + iround(v.y)) % 2 == 1);
    r->start = 0.5 * v;
}

static void _round_to_grid(geom::ray *r)
{
    // x + y or x - y is of the form 0.5+k
    geom::vector v = r->start;
    double sum = v.x + v.y - 0.5;
    double diff = v.x - v.y - 0.5;
    double deltas = round(sum) - sum;
    double deltad = round(diff) - diff;
    if (abs(deltas) <= abs(deltad))
    {
        v.x += 0.5 * deltas;
        v.y += 0.5 * deltas;
    }
    else
    {
        v.x += 0.5 * deltad;
        v.y -= 0.5 * deltad;
    }
    r->start = v;
}

static bool _to_next_cell(geom::ray *r)
{
    bool c = r->to_next_cell(diamonds);
    if (c)
        _round_to_corner(r);
    return c;
}

static bool _to_grid(geom::ray *r, bool half)
{
    bool c = r->to_grid(diamonds, half);
    if (!half)
        _round_to_grid(r);
    c = c || is_corner(r->start);
    if (c)
        _round_to_corner(r);
    return c;
}

static bool _advance_from_non_diamond(geom::ray *r)
{
    ASSERT(in_non_diamond_int(r->start));
    if (!_to_next_cell(r))
    {
        ASSERT(in_diamond_int(r->start));
        return false;
    }
    else
    {
        // r is now on a corner, going from non-diamond to non-diamond.
        ASSERT(is_corner(r->start));
        return true;
    }
}

// The ray is in a legal state to be passed around externally.
bool ray_def::_valid() const
{
    return (on_corner && is_corner(r.start) && bad_corner(r)
            || !on_corner && in_diamond_int(r.start));
}

static geom::vector _normalize(const geom::vector &v)
{
    double n = sqrt(v.x*v.x + v.y*v.y);
    return ((1.0 / n) * v);
}

// Return true if we didn't hit a corner, hence if this
// is a good ray so far.
bool ray_def::advance()
{
    ASSERT(_valid());
    r.dir = _normalize(r.dir);
    if (on_corner)
    {
        ASSERT(is_corner(r.start));
        on_corner = false;
        _to_grid(&r, true);
    }
    else
    {
        // Starting inside a diamond.
        bool c = _to_next_cell(&r);

        if (c)
        {
            // r is now on a corner, going from diamond to diamond.
            _to_grid(&r, true);
            ASSERT(_valid());
            return true;
        }
    }

    // Now inside a non-diamond.
    ASSERT(in_non_diamond_int(r.start));

    on_corner = _advance_from_non_diamond(&r);
    ASSERT(_valid());
    return (!on_corner);
}

void ray_def::regress()
{
    ASSERT(_valid());
    r.dir = -r.dir;
    advance();
    r.dir = -r.dir;
    ASSERT(_valid());
}

static geom::vector _mirror_pt(const geom::vector &vorig, const coord_def &side)
{
    geom::vector v = vorig;
    if (side.x == -1)
        v.x = 1.0 - v.x;
    if (side.y == -1)
        v.y = 1.0 - v.y;
    return v;
}

static geom::vector _mirror_dir(const geom::vector &vorig, const coord_def &side)
{
    geom::vector v = vorig;
    if (side.x == -1)
        v.x = -v.x;
    if (side.y == -1)
        v.y = -v.y;
    return v;
}

static geom::ray _mirror(const geom::ray &rorig, const coord_def &side)
{
    geom::ray r;
    r.start = _mirror_pt(rorig.start, side);
    r.dir = _mirror_dir(rorig.dir, side);
    return r;
}

static geom::line _choose_reflect_line(bool rx, bool ry, bool rxy)
{
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
    return l;
}

static geom::vector _fudge_corner(const geom::vector &w, const reflect_grid &rg)
{
    geom::vector v = w;
    if (double_is_integral(v.x))
    {
        // just try both sides
        v.x += 10 * EPSILON_VALUE;
        if (rg(floor_vec(v)))
            v.x -= 20 * EPSILON_VALUE;
        ASSERT(!rg(floor_vec(v)));
    }
    else
    {
        ASSERT(double_is_integral(v.y));
        v.y += 10 * EPSILON_VALUE;
        if (rg(floor_vec(v)))
            v.y -= 20 * EPSILON_VALUE;
        ASSERT(!rg(floor_vec(v)));
    }
    return v;
}

// Bounce a ray leaving (0,0) through the positive side
// along a diagonal corridor between (0,1) and (1,0) until
// it's inside (1,1).
static geom::ray _bounce_diag_corridor(const geom::ray &rorig)
{
    geom::ray r = rorig;
    geom::form wall(1, -1);
    // The actual walls: geom::line l1(1, -1, 0.5), l2(1, -1, -0.5);
    geom::line k(1, 1, 2.5);
    ASSERT(k.f(r.dir) > 0); // We're actually moving towards k.
    ASSERT(!geom::parallel(r.dir, geom::form(1, -1)));
    // Now bounce back and forth between l1 and l2 until we hit k.
    while (!double_is_zero(geom::intersect(r, k)))
    {
        _to_grid(&r, false);
        r.dir = reflect(r.dir, wall);
    }
    // Now pointing inside the destination cell (1,1) -- move inside.
    _to_grid(&r, true);
    return r;
}

// Bounce a ray leaving (0,0) properly through one of the sides
// of the diamond.
// r is positioned on the edge already, and side says which
// side this is.
static geom::ray _bounce_noncorner(const geom::ray &r, const coord_def &side,
                            const reflect_grid &rg)
{
    // Mirror r to have it leave through the positive side.
    geom::ray rmirr = _mirror(r, side);

    // Determine which of the three relevant cells are bouncy.
    const coord_def dx = coord_def(side.x, 0);
    const coord_def dy = coord_def(0, side.y);
    bool rx  = rg(dx);
    bool ry  = rg(dy);
    bool rxy = rg(dx + dy);
    // One of the three neighbours on this side must be bouncy.
    ASSERT(rx || ry || rxy);

    // Now go through the cases separately.
    if (rx && ry && !rxy)
        rmirr = _bounce_diag_corridor(rmirr);
    else
    {
        // These all reduce to reflection at one line.
        geom::line l = _choose_reflect_line(rx, ry, rxy);

        double t = intersect(rmirr, l);
        ASSERT(double_is_zero(t) || t >= 0);
        rmirr.advance(t);
        rmirr.dir = geom::reflect(rmirr.dir, l.f);
        if (bad_corner(rmirr))
        {
            // Really want to stay and set on_corner.
            // But then pos() might be a solid cell.
            geom::vector v = _mirror_pt(rmirr.start, side);
            v = _fudge_corner(v, rg);
            rmirr.start = _mirror_pt(v, side);
        }
        else
        {
            // Depending on the case, we're on some diamond edge
            // or between diamonds. We'll just move safely into
            // the next one.
            _to_grid(&rmirr, true);
            if (in_non_diamond_int(rmirr.start))
                _advance_from_non_diamond(&rmirr);
        }
    }

    // Mirror back.
    return _mirror(rmirr, side);
}

static geom::form _corner_wall(const coord_def &side, const reflect_grid &rg)
{
    coord_def e;
    if (side.x == 0)
        e = coord_def(1, 0);
    else
        e = coord_def(0, 1);
    ASSERT(!rg(coord_def(0,0)) && rg(side));
    // Reflect back by an orthogonal wall...
    coord_def wall = e;
    // unless the wall is clearly diagonal:
    //  ##.
    //  #*.  (with side.y == -1)
    if (rg(e) && rg(side+e) && !rg(-e) && !rg(side-e))
    {
        // diagonal wall through side and e
        wall = side - e;
    }
    else if (rg(-e) && rg(side-e) && !rg(e) && !rg(side+e))
    {
        // diagonal wall through side and -e
        wall = side + e;
    }
    return geom::form(wall.y, -wall.x);
}

// Bounce a ray that leaves cell (0,0) through a corner. We could
// just fudge it a little, but want to be consistent for rays
// shot in cardinal directions.
static geom::ray _bounce_corner(const geom::ray &rorig, const coord_def &side,
                         const reflect_grid &rg)
{
    geom::ray r = rorig;
    geom::form f = _corner_wall(side, rg);

    if (r.dir.x == 0 || r.dir.y == 0)
    {
        // To keep cardinal rays nicely in the middle,
        // we reflect them earlier.
        r.start.x = r.start.y = 0.5;
        r.dir = geom::reflect(r.dir, f);
        ASSERT(r.dir.x == 0 || r.dir.y == 0);
    }
    else
    {
        // Others are reflected at the corner.
        r.dir = geom::reflect(r.dir, f);
        if (f.a != 0 && f.b != 0)
        {
            // Diagonal wall: to the next diamond, then inside.
            _to_grid(&r, false);
            _to_grid(&r, true);
        }
        else
        {
            // Back inside diamond (0,0).
            _to_grid(&r, true);
        }
    }
    return r;
}

// Nudge an on-corner ray to be inside the diamond.
void ray_def::nudge_inside()
{
    ASSERT(on_corner);
    geom::vector centre(pos().x + 0.5, pos().y + 0.5);
    // Move a little bit towards cell center.
    r.start = 0.9 * r.start + 0.1 * centre;
    on_corner = false;
    ASSERT(in_diamond_int(r.start));
}

void ray_def::bounce(const reflect_grid &rg)
{
    ASSERT(_valid());
    ASSERT(!rg(coord_def(0,0))); // The cell we bounce from is not solid.
#ifdef ASSERTS
    const coord_def old_pos = pos();
#endif

    // If we're exactly on a corner, adjust to slightly inside the diamond.
    if (on_corner)
        nudge_inside();

    // Translate to cell (0,0).
    geom::vector p(pos().x, pos().y);
    geom::ray rtrans;
    rtrans.start = r.start - p;
    rtrans.dir = r.dir;

    // Move to the diamond edge to determine the side.
    coord_def side;
    bool corner = _to_grid(&rtrans, false);
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

    if (corner)
        rtrans = _bounce_corner(rtrans, side, rg);
    else
        rtrans = _bounce_noncorner(rtrans, side, rg);

    // Translate back.
    r.start = rtrans.start + p;
    r.dir = rtrans.dir;

    // Set on_corner if we happen to have ended up on a corner.
    on_corner = is_corner(r.start);

    ASSERT(_valid());
    ASSERT(!rg(pos() - old_pos));
}

double ray_def::get_degrees() const
{
    return geom::degrees(r.dir);
}

void ray_def::set_degrees(double d)
{
    // Changing the angle while on a diamond corner causes problems when the
    // new direction points inside a diamond (#5892).  Move the ray slightly
    // inside the diamond first correct for this.
    if (on_corner)
        nudge_inside();
    r.dir = geom::degree_to_vector(d);
}
