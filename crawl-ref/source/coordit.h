#ifndef COORDIT_H
#define COORDIT_H

#include "coord-circle.h"

class rectangle_iterator :
    public std::iterator<std::forward_iterator_tag, coord_def>
{
public:
    rectangle_iterator(const coord_def& corner1, const coord_def& corner2);
    explicit rectangle_iterator(int x_border_dist, int y_border_dist = -1);
    operator bool() const;
    coord_def operator *() const;
    const coord_def* operator->() const;

    rectangle_iterator& operator ++ ();
    rectangle_iterator operator ++ (int);
private:
    coord_def current, topleft, bottomright;
};

class circle_iterator
{
    const circle_def &circle;
    rectangle_iterator iter;

public:
    circle_iterator(const circle_def &circle_);

    operator bool() const;
    coord_def operator*() const;

    void operator++();
    void operator++(int);
};

class los_def;
class radius_iterator : public std::iterator<std::forward_iterator_tag,
                        coord_def>
{
public:
    radius_iterator(const coord_def& center, int param,
                    circle_type ctype,
                    const los_def* los = NULL,
                    bool exclude_center = false);
    radius_iterator(const coord_def& center, int radius,
                    bool roguelike_metric = true,
                    bool require_los = true,
                    bool exclude_center = false,
                    const los_def* los = NULL);
    radius_iterator(const los_def* los,
                    bool exclude_center = false);
    operator bool() const;
    coord_def operator *() const;
    const coord_def* operator->() const;

    const radius_iterator& operator ++ ();
    radius_iterator operator ++ (int);

private:
    void advance(bool may_stay);
    bool is_valid_square(const coord_def& p) const;

    circle_def circle;
    circle_iterator iter;
    bool exclude_center;
    const los_def* los;
    coord_def current;
};

class adjacent_iterator : public radius_iterator
{
public:
    explicit adjacent_iterator( const coord_def& pos,
                                bool _exclude_center = true ) :
        radius_iterator(pos, 1, true, false, _exclude_center) {}
};

#endif
