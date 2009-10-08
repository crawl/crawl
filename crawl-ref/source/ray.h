/*
 *  File:       ray.h
 *  Summary:    Ray definition
 *  Written by: Linley Henzell
 */

#ifndef RAY_H
#define RAY_H

int shoot_ray(double accx, double accy, const double slope,
                int maxrange, int xpos[], int ypos[]);

struct ray_def
{
public:
    double accx;
    double accy;
    double slope;
    // Quadrant 1: down-right
    // Quadrant 2: down-left
    // Quadrant 3: up-left
    // Quadrant 4: up-right
    int quadrant;
    int fullray_idx;            // for cycling: where did we come from?

public:
    ray_def();
    int x() const { return static_cast<int>(accx); }
    int y() const { return static_cast<int>(accy); }
    coord_def pos() const { return coord_def(x(), y()); }

    // returns the direction taken (0,1,2)
    int advance(bool shorten = false, const coord_def *p = NULL);
    int advance_through(const coord_def &point);
    void advance_and_bounce();
    void regress();

    // Gets/sets the slope in terms of degrees, with 0 = east, 90 = north,
    // 180 = west, 270 = south, 360 = east, -90 = south, etc
    double get_degrees() const;
    void   set_degrees(double deg);

private:
    int raw_advance();
    double reflect(bool x, double oldc, double newc) const;
    double reflect(double x, double c) const;
    void set_reflect_point(const double oldx, const double oldy,
                           double *newx, double *newy,
                           bool blocked_x, bool blocked_y);
};

#endif
