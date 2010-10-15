/*
 * File:     coordit.h
 * Summary:  Coordinate iterators.
 */

#include "AppHdr.h"

#include "coordit.h"

#include "coord-circle.h"
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
