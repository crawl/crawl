#ifndef COORDIT_H
#define COORDIT_H

#include "coord-circle.h"
#include "los-type.h"

class rectangle_iterator : public iterator<forward_iterator_tag, coord_def>
{
public:
    rectangle_iterator(const coord_def& corner1, const coord_def& corner2);
    rectangle_iterator(const coord_def& center, int halfside,
                       bool clip_to_map = false);
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
 * random order. This interator does not favour any given
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

/**
 * @class radius_iterator
 * Iterator over coordinates in a circular region.
 *
 * The region can be a circle of any r²; furthermore, the cells can
 * be restricted to lie within LOS from the center (of any type)
 * centered at the same point), and to exclude the center.
 */
class radius_iterator : public iterator<forward_iterator_tag, coord_def>
{
public:
    radius_iterator(const coord_def center, int radius, circle_type ctype,
                    bool exclude_center = false);
    radius_iterator(const coord_def center, int radius, circle_type ctype,
                    los_type los, bool exclude_center = false);
    radius_iterator(const coord_def center, los_type los,
                    bool exclude_center = false);

    operator bool() const PURE;
    coord_def operator *() const PURE;
    const coord_def *operator->() const PURE;

    void operator ++ ();
    void operator ++ (int);

private:
    enum costate { RI_DONE, RI_START, RI_SE, RI_NE, RI_SW, RI_NW };
    int x, y, cost_x, cost_y, credit, credit_x, credit_y, base_cost, inc_cost;
    bool is_square;

    costate state;
    coord_def center;
    los_type los;
    coord_def current;    // storage for operator->
};

class adjacent_iterator : public iterator<forward_iterator_tag, coord_def>
{
public:
    adjacent_iterator(const coord_def pos, bool _exclude_center = true)
        : center(pos), i(_exclude_center ? 8 : 9) {++(*this);};

    operator bool() const PURE;
    coord_def operator *() const PURE;
    const coord_def *operator->() const PURE;

    void operator ++ ();
    void operator ++ (int);
private:
    coord_def center;
    coord_def val;
    int i;
};

class orth_adjacent_iterator : public radius_iterator
{
public:
    explicit orth_adjacent_iterator(const coord_def& pos,
                                    bool _exclude_center = true) :
    radius_iterator(pos, 1, C_POINTY, _exclude_center) {}
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

// If this becomes performance-critical, reimplement it as adjacent_iterator
// with a permutation array.
class fair_adjacent_iterator : public distance_iterator
{
public:
    fair_adjacent_iterator(coord_def _center, bool _exclude_center = true)
        : distance_iterator(_center, true, _exclude_center, 1) {}
};

# ifdef DEBUG_TESTS
void coordit_tests();
# endif
#endif
