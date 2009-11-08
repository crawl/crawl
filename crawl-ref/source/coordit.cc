/*
 * File:     coordit.h
 * Summary:  Coordinate iterators.
 */

#include "AppHdr.h"

#include "coordit.h"

#include "coord.h"
#include "los.h"
#include "player.h"

rectangle_iterator::rectangle_iterator( const coord_def& corner1,
                                        const coord_def& corner2 )
{
    topleft.x = std::min(corner1.x, corner2.x);
    topleft.y = std::min(corner1.y, corner2.y); // not really necessary
    bottomright.x = std::max(corner1.x, corner2.x);
    bottomright.y = std::max(corner1.y, corner2.y);
    current = topleft;
}

rectangle_iterator::rectangle_iterator( int x_border_dist, int y_border_dist )
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

rectangle_iterator rectangle_iterator::operator++( int dummy )
{
    const rectangle_iterator copy = *this;
    ++(*this);
    return (copy);
}

radius_iterator::radius_iterator(const coord_def& _center, int _radius,
                                 bool _roguelike_metric, bool _require_los,
                                 bool _exclude_center,
                                 const env_show_grid* _losgrid)
    : center(_center), radius(_radius), roguelike_metric(_roguelike_metric),
      require_los(_require_los), exclude_center(_exclude_center),
      losgrid(_losgrid), iter_done(false)
{
    reset();
}

void radius_iterator::reset()
{
    iter_done = false;

    location.x = center.x - radius;
    location.y = center.y - radius;

    if ( !this->on_valid_square() )
        ++(*this);
}

bool radius_iterator::done() const
{
    return iter_done;
}

coord_def radius_iterator::operator *() const
{
    return location;
}

const coord_def* radius_iterator::operator->() const
{
    return &location;
}

void radius_iterator::step()
{
    const int minx = std::max(X_BOUND_1+1, center.x - radius);
    const int maxx = std::min(X_BOUND_2-1, center.x + radius);
    const int maxy = std::min(Y_BOUND_2-1, center.y + radius);

    // Sweep L-R, U-D
    location.x++;
    if (location.x > maxx)
    {
        location.x = minx;
        location.y++;
        if (location.y > maxy)
            iter_done = true;
    }
}

bool radius_iterator::on_valid_square() const
{
    if (!in_bounds(location))
        return (false);
    if (!roguelike_metric && (location - center).abs() > radius*radius)
        return (false);
    if (require_los)
    {
        if (!losgrid && !you.see_cell(location))
            return (false);
        if (losgrid && !see_cell(*losgrid, center, location))
            return (false);
    }
    if (exclude_center && location == center)
        return (false);

    return (true);
}

const radius_iterator& radius_iterator::operator++()
{
    do
        this->step();
    while (!this->done() && !this->on_valid_square());

    return (*this);
}

radius_iterator radius_iterator::operator++(int dummy)
{
    const radius_iterator copy = *this;
    ++(*this);
    return (copy);
}
