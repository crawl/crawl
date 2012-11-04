#include "AppHdr.h"

#include "act-iter.h"

#include "coord-circle.h"
#include "mon-iter.h"
#include "monster.h"
#include "player.h"

actor_iterator::actor_iterator()
    : restr(R_NONE), did_you(false), mi()
{
    advance(true);
}

actor_iterator::actor_iterator(const circle_def* circle_)
    : restr(R_CIRC), circle(circle_), did_you(false), mi(circle_)
{
    advance(true);
}

actor_iterator::actor_iterator(const los_base* los_)
    : restr(R_LOS), los(los_), did_you(false), mi(los_)
{
    advance(true);
}

actor_iterator::actor_iterator(const actor* act_)
    : restr(R_ACT), act(act_), did_you(false), mi(act_)
{
    advance(true);
}

actor_iterator::operator bool() const
{
    return (!did_you || mi);
}

actor* actor_iterator::operator*() const
{
    if (!did_you)
        return &you;
    else
        return *mi;
}

actor* actor_iterator::operator->() const
{
    return **this;
}

actor_iterator& actor_iterator::operator++()
{
    advance();
    return *this;
}

actor_iterator actor_iterator::operator++(int)
{
    actor_iterator copy = *this;
    ++(*this);
    return copy;
}

bool actor_iterator::valid(const actor* a) const
{
    if (!a->alive())
        return false;
    switch (restr)
    {
    case R_CIRC:
        return circle->contains(a->pos());
    case R_LOS:
        return los->see_cell(a->pos());
    case R_ACT:
        return act->can_see(a);
    default:
        return true;
    }
}

void actor_iterator::raw_advance()
{
    if (!did_you)
        did_you = true;
    else
        ++mi;
}

void actor_iterator::advance(bool may_stay)
{
    if (!may_stay)
        raw_advance();
    while (*this && !valid(**this))
        raw_advance();
}
