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
    actor_iterator();
    actor_iterator(const circle_def* circle_);
    actor_iterator(const los_base* los_);
    actor_iterator(const actor* act_);

    operator bool() const;
    actor* operator*() const;
    actor* operator->() const;
    actor_iterator& operator++();
    actor_iterator operator++(int);

protected:
    restr_type restr;
    const circle_def* circle;
    const los_base* los;
    const actor* act;
    bool did_you;
    monster_iterator mi;

    bool valid(const actor* a) const;
    void raw_advance();
    void advance(bool may_stay=false);
};

#endif
