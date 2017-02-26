#pragma once

#include "los.h"

class los_def
{
    los_grid show;
    coord_def center;
    opacity_func const * opc;
    circle_def bds;

public:
    los_def();
    los_def(const coord_def& c, const opacity_func &o = opc_default,
                                const circle_def &b = BDS_DEFAULT);
    los_def(const los_def& l);
    ~los_def();
    los_def& operator=(const los_def& l);
    void init(const coord_def& center, const opacity_func& o,
                                       const circle_def& b);
    void set_center(const coord_def& center);
    coord_def get_center() const;
    void set_opacity(const opacity_func& o);
    void set_bounds(const circle_def& b);
    circle_def get_bounds() const;

    void update();
    bool in_bounds(const coord_def& p) const;
    bool see_cell(const coord_def& p) const;
};

