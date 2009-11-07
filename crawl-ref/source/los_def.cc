/*
 * File:     los_def.cc
 * Summary:  LOS wrapper class.
 */

#include "AppHdr.h"

#include "los.h"

los_def::los_def()
    : show(0), opc(opc_default.clone()), bds(bds_default.clone())
{
}

los_def::los_def(const coord_def& c, const opacity_func &o,
                                     const bounds_func &b)
    : show(0), center(c), opc(o.clone()), bds(b.clone())
{
}

los_def::los_def(const los_def& los)
    : show(los.show), center(los.center),
      opc(los.opc->clone()), bds(los.bds->clone())
{
}

los_def& los_def::operator=(const los_def& los)
{
    init(los.center, *los.opc, *los.bds);
    show = los.show;
    return (*this);
}

void los_def::init(const coord_def &c, const opacity_func &o,
                   const bounds_func &b)
{
    show.init(0);
    set_center(c);
    set_opacity(o);
    set_bounds(b);
}

los_def::~los_def()
{
    delete opc;
    delete bds;
}

void los_def::update()
{
    losight(show, center, *opc, *bds);
}

void los_def::set_center(const coord_def& c)
{
    center = c;
}

void los_def::set_opacity(const opacity_func &o)
{
    delete opc;
    opc = o.clone();
}

void los_def::set_bounds(const bounds_func &b)
{
    delete bds;
    bds = b.clone();
}

bool los_def::in_bounds(const coord_def& p) const
{
    return ((*bds)(p));
}

bool los_def::see_cell(const coord_def& p) const
{
    return (::see_cell(show, center, p));
}
