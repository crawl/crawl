/*
 * File:     coordit.h
 * Summary:  Coordinate iterators.
 */

#include "AppHdr.h"

#include "coordit.h"

#include "coord-circle.h"
#include "coord.h"
#include "random.h"
#include "player.h"

rectangle_iterator::rectangle_iterator(const coord_def& corner1,
                                        const coord_def& corner2)
{
    topleft.x = std::min(corner1.x, corner2.x);
    topleft.y = std::min(corner1.y, corner2.y); // not really necessary
    bottomright.x = std::max(corner1.x, corner2.x);
    bottomright.y = std::max(corner1.y, corner2.y);
    current = topleft;
}

rectangle_iterator::rectangle_iterator(int x_border_dist, int y_border_dist)
{
    if (y_border_dist < 0)
        y_border_dist = x_border_dist;

    topleft.set(x_border_dist, y_border_dist);
    bottomright.set(GXM - x_border_dist - 1, GYM - y_border_dist - 1);
    current = topleft;
}

rectangle_iterator::operator bool() const
{
    return (current.y <= bottomright.y);
}

coord_def rectangle_iterator::operator *() const
{
    return current;
}

const coord_def* rectangle_iterator::operator->() const
{
    return &current;
}

rectangle_iterator& rectangle_iterator::operator ++()
{
    if (current.x == bottomright.x)
    {
        current.x = topleft.x;
        current.y++;
    }
    else
    {
        current.x++;
    }
    return *this;
}


rectangle_iterator rectangle_iterator::operator++(int dummy)
{
    const rectangle_iterator copy = *this;
    ++(*this);
    return (copy);
}

/*
 *  circle iterator
 */

circle_iterator::circle_iterator(const circle_def &circle_)
    : circle(circle_), iter(circle_.get_bbox().iter())
{
    while (iter && !circle.contains(*iter))
        ++iter;
}

circle_iterator::operator bool() const
{
    return (iter);
}

coord_def circle_iterator::operator*() const
{
    return (*iter);
}

void circle_iterator::operator++()
{
    do
        ++iter;
    while (iter && !circle.contains(*iter));
}

void circle_iterator::operator++(int)
{
    ++(*this);
}


radius_iterator::radius_iterator(const coord_def& center, int param,
                                 circle_type ctype,
                                 const los_base* _los,
                                 bool _exclude_center)
    : circle(center, param, ctype),
      iter(circle.iter()),
      exclude_center(_exclude_center),
      los(_los)
{
    advance(true);
}

radius_iterator::radius_iterator(const coord_def& _center, int _radius,
                                 bool roguelike, bool _require_los,
                                 bool _exclude_center,
                                 const los_base* _los)
    : circle(_center, _radius, roguelike ? C_SQUARE : C_POINTY),
      iter(circle.iter()),
      exclude_center(_exclude_center),
      los(_require_los ? (_los ? _los : you.get_los()) : NULL)
{
    advance(true);
}

radius_iterator::radius_iterator(const los_base* los_,
                                 bool _exclude_center)
    : circle(los_->get_bounds()),
      iter(circle.iter()),
      exclude_center(_exclude_center),
      los(los_)
{
    advance(true);
}

void radius_iterator::advance(bool may_stay)
{
    if (!may_stay)
        ++iter;
    while (iter && !is_valid_square(*iter))
        ++iter;
    current = *iter;
}

radius_iterator::operator bool() const
{
    return (iter);
}

coord_def radius_iterator::operator *() const
{
    return (current);
}

const coord_def* radius_iterator::operator->() const
{
    return &current;
}

bool radius_iterator::is_valid_square(const coord_def &p) const
{
    if (exclude_center && p == circle.get_center())
        return (false);
    if (los && !los->see_cell(p))
        return (false);
    return (true);
}

const radius_iterator& radius_iterator::operator++()
{
    advance(false);
    return (*this);
}

radius_iterator radius_iterator::operator++(int dummy)
{
    const radius_iterator copy = *this;
    ++(*this);
    return (copy);
}

/*
 *  spiral iterator
 */
spiral_iterator::spiral_iterator(const coord_def& _center, bool _fair,
                                 bool exclude_center, int _max_radius) :
    center(_center), current(_center), radius(1), max_radius(_max_radius),
    threshold(0), icur(0), iend(0), fair(_fair)
{
    vcur  = lists + 0;
    vnear = lists + 1;
    vfar  = lists + 2;

    for (int dx = -1; dx <= 1; dx++)
        for (int dy = -1; dy <= 1; dy++)
            if (dx || dy)
                vnear->push_back(coord_def(dx, dy));

    if (exclude_center)
        advance();
}

static inline int sgn(int x)
{
    return (x < 0) ? -1 : (x > 0) ? 1 : 0;
}

bool spiral_iterator::advance()
{
again:
    if (++icur >= vcur->size())
        icur = 0;
    if (icur == iend)
    {
        // Advance to the next radius.
        std::vector<coord_def> *tmp = vcur;
        vcur = vnear;
        vnear = vfar;
        vfar = tmp;
        tmp->clear();

        if (!vcur->size())
            return false;
        // Randomize the order various directions are returned.
        // Just the initial angle is enough.
        if (fair)
            icur = iend = random2(vcur->size() + 1);
        else
            icur = iend = 0; // randomness is costly

        if (radius++ >= max_radius)
        {
            vcur->clear();
            return false;
        }
        threshold = radius * radius + 1;
    }

    coord_def d = (*vcur)[icur];
    if (!in_bounds(current = center + d))
        goto again;

    ASSERT(d.x || d.y);
    if (!d.y)
        push_neigh(d, sgn(d.x), 0);
    if (!d.x)
        push_neigh(d, 0, sgn(d.y));
    if (d.x <= 0)
    {
        if (d.y <= 0)
            push_neigh(d, -1, -1);
        if (d.y >= 0)
            push_neigh(d, -1, +1);
    }
    if (d.x >= 0)
    {
        if (d.y <= 0)
            push_neigh(d, +1, -1);
        if (d.y >= 0)
            push_neigh(d, +1, +1);
    }

    return true;
}

void spiral_iterator::push_neigh(coord_def d, int dx, int dy)
{
    d.x += dx;
    d.y += dy;
    ((d.abs() <= threshold) ? vnear : vfar)->push_back(d);
}

spiral_iterator::operator bool() const
{
    return in_bounds(current);
}

coord_def spiral_iterator::operator *() const
{
    return current;
}

const coord_def* spiral_iterator::operator->() const
{
    return &current;
}

const spiral_iterator& spiral_iterator::operator++()
{
    advance();
    return *this;
}

spiral_iterator spiral_iterator::operator++(int dummy)
{
    const spiral_iterator copy = *this;
    ++(*this);
    return (copy);
}
