#include "AppHdr.h"

#include "coord-circle.h"

#include "coordit.h"

#include <cmath>

rectangle_iterator rect_def::iter() const
{
    return (rectangle_iterator(min, max));
}

circle_def::circle_def()
    : los_radius(true), origin(coord_def(0,0))
{
    // Set up bounding box and shape.
    init(LOS_MAX_RADIUS, C_ROUND);
}

circle_def::circle_def(int param, circle_type ctype)
    : los_radius(false), origin(coord_def(0,0))
{
    init(param, ctype);
}

circle_def::circle_def(const coord_def &origin_, int param,
                       circle_type ctype)
    : los_radius(false), origin(origin_)
{
    init(param, ctype);
}

void circle_def::init(int param, circle_type ctype)
{
    switch (ctype)
    {
    case C_SQUARE:
        shape = SH_SQUARE;
        radius = param;
        break;
    case C_CIRCLE:
        shape = SH_CIRCLE;
        radius_sq = param;
        radius = static_cast<int>(ceil(sqrt(radius_sq)));
        break;
    case C_ROUND:
        shape = SH_CIRCLE;
        radius = param;
        radius_sq = radius * radius + 1;
        break;
    case C_POINTY:
        shape = SH_CIRCLE;
        radius = param;
        radius_sq = radius * radius;
    }
    bbox = rect_def(origin - coord_def(radius, radius),
                    origin + coord_def(radius, radius));
}

const rect_def& circle_def::get_bbox() const
{
    return (bbox);
}

bool circle_def::contains(const coord_def &p) const
{
    switch (shape)
    {
    case SH_SQUARE:
        return ((p - origin).rdist() <= radius);
    case SH_CIRCLE:
        return ((p - origin).abs() <= radius_sq);
    default:
        return (false);
    }
}

circle_iterator::circle_iterator(const circle_def &circle_)
    : circle(circle_), iter(circle_.get_bbox().iter())
{
    while (iter && !circle.contains(*iter))
        ++iter;
}

circle_iterator::operator bool() const
{
    return ((bool)iter);
}

coord_def circle_iterator::operator*() const
{
    return (*iter);
}

void circle_iterator::operator++()
{
    do
        ++iter;
    while (iter && !circle.contains(*iter));
}

void circle_iterator::operator++(int)
{
    ++(*this);
}
