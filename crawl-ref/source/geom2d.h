#pragma once

namespace geom
{

struct vector
{
    double x;
    double y;

    vector(double _x = 0.0, double _y = 0.0)
        : x(_x), y(_y) {}

    const vector& operator+=(const vector &v);
    vector operator+(const vector &v) const PURE;
    vector operator-() const PURE;
    const vector& operator-=(const vector &v);
    vector operator-(const vector &v) const PURE;
};

vector operator*(double t, const vector &v) PURE;

struct form
{
    double a;
    double b;

    form(double _a = 0.0, double _b = 0.0)
        : a(_a), b(_b) {}

    double operator()(const vector &v) const;
};

// A ray in two-dimensional space given by starting point
// and direction vector.
// The points of R are start + t*dir.
struct grid;
struct ray
{
    vector start;
    vector dir;

    ray() {}
    ray(double x0, double y0, double xd, double yd)
        : start(x0, y0), dir(xd, yd) {}

    vector shoot(double t) const;
    void advance(double t);
    bool to_grid(const grid& g, bool half);
    bool to_next_cell(const grid& g);
};

// A line in two-dimensional space as the preimage of a number
// under a linear form. L = form^{-1}(val).
struct line
{
    form f;
    double val;

    line() {}
    line(double a, double b, double v)
        : f(a,b), val(v) {}
};

// A sequence of evenly spaced parallel lines, like the
// horizontal or vertical lines in a grid.
// Lines are f^{-1}(offset + k*dist) for integers k.
struct lineseq
{
    form f;
    double offset;
    double dist;

    lineseq() {}
    lineseq(double a, double b, double o, double d)
        : f(a,b), offset(o), dist(d) {}

    double index(const vector &v) const PURE;
};

struct grid
{
    lineseq ls1;
    lineseq ls2;

    grid(lineseq l1, lineseq l2) : ls1(l1), ls2(l2) {}
};

double intersect(const ray &r, const line &l);
double nextintersect(const ray &r, const lineseq &ls);
bool parallel(const vector& v, const form &f);
vector reflect(const vector& v, const form &f);

}

