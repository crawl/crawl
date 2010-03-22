#include "AppHdr.h"

#include "actor.h"
#include "coord.h"
#include "losglobal.h"
#include "player.h"
#include "monster.h"
#include "state.h"

bool actor::observable() const
{
    return (crawl_state.game_is_arena() && this != &you
            || this == &you || you.can_see(this));
}

bool actor::see_cell(const coord_def &p) const
{
    return (cell_see_cell(pos(), p, LOS_DEFAULT));
}

bool actor::can_see(const actor *target) const
{
    return (target->visible_to(this) && see_cell(target->pos()));
}

bool actor::see_cell_no_trans(const coord_def &p) const
{
    return (cell_see_cell(pos(), p, LOS_NO_TRANS));
}

bool player::trans_wall_blocking(const coord_def &p) const
{
    return (see_cell(p) && !see_cell_no_trans(p));
}

const los_base* actor::get_los()
{
    los = los_glob(pos(), LOS_DEFAULT);
    return (&los);
}

const los_base* actor::get_los_no_trans()
{
    los_no_trans = los_glob(pos(), LOS_NO_TRANS);
    return (&los_no_trans);
}

// Player LOS overrides for arena.

void player::set_arena_los(const coord_def& c)
{
    // XXX: update
}

bool player::can_see(const actor* a) const
{
    if (crawl_state.game_is_arena() || crawl_state.arena_suspended)
        return (see_cell(a->pos()));
    else
        return (actor::can_see(a));
}
