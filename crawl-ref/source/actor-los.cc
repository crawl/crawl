#include "AppHdr.h"

#include "actor.h"
#include "player.h"
#include "monster.h"
#include "state.h"
#include "viewgeom.h"

bool actor::observable() const
{
    return (crawl_state.arena && this != &you
            || this == &you || you.can_see(this));
}

bool actor::see_cell(const coord_def &p) const
{
    return (los.see_cell(p));
}

void actor::update_los()
{
    los.update();
}

bool actor::can_see(const actor *target) const
{
    return (target->visible_to(this) && see_cell(target->pos()));
}

bool player::see_cell_no_trans(const coord_def &p) const
{
    return (los_no_trans.see_cell(p));
}

bool player::trans_wall_blocking(const coord_def &p) const
{
    return (see_cell(p) && !see_cell_no_trans(p));
}

const los_def& actor::get_los() const
{
    return (los);
}

const los_def& actor::get_los_no_trans()
{
    return (los_no_trans);
}

const los_def& monsters::get_los_no_trans()
{
    los_no_trans.update();
    return (los_no_trans);
}

void player::update_los()
{
    if (!crawl_state.arena || !crawl_state.arena_suspended)
    {
        los_no_trans.update();
        actor::update_los();
    }
}

// Player LOS overrides for arena.

bool player::see_cell(const coord_def &c) const
{
    if (crawl_state.arena || crawl_state.arena_suspended)
        return (crawl_view.in_grid_los(c));
    else
        return (actor::see_cell(c));
}

bool player::can_see(const actor* a) const
{
    if (crawl_state.arena || crawl_state.arena_suspended)
        return (see_cell(a->pos()));
    else
        return (actor::can_see(a));
}
