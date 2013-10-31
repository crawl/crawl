/**
 * @file
 * Provide a way to iterator over all monsters,
 * subject to a few common restrictions.
**/
// TODO: Iterate over actors?

#ifndef MON_ITER_H
#define MON_ITER_H

enum restr_type
{
    R_NONE,
    R_CIRC,
    R_LOS,
    R_ACT,
};

class circle_def;
class los_base;
class actor;

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
    restr_type restr;
    int curr_mid;
    const circle_def* circle;
    const los_base* los;
    const actor* act;

    bool valid(int mid) const;
    void advance(bool may_stay=false);
};

#endif
