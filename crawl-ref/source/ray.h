/*
 *  File:       ray.h
 *  Summary:    Ray definition
 */

#ifndef RAY_H
#define RAY_H

#include "geom2d.h"

struct ray_def
{
    geom::ray r;
    int cycle_idx;

    ray_def() {}
    ray_def(const geom::ray& _r)
        : r(_r), cycle_idx(0) {}

    coord_def pos() const;
    bool advance();
    void advance_and_bounce();
    void regress();
};

#endif
