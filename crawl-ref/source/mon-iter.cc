#include "AppHdr.h"

#include "mon-iter.h"

#include "actor.h"
#include "coord-circle.h"
#include "env.h"
#include "monster.h"

monster_iterator::monster_iterator()
    : restr(R_NONE), curr_mid(0)
{
    advance(true);
}

monster_iterator::operator bool() const
{
    return (curr_mid < MAX_MONSTERS);
}

monster* monster_iterator::operator*() const
{
    return (&env.mons[curr_mid]);
}

monster* monster_iterator::operator->() const
{
    return (&env.mons[curr_mid]);
}

monster_iterator& monster_iterator::operator++()
{
    advance();
    return *this;
}

monster_iterator monster_iterator::operator++(int)
{
    monster_iterator copy = *this;
    ++(*this);
    return copy;
}

bool monster_iterator::valid(int mid) const
{
    monster* mon = &env.mons[mid];
    if (!mon->alive())
        return false;
    switch (restr)
    {
    case R_CIRC:
        return (circle->contains(mon->pos()));
    case R_LOS:
        return (los->see_cell(mon->pos()));
    case R_ACT:
        return act->can_see(mon);
    default:
        return true;
    }
}

void monster_iterator::advance(bool may_stay)
{
    if (!may_stay)
        ++curr_mid;
    while (curr_mid < MAX_MONSTERS && !valid(curr_mid))
        ++curr_mid;
}
