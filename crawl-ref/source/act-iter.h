/**
 * @file
 * @brief Provide a way to iterator over all actors, subject to a few common restrictions.
**/

#pragma once

#include "los-type.h"

class actor_near_iterator
{
public:
    actor_near_iterator(coord_def c, los_type los = LOS_DEFAULT);
    actor_near_iterator(const actor* a, los_type los = LOS_DEFAULT);

    operator bool() const;
    actor* operator*() const;
    actor* operator->() const;
    actor_near_iterator& operator++();
    actor_near_iterator operator++(int);

protected:
    const coord_def center;
    los_type _los;
    const actor* viewer;
    int i;
    const int max;

    bool valid(const actor* a) const;
    void advance();
};

class monster_near_iterator
{
public:
    monster_near_iterator(coord_def c, los_type los = LOS_DEFAULT);
    monster_near_iterator(const actor* a, los_type los = LOS_DEFAULT);

    operator bool() const;
    monster* operator*() const;
    monster* operator->() const;
    monster_near_iterator& operator++();
    monster_near_iterator operator++(int);
    bool operator==(const monster_near_iterator &other);
    bool operator!=(const monster_near_iterator &other);
    monster_near_iterator begin();
    monster_near_iterator end();

protected:
    const coord_def center;
    los_type _los;
    const actor* viewer;
    int i;
    const int max;
    int begin_point;

    bool valid(const monster* a) const;
    void advance();
};

class monster_iterator
{
public:
    monster_iterator();

    operator bool() const;
    monster* operator*() const;
    monster* operator->() const;
    monster_iterator& operator++();
    monster_iterator operator++(int);

protected:
    int i;
    const int max;
    void advance();
};

// Actor sorters for combination with the above
// Compare two actors, sorting farthest to nearest from {pos}
struct far_to_near_sorter
{
    coord_def pos;
    bool operator()(const actor* a, const actor* b);
};

// Compare two actors, sorting nearest to farthest from {pos}
struct near_to_far_sorter
{
    coord_def pos;
    bool operator()(const actor* a, const actor* b);
};
