#include "AppHdr.h"

#include "coord-circle.h"

#include "coordit.h"
#include "libutil.h"
#include "los.h"

bool rect_def::contains(const coord_def& p) const
{
    return p.x >= min.x && p.x <= max.x && p.y >= min.y && p.y <= max.y;
}

rect_def rect_def::intersect(const rect_def& other) const
{
    rect_def res;
    res.min.x = ::max(min.x, other.min.x);
    res.min.y = ::max(min.y, other.min.y);
    res.max.x = ::min(max.x, other.max.x);
    res.max.y = ::min(max.y, other.max.y);
    return res;
}

rectangle_iterator rect_def::iter() const
{
    return rectangle_iterator(min, max);
}

circle_def::circle_def()
    : los_radius(true), check_bounds(false), origin(coord_def(0,0))
{
    // Set up bounding box and shape.
    init(LOS_RADIUS, C_ROUND);
}

circle_def::circle_def(const coord_def& origin_, const circle_def& bds)
    : los_radius(bds.los_radius), check_bounds(true),
      origin(origin_),
      radius(bds.radius), radius_sq(bds.radius_sq)
{
    // Set up bounding box.
    init_bbox();
}

circle_def::circle_def(int param, circle_type ctype)
    : los_radius(false), check_bounds(false), origin(coord_def(0,0))
{
    init(param, ctype);
}

circle_def::circle_def(const coord_def &origin_, int param,
                       circle_type ctype)
    : los_radius(false), check_bounds(true), origin(origin_)
{
    init(param, ctype);
}

void circle_def::init(int param, circle_type ctype)
{
    switch (ctype)
    {
    case C_CIRCLE:
        radius_sq = param;
        radius = isqrt_ceil(radius_sq);
        break;
    case C_ROUND:
        radius = param;
        radius_sq = radius * radius + 1;
        break;
    case C_POINTY:
        radius = param;
        radius_sq = radius * radius;
    }
    init_bbox();
}

void circle_def::init_bbox()
{
    bbox = rect_def(origin - coord_def(radius, radius),
                    origin + coord_def(radius, radius));
    if (check_bounds)
        bbox = bbox.intersect(RECT_MAP_BOUNDS);
}

const rect_def& circle_def::get_bbox() const
{
    return bbox;
}

const coord_def& circle_def::get_center() const
{
    return origin;
}

bool circle_def::contains(const coord_def &p) const
{
    if (!bbox.contains(p))
        return false;

    int r_sq = los_radius ? los_radius2 : radius_sq;
    return (p - origin).abs() <= r_sq;
}
