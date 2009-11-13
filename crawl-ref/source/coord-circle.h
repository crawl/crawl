#ifndef COORD_CIRCLE_H
#define COORD_CIRCLE_H

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

class rectangle_iterator;
class rect_def
{
    coord_def min;
    coord_def max;

public:
    rect_def() {}
    rect_def(const coord_def &min_, const coord_def &max_)
        : min(min_), max(max_) {}

    bool contains(const coord_def& p) const;
    rect_def intersect(const rect_def& other) const;
    rectangle_iterator iter() const;
};

#define RECT_MAP_BOUNDS (rect_def(coord_def(X_BOUND_1, Y_BOUND_1), \
                                  coord_def(X_BOUND_2, Y_BOUND_2)))

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
    const coord_def& get_center() const;

    circle_iterator iter() const;

private:
    void init(int param, circle_type ctype);
};

#endif
