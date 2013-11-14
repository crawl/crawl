/**
 * @file
 * @brief Plane geometry for shooting rays.
**/

#include "AppHdr.h"
#include "geom2d.h"

#include <cmath>

namespace geom
{

static bool double_is_zero(double d)
{
    return abs(d) < 0.0000001;
}

// Is v parallel to the kernel of f?
bool parallel(const vector &v, const form &f)
{
    return double_is_zero(f(v));
}

vector ray::shoot(double t) const
{
    return start + t*dir;
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
    return t;
}

double lineseq::index(const vector &v) const
{
    return ((f(v) - offset) / dist);
}

// Find the next intersection of r with a line in ls.
double nextintersect(const ray &r, const lineseq &ls)
{
    // ray must not be parallel to the lines
    ASSERT(!parallel(r.dir, ls.f));
    double fp = ls.f(r.start);
    double fd = ls.f(r.dir);
    // want smallest positive t > 0 with
    // fp + t*fd = ls.offset + k*ls.dist, k integral
    double a = (fp - ls.offset) / ls.dist;
    double k = (ls.dist*fd) > 0 ? ceil(a) : floor(a);
    if (double_is_zero(k - a))
        k += (ls.dist*fd > 0 ? 1 : -1);
    double t = (k - a) * ls.dist / fd;
    return t;
}

// Move the ray towards the next grid line.
// Half way there if "half" is true, else all the way there
// Returns whether it hit a corner.
bool ray::to_grid(const grid &g, bool half)
{
    // ASSERT(!parallel(g.vert, g.horiz));
    bool corner = false;
    double t = 0;
    if (parallel(dir, g.ls1.f))
        t = nextintersect(*this, g.ls2);
    else if (parallel(dir, g.ls2.f))
        t = nextintersect(*this, g.ls1);
    else
    {
        double r = nextintersect(*this, g.ls1);
        double s = nextintersect(*this, g.ls2);
        t = min(r, s);
        corner = double_is_zero(r - s);
    }
    advance(half ? 0.5 * t : t);
    return corner && !half;
}

// Shoot the ray inside the next cell, stopping at corners.
// Returns true if it hit a corner.
bool ray::to_next_cell(const grid &g)
{
    if (to_grid(g, false))
        return true;
    to_grid(g, true);
    return false;
}

vector reflect(const vector &v, const form &f)
{
    vector normal = vector(f.a, f.b);
    return (v - 2 * f(v)/f(normal) * normal);
}



//////////////////////////////////////////////////
// vector space implementation

const vector& vector::operator+=(const vector &v)
{
    x += v.x;
    y += v.y;
    return *this;
}

vector vector::operator+(const vector &v) const
{
    vector copy = *this;
    copy += v;
    return copy;
}

vector vector::operator-() const
{
    return ((-1) * (*this));
}

const vector& vector::operator-=(const vector &v)
{
    return *this += -v;
}

vector vector::operator-(const vector &v) const
{
    return (*this + (-v));
}

vector operator*(double t, const vector &v)
{
    return vector(t*v.x, t*v.y);
}

double form::operator()(const vector& v) const
{
    return a*v.x + b*v.y;
}

double degrees(const vector &v)
{
    if (v.x == 0)
        return v.y > 0 ? 90.0 : -90.0;
    double rad = v.x > 0 ? atan(v.y/v.x) : M_PI + atan(v.y/v.x);
    return 180.0 / M_PI * rad;
}

vector degree_to_vector(double d)
{
    double rad = d / 180.0 * M_PI;
    return vector(cos(rad), sin(rad));
}

}
