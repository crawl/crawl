/*
 *  File:       ray.h
 *  Summary:    Ray definition
 *  Written by: Linley Henzell
 */

#ifndef RAY_H
#define RAY_H

// quadrant
enum quad_type
{
    QUAD_SE = 0,
    QUAD_SW = 1,
    QUAD_NW = 2,
    QUAD_NE = 3
};

// direction of advance:
enum adv_type
{
    ADV_X  = 0, // changed x
    ADV_Y  = 1, // changed y
    ADV_XY = 2  // changed x and y (diagonal)
};

struct ray_def
{
public:
    double accx;
    double accy;
    double slope;
    quad_type quadrant;
    int fullray_idx;   // for cycling: where did we come from?

public:
    ray_def(double accx = 0.0, double accy = 0.0, double slope = 0.0,
            quad_type quadrant = QUAD_SE, int fullray_idx = -1);
    int x() const;
    int y() const;
    coord_def pos() const;

    // returns the direction taken
    adv_type advance(bool shorten = false, const coord_def *p = NULL);
    adv_type advance_through(const coord_def &point);
    void advance_and_bounce();
    void regress();

    int footprint(int radius2, coord_def pos[]) const;

    // Gets/sets the slope in terms of degrees, with 0 = east, 90 = north,
    // 180 = west, 270 = south, 360 = east, -90 = south, etc
    double get_degrees() const;
    void   set_degrees(double deg);

private:
    adv_type raw_advance_0();
    void flip();
    adv_type raw_advance();
    double reflect(bool x, double oldc, double newc) const;
    void set_reflect_point(const double oldx, const double oldy,
                           bool blocked_x, bool blocked_y);
};

#endif
