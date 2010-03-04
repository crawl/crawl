#include "AppHdr.h"

#include "actor.h"
#include "coord.h"
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
    return (los.see_cell(p));
}

void actor::update_los()
{
    // Optimization to prevent updating every monster's LOS
    // every turn. Monsters may have incorrect LOS if opacities
    // change without the monster moving (e.g. digging, doors
    // opening/closing, smoke appearing/disappearing).
    if (changed_los_center
        || distance(you.pos(), pos()) <= LOS_MAX_RADIUS_SQ
        || observable())   // arena
    {
        los.update();
        changed_los_center = false;
    }
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
    if (!crawl_state.game_is_arena() || !crawl_state.arena_suspended)
    {
        los_no_trans.update();
        actor::update_los();
    }
}

// Player LOS overrides for arena.

void player::set_arena_los(const coord_def& c)
{
    los.init_arena(c);
}

bool player::can_see(const actor* a) const
{
    if (crawl_state.game_is_arena() || crawl_state.arena_suspended)
        return (see_cell(a->pos()));
    else
        return (actor::can_see(a));
}
