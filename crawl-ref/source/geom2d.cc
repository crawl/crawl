/*
 * File:     geom2d.cc
 * Summary:  Plane geometry for shooting rays.
 */

#include "AppHdr.h"
#include "debug.h"
#include "geom2d.h"

#include <cmath>

namespace geom {

static bool double_is_zero(double d)
{
    return (std::abs(d) < 0.0000001);
}

// Is v parallel to the kernel of f?
static bool parallel(vector v, form f)
{
    return (double_is_zero(f(v)));
}

vector ray::shoot(double t) const
{
    return (start + t*dir);
}

void ray::advance(double t)
{
    start = shoot(t);
}

// Determine the value of t such that
//   r.start + t * r.dir lies on the line l.
// r and l must not be parallel.
double intersect(const ray &r, const line &l)
{
    ASSERT(!parallel(r.dir, l.f));
    double fp = l.f(r.start);
    double fd = l.f(r.dir);
    double t = (l.val - fp) / fd;
    return (t);
}

// Find the next intersection of r with a line in ls.
double nextintersect(const ray &r, const lineseq &ls)
{
    // ray must not be parallel to the lines
    ASSERT(!parallel(r.dir, ls.f));
    double fp = ls.f(r.start);
    double fd = ls.f(r.dir);
    // want smallest positive t with
    // fp + t*fd = ls.offset + k*ls.dist, k integral
    double a = (fp - ls.offset) / ls.dist;
    double k = (ls.dist*fd) > 0 ? ceil(a) : floor(a);
    double t = (k - a) * ls.dist / fd;
    return (t);
}

// Line distance in the ray parameter.
//   ls.f(r.shoot(tdist)) == ls.f(r.start) +- ls.dist
static double tdist(const ray &r, const lineseq &ls)
{
    ASSERT(!parallel(r.dir, ls.f));
    return (std::abs(ls.dist / ls.f(r.dir)));
}

// Shoot the ray inside the next cell.
// Returns true if successfully advanced into a new cell,
// false if it hit a corner.
bool ray::to_next_cell(const grid &g)
{
    // ASSERT(!parallel(g.vert, g.horiz));
    lineseq ls;
    if (parallel(dir, g.ls1.f))
        ls = g.ls2;
    else if (parallel(dir, g.ls2.f))
        ls = g.ls1;
    else
    {
        double s = nextintersect(*this, g.ls1);
        double t = nextintersect(*this, g.ls2);
        double sd = tdist(*this, g.ls1);
        double td = tdist(*this, g.ls2);
        if (double_is_zero(s - t))
        {
            // Move onto the corner. The fact that we're on a corner
            // should be tracked externally.
            advance(s);
            return (false);
        }
        double dmin = std::min(s,t);
        double dnext = std::min(std::max(s,t), dmin + std::min(sd, td));
        advance((dmin + dnext) / 2.0);
        return (true);
    }
    // parallel: just advance an extra half
    double s = nextintersect(*this, ls);
    advance(s + 0.5 * tdist(*this, ls));
    return (true);
}

// Move a ray by half a cell: starting at a corner of the grid,
// it ends up within the next cell.
void ray::move_half_cell(const grid &g)
{
    // ASSERT(!parallel(g.vert, g.horiz));
    lineseq ls;
    double t;
    if (parallel(dir, g.ls1.f))
        t = tdist(*this, g.ls2);
    else if (parallel(dir, g.ls2.f))
        t = tdist(*this, g.ls1);
    else
        t = std::min(tdist(*this, g.ls1), tdist(*this, g.ls2));
    advance(0.5 * t);
}

vector normal(const form &f)
{
    return (vector(f.a, f.b));
}

vector reflect(const vector &v, const form &f)
{
    vector n = normal(f);
    return (v - 2 * f(v)/f(n) * n);
}

// vector space implementation

const vector& vector::operator+=(const vector &v)
{
    x += v.x;
    y += v.y;
    return (*this);
}

vector vector::operator+(const vector &v) const
{
    vector copy = *this;
    copy += v;
    return (copy);
}

vector vector::operator-() const
{
    return ((-1) * (*this));
}

const vector& vector::operator-=(const vector &v)
{
    return (*this += -v);
}

vector vector::operator-(const vector &v) const
{
    return (*this + (-v));
}

vector operator*(double t, const vector &v)
{
    return (vector(t*v.x, t*v.y));
}

double form::operator()(const vector& v) const
{
    return (a*v.x + b*v.y);
}

}

