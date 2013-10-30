/**
 * @file
 * @brief Provide a way to iterator over all actors, subject to a few common restrictions.
**/

#ifndef ACT_ITER_H
#define ACT_ITER_H

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

    bool valid(const actor* a) const;
    void advance();
};

#endif
