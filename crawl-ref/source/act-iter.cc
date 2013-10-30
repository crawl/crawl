#include "AppHdr.h"

#include "act-iter.h"

#include "env.h"
#include "losglobal.h"

actor_near_iterator::actor_near_iterator(coord_def c, los_type los)
    : center(c), _los(los), viewer(nullptr), i(-1)
{
    if (!valid(&you))
        advance();
}

actor_near_iterator::actor_near_iterator(const actor* a, los_type los)
    : center(a->pos()), _los(los), viewer(a), i(-1)
{
    if (!valid(&you))
        advance();
}

actor_near_iterator::operator bool() const
{
    return valid(**this);
}

actor* actor_near_iterator::operator*() const
{
    if (i == -1)
        return &you;
    else if (i < MAX_MONSTERS)
        return &menv[i];
    else
        return nullptr;
}

actor* actor_near_iterator::operator->() const
{
    return **this;
}

actor_near_iterator& actor_near_iterator::operator++()
{
    advance();
    return *this;
}

actor_near_iterator actor_near_iterator::operator++(int)
{
    actor_near_iterator copy = *this;
    ++(*this);
    return copy;
}

bool actor_near_iterator::valid(const actor* a) const
{
    if (!a || !a->alive())
        return false;
    if (viewer && !a->visible_to(a))
        return false;
    return cell_see_cell(center, a->pos(), _los);
}

void actor_near_iterator::advance()
{
    do
         if (++i >= MAX_MONSTERS)
             return;
    while (!valid(**this));
}
