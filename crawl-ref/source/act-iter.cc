#include "AppHdr.h"

#include "act-iter.h"

#include "coord-circle.h"
#include "mon-iter.h"
#include "monster.h"
#include "player.h"

actor_iterator::actor_iterator(const los_base* los_)
    : los(los_), did_you(false), mi(los_)
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
    return los->see_cell(a->pos());
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
