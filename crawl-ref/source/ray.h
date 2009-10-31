/*
 *  File:       ray.h
 *  Summary:    Ray definition
 */

#ifndef RAY_H
#define RAY_H

#include "fixary.h"
#include "geom2d.h"

typedef FixedArray<bool,3,3> reflect_grid;
const coord_def rg_o = coord_def(1,1);

struct ray_def
{
    geom::ray r;
    bool on_corner;
    int cycle_idx;

    ray_def() : on_corner(false), cycle_idx(-1) {}
    ray_def(const geom::ray& _r)
        : r(_r), on_corner(false), cycle_idx(-1) {}

    coord_def pos() const;
    bool advance();
    coord_def move_to_side();
    void bounce(const reflect_grid &rg);
    void regress();
};

#endif
