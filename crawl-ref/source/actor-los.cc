#include "AppHdr.h"

#include "actor.h"
#include "coord.h"
#include "losglobal.h"
#include "player.h"
#include "monster.h"
#include "state.h"
#include "viewgeom.h"

bool actor::observable() const
{
    return (crawl_state.game_is_arena() && this != &you
            || this == &you || you.can_see(this));
}

bool actor::see_cell(const coord_def &p) const
{
    los_type lt = LOS_DEFAULT;

    if (crawl_state.game_is_arena() && this == &you)
        lt = LOS_ARENA; // observer can see everything
    else if (!in_bounds(pos()))
        return (false); // actor is off the map

    return (cell_see_cell(pos(), p, lt));
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
    if (crawl_state.game_is_arena())
    {
        // env.show.init iterates over these bounds for arena
        los = los_glob(crawl_view.vgrdc, LOS_ARENA,
                       circle_def(LOS_MAX_RANGE, C_SQUARE));
    }
    else
        los = los_glob(pos(), LOS_DEFAULT);
    return (&los);
}

const los_base* actor::get_los_no_trans()
{
    los_no_trans = los_glob(pos(), LOS_NO_TRANS);
    return (&los_no_trans);
}

// Player LOS overrides for arena.
bool player::can_see(const actor* a) const
{
    if (crawl_state.game_is_arena() || crawl_state.arena_suspended)
        return (see_cell(a->pos()));
    else
        return (actor::can_see(a));
}
