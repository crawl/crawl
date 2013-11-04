#ifndef COORD_CIRCLE_H
#define COORD_CIRCLE_H

enum circle_type
{
    C_CIRCLE,      // circle specified by pre-squared radius
    C_POINTY,      // circle with square radius r*r
    C_ROUND,       // circle with square radius r*r+1
};

class rectangle_iterator;
class rect_def
{
    coord_def min;
    coord_def max;

public:
    rect_def() {}
    rect_def(const coord_def &min_, const coord_def &max_)
        : min(min_), max(max_) {}

    bool contains(const coord_def& p) const PURE;
    rect_def intersect(const rect_def& other) const PURE;
    rectangle_iterator iter() const;
};

#define RECT_MAP_BOUNDS (rect_def(coord_def(X_BOUND_1, Y_BOUND_1), \
                                  coord_def(X_BOUND_2, Y_BOUND_2)))

class circle_iterator;
/*
 * Circles of different shapes; see circle_type for these.
 *
 * radius/radius_sq don't have meaning when los_radius is set.
 */
class circle_def
{
    // Are we tracking global LOS radius?
    bool los_radius;

    // Check against map bounds for containment?
    bool check_bounds;

    coord_def origin;

    int radius;
    int radius_sq;
    rect_def bbox;

public:
    // Circle around (0,0) with radius that tracks global LOS radius.
    // No bounds checking.
    circle_def();
    // Circle around origin with shape of given circle.
    circle_def(const coord_def &origin_, const circle_def &bds);
    // Circle around (0,0) of specified shape and size.
    // No bounds checking.
    explicit circle_def(int param, circle_type ctype);
    // Circle around given origin of specified shape and size.
    circle_def(const coord_def &origin_, int param, circle_type ctype);

    bool contains(const coord_def &p) const PURE;
    const rect_def& get_bbox() const PURE;
    const coord_def& get_center() const PURE;

    circle_iterator iter() const;

private:
    void init(int param, circle_type ctype);
    void init_bbox();
};

#endif
