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
    int quadrant;      // 0 down-right, 1 down-left, 2 up-left, 3 up-right
    int fullray_idx;   // for cycling: where did we come from?

public:
    ray_def(double accx = 0.0, double accy = 0.0, double slope = 0.0,
            int quadrant = 0, int fullray_idx = -1);
    int x() const;
    int y() const;
    coord_def pos() const;

    // returns the direction taken (0,1,2)
    int advance(bool shorten = false, const coord_def *p = NULL);
    int advance_through(const coord_def &point);
    void advance_and_bounce();
    void regress();

    int footprint(int radius2, coord_def pos[]) const;

    // Gets/sets the slope in terms of degrees, with 0 = east, 90 = north,
    // 180 = west, 270 = south, 360 = east, -90 = south, etc
    double get_degrees() const;
    void   set_degrees(double deg);

private:
    int _find_next_intercept();
    int raw_advance();
    double reflect(bool x, double oldc, double newc) const;
    double reflect(double x, double c) const;
    void set_reflect_point(const double oldx, const double oldy,
                           double *newx, double *newy,
                           bool blocked_x, bool blocked_y);
};

#endif
