#ifndef COORD_CIRCLE_H
#define COORD_CIRCLE_H

#include "coordit.h"

enum shape_type
{
    SH_SQUARE,     // square around an origin
    SH_CIRCLE      // circle around an origin
};

enum circle_type
{
    C_SQUARE,
    C_CIRCLE,
    C_POINTY,
    C_ROUND
};

class rect_def
{
    coord_def min;
    coord_def max;

public:
    rect_def() {}
    rect_def(const coord_def &min_, const coord_def &max_)
        : min(min_), max(max_) {}

    rectangle_iterator iter() const;
};

class circle_iterator;
class circle_def
{
    bool los_radius;
    shape_type shape;

    coord_def origin;
    int radius;
    int radius_sq;
    rect_def bbox;

public:
    // Circle around (0,0) with radius that tracks global LOS radius.
    circle_def();
    // Circle around (0,0) of specified shape and size.
    explicit circle_def(int param, circle_type ctype = C_SQUARE);
    // Circle around given origin of specified shape and size.
    circle_def(const coord_def &origin_, int param, circle_type ctype = C_SQUARE);

    bool contains(const coord_def &p) const;
    const rect_def& get_bbox() const;

    circle_iterator iter() const;

private:
    void init(int param, circle_type ctype);
};

class circle_iterator
{
    const circle_def &circle;
    rectangle_iterator iter;

public:
    circle_iterator(const circle_def &circle_);

    operator bool() const;
    coord_def operator*() const;

    void operator++();
    void operator++(int);
};

#endif
