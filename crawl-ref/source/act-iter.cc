#include "AppHdr.h"

#include "act-iter.h"

#include "env.h"
#include "losglobal.h"

actor_near_iterator::actor_near_iterator(coord_def c, los_type los)
    : center(c), _los(los), viewer(nullptr), i(-1), max(env.max_mon_index)
{
    if (!valid(&you))
        advance();
}

actor_near_iterator::actor_near_iterator(const actor* a, los_type los)
    : center(a->pos()), _los(los), viewer(a), i(-1), max(env.max_mon_index)
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
    else if (i <= max)
        return &env.mons[i];
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
    if (viewer && !a->visible_to(viewer))
        return false;
    return cell_see_cell(center, a->pos(), _los);
}

void actor_near_iterator::advance()
{
    do
         if (++i > max)
             return;
    while (!valid(**this));
}

//////////////////////////////////////////////////////////////////////////

monster_near_iterator::monster_near_iterator(coord_def c, los_type los)
    : center(c), _los(los), viewer(nullptr), i(0), max(env.max_mon_index)
{
    if (!valid(&env.mons[0]))
        advance();
    begin_point = i;
}

monster_near_iterator::monster_near_iterator(const actor *a, los_type los)
    : center(a->pos()), _los(los), viewer(a), i(0), max(env.max_mon_index)
{
    if (!valid(&env.mons[0]))
        advance();
    begin_point = i;
}

monster_near_iterator::operator bool() const
{
    return valid(**this);
}

monster* monster_near_iterator::operator*() const
{
    if (i <= max)
        return &env.mons[i];
    else
        return nullptr;
}

monster* monster_near_iterator::operator->() const
{
    return **this;
}

monster_near_iterator& monster_near_iterator::operator++()
{
    advance();
    return *this;
}

bool monster_near_iterator::operator==(const monster_near_iterator &other)
{
    return i == other.i;
}

bool monster_near_iterator::operator!=(const monster_near_iterator &other)
{
    return !(operator==(other));
}

monster_near_iterator monster_near_iterator::operator++(int)
{
    monster_near_iterator copy = *this;
    ++(*this);
    return copy;
}

monster_near_iterator monster_near_iterator::begin()
{
    monster_near_iterator copy = *this;
    copy.i = begin_point;
    return copy;
}

monster_near_iterator monster_near_iterator::end()
{
    monster_near_iterator copy = *this;
    copy.i = max + 1;
    return copy;
}

bool monster_near_iterator::valid(const monster* a) const
{
    if (!a || !a->alive())
        return false;
    if (viewer && !a->visible_to(viewer))
        return false;
    return cell_see_cell(center, a->pos(), _los);
}

void monster_near_iterator::advance()
{
    do
         if (++i > max)
             return;
    while (!valid(**this));
}

//////////////////////////////////////////////////////////////////////////

monster_iterator::monster_iterator()
    : i(0), max(env.max_mon_index)
{
    while (i <= max && !env.mons[i].alive())
        i++;
}

monster_iterator::operator bool() const
{
    return i <= max && (*this)->alive();
}

monster* monster_iterator::operator*() const
{
    if (i < MAX_MONSTERS)
        return &env.mons[i];
    else
        return nullptr;
}

monster* monster_iterator::operator->() const
{
    return **this;
}

monster_iterator& monster_iterator::operator++()
{
    while (++i <= max)
        if (env.mons[i].alive())
            break;
    return *this;
}

monster_iterator monster_iterator::operator++(int)
{
    monster_iterator copy = *this;
    ++(*this);
    return copy;
}

void monster_iterator::advance()
{
    do
         if (++i > max)
             return;
    while (!(*this)->alive());
}

bool far_to_near_sorter::operator()(const actor* a, const actor* b)
{
    return a->pos().distance_from(pos) > b->pos().distance_from(pos);
}

bool near_to_far_sorter::operator()(const actor* a, const actor* b)
{
    return a->pos().distance_from(pos) < b->pos().distance_from(pos);
}
