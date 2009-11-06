#ifndef LOS_DEF_H
#define LOS_DEF_H

#include "losparam.h"

class los_def
{
    env_show_grid show;
    coord_def center;
    opacity_func const * opc;
    bounds_func const * bds;

public:
    los_def();
    los_def(const coord_def& c, const opacity_func &o = opc_default,
                                const bounds_func &b = bds_default);
    los_def(const los_def& l);
    ~los_def();
    los_def& operator=(const los_def& l);
    void init(const coord_def& center, const opacity_func& o,
                                       const bounds_func& b);
    void set_center(const coord_def& center);
    void set_opacity(const opacity_func& o);
    void set_bounds(const bounds_func& b);

    void update();
    bool in_bounds(const coord_def& p) const;
    bool see_cell(const coord_def& p) const;
};

#endif

