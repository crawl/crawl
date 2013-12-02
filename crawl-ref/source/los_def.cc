/**
 * @file
 * @brief LOS wrapper class.
**/

#include "AppHdr.h"

#include "los_def.h"
#include "losglobal.h"

#include "coord-circle.h"

los_def::los_def()
    : show(0), opc(opc_default.clone()), bds(BDS_DEFAULT)
{
}

los_def::los_def(const coord_def& c, const opacity_func &o,
                                     const circle_def &b)
    : show(0), center(c), opc(o.clone()), bds(b)
{
}

los_def::los_def(const los_def& los)
    : show(los.show), center(los.center),
      opc(los.opc->clone()), bds(los.bds)
{
}

los_def& los_def::operator=(const los_def& los)
{
    if (this != &los)
    {
        init(los.center, *los.opc, los.bds);
        show = los.show;
    }
    return *this;
}

void los_def::init(const coord_def &c, const opacity_func &o,
                   const circle_def &b)
{
    show.init(0);
    set_center(c);
    set_opacity(o);
    set_bounds(b);
}

los_def::~los_def()
{
    delete opc;
}

void los_def::update()
{
    losight(show, center, *opc, bds);
}

void los_def::set_center(const coord_def& c)
{
    center = c;
}

coord_def los_def::get_center() const
{
    return center;
}

void los_def::set_opacity(const opacity_func &o)
{
    delete opc;
    opc = o.clone();
}

void los_def::set_bounds(const circle_def &b)
{
    bds = b;
}

circle_def los_def::get_bounds() const
{
    return circle_def(center, bds);
}

bool los_def::in_bounds(const coord_def& p) const
{
    return bds.contains(p - center);
}

bool los_def::see_cell(const coord_def& p) const
{
    const coord_def sp = p - center;
    return sp.rdist() <= LOS_MAX_RANGE && show(sp);
}
