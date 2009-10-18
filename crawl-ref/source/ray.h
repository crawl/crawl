/*
 *  File:       ray.h
 *  Summary:    Ray definition
 *  Written by: Linley Henzell
 */

#ifndef RAY_H
#define RAY_H

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
    // Quadrant by sign of x/y coordinate.
    int quadx;
    int quady;
    // For cycling: where did we come from?
    int cycle_idx;

public:
    ray_def(double accx = 0.0, double accy = 0.0, double slope = 0.0,
            int quadx = 1, int quady = 1, int fullray_idx = -1);
    int x() const;
    int y() const;
    coord_def pos() const;

    // returns the direction taken
    adv_type advance(bool shorten = false, const coord_def *p = NULL);
    adv_type advance_through(const coord_def &point);
    void advance_and_bounce();
    void regress();

    // Gets/sets the slope in terms of degrees, with 0 = east, 90 = north,
    // 180 = west, 270 = south, 360 = east, -90 = south, etc
    double get_degrees() const;
    void   set_degrees(double deg);

protected:
    adv_type raw_advance_pos();
    void flip();
    adv_type raw_advance();
    double reflect(bool x, double oldc, double newc) const;
    void set_reflect_point(const double oldx, const double oldy,
                           bool blocked_x, bool blocked_y);
};

#endif
