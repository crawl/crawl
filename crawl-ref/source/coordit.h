#ifndef COORDIT_H
#define COORDIT_H

#include "coord-circle.h"

class rectangle_iterator : public iterator<forward_iterator_tag, coord_def>
{
public:
    rectangle_iterator(const coord_def& corner1, const coord_def& corner2);
    rectangle_iterator(const coord_def& center, int halfside);
    explicit rectangle_iterator(int x_border_dist, int y_border_dist = -1);
    operator bool() const PURE;
    coord_def operator *() const PURE;
    const coord_def* operator->() const PURE;

    void operator ++ ();
    void operator ++ (int);
private:
    coord_def current, topleft, bottomright;
};

/**
 * @class random_rectangle_iterator
 * Iterator over coordinates in a rectangular region in a
 * random order.  This interator does not favour any given
 * direction, but is slower than rectangle_iterator.
 *
 * When this iterator has returned all elements, it will just
 * return the top left corner forever.
 */
class random_rectangle_iterator : public iterator<forward_iterator_tag,
                                                  coord_def>
{
public:
    random_rectangle_iterator(const coord_def& corner1,
                              const coord_def& corner2);
    explicit random_rectangle_iterator(int x_border_dist,
                                       int y_border_dist = -1);
    operator bool() const PURE;
    coord_def operator *() const PURE;
    const coord_def* operator->() const PURE;

    void operator ++ ();
    void operator ++ (int);
private:
    coord_def top_left;
    vector<coord_def> remaining;
    int current;
};

class circle_iterator
{
    const circle_def &circle;
    rectangle_iterator iter;

public:
    circle_iterator(const circle_def &circle_);

    operator bool() const PURE;
    coord_def operator *() const PURE;

    void operator ++ ();
    void operator ++ (int);
};

/**
 * @class radius_iterator
 * Iterator over coordinates in a more-or-less circular region.
 *
 * The region can be any circle_def; furthermore, the cells can
 * be restricted to lie within some LOS field (need not be
 * centered at the same point), and to exclude the center.
 */
class los_base;
class radius_iterator : public iterator<forward_iterator_tag, coord_def>
{
public:
    // General constructor.
    radius_iterator(const coord_def& center, int param,
                    circle_type ctype,
                    const los_base* los = NULL,
                    bool exclude_center = false);
    // Legacy constructor -- use above instead.
    radius_iterator(const coord_def& center, int radius,
                    bool roguelike_metric = true,
                    bool require_los = true,
                    bool exclude_center = false);
    // Just iterate over a LOS field.
    radius_iterator(const los_base* los,
                    bool exclude_center = false);

    radius_iterator(const coord_def center, int param, circle_type ctype,
                    los_type los, bool exclude_center = false);
    radius_iterator(const coord_def center, los_type los,
                    bool exclude_center = false);

    operator bool() const PURE;
    coord_def operator *() const PURE;
    const coord_def *operator->() const PURE;

    void operator ++ ();
    void operator ++ (int);

private:
    void advance(bool may_stay);
    bool is_valid_square(const coord_def& p) const;

    circle_def circle;
    circle_iterator iter;
    bool exclude_center;
    const los_base* los;  // restrict to the los if not NULL
    unique_ptr<los_base> used_los;
    coord_def current;    // storage for operator->
};

class adjacent_iterator : public radius_iterator
{
public:
    explicit adjacent_iterator(const coord_def& pos,
                               bool _exclude_center = true) :
    radius_iterator(pos, 1, C_SQUARE, NULL, _exclude_center) {}
};

class orth_adjacent_iterator : public radius_iterator
{
public:
    explicit orth_adjacent_iterator(const coord_def& pos,
                                    bool _exclude_center = true) :
    radius_iterator(pos, 1, C_POINTY, NULL, _exclude_center) {}
};

/* @class distance_iterator
 * Iterates over coordinates in integer ranges.
 *
 * Unlike other iterators, it tries hard to not favorize any
 * particular direction (unless fair = false, when it saves some CPU).
 */
class distance_iterator : public iterator<forward_iterator_tag, coord_def>
{
public:
    distance_iterator(const coord_def& _center,
                    bool _fair = true,
                    bool exclude_center = true,
                    int _max_radius = GDM);
    operator bool() const PURE;
    coord_def operator *() const PURE;
    const coord_def *operator->() const PURE;

    const distance_iterator &operator ++();
    void operator ++(int);
    int radius() const;
private:
    coord_def center, current;
    vector<coord_def> lists[3], *vcur, *vnear, *vfar;
    int r, max_radius;
    int threshold;
    unsigned int icur, iend;
    bool fair;
    bool advance();
    void push_neigh(coord_def from, int dx, int dy);
};

#endif
