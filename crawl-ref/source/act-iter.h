/**
 * @file
 * @brief Provide a way to iterator over all actors, subject to a few common restrictions.
**/

#ifndef ACT_ITER_H
#define ACT_ITER_H

#include "mon-iter.h"

class actor_iterator
{
public:
    actor_iterator(const los_base* los_);

    operator bool() const;
    actor* operator*() const;
    actor* operator->() const;
    actor_iterator& operator++();
    actor_iterator operator++(int);

protected:
    const los_base* los;
    bool did_you;
    monster_iterator mi;

    bool valid(const actor* a) const;
    void raw_advance();
    void advance(bool may_stay=false);
};

#endif
