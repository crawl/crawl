#ifndef COORDIT_H
#define COORDIT_H

#include "player.h"

class rectangle_iterator :
    public std::iterator<std::forward_iterator_tag, coord_def>
{
public:
    rectangle_iterator( const coord_def& corner1, const coord_def& corner2 );
    explicit rectangle_iterator( int x_border_dist, int y_border_dist = -1 );
    operator bool() const;
    coord_def operator *() const;
    const coord_def* operator->() const;

    rectangle_iterator& operator ++ ();
    rectangle_iterator operator ++ (int);
private:
    coord_def current, topleft, bottomright;
};

class radius_iterator : public std::iterator<std::bidirectional_iterator_tag,
                        coord_def>
{
public:
    radius_iterator( const coord_def& center, int radius,
                     bool roguelike_metric = true,
                     bool require_los = true,
                     bool exclude_center = false,
                     const env_show_grid* losgrid = NULL );
    bool done() const;
    void reset();
    operator bool() const { return !done(); }
    coord_def operator *() const;
    const coord_def* operator->() const;

    const radius_iterator& operator ++ ();
    const radius_iterator& operator -- ();
    radius_iterator operator ++ (int);
    radius_iterator operator -- (int);
private:
    void step();
    void step_back();
    bool on_valid_square() const;

    coord_def location, center;
    int radius;
    bool roguelike_metric, require_los, exclude_center;
    const env_show_grid* losgrid;
    bool iter_done;
};

class adjacent_iterator : public radius_iterator
{
public:
    explicit adjacent_iterator( const coord_def& pos = you.pos(),
                                bool _exclude_center = true ) :
        radius_iterator(pos, 1, true, false, _exclude_center) {}
};

#endif

