/*
 *  File:       ray.h
 *  Summary:    Ray definition
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 *
 *  Change History (most recent first):
 *
 * <1>     7/11/07    MPC   Split off from externs.h
 */

#ifndef RAY_H
#define RAY_H

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

private:
    int raw_advance();
    double reflect(bool x, double oldc, double newc) const;
    double reflect(double x, double c) const;
    void set_reflect_point(const double oldx, const double oldy,
                           double *newx, double *newy,
                           bool blocked_x, bool blocked_y);
};

#endif
