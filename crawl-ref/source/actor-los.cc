#include "AppHdr.h"

#include "actor.h"
#include "player.h"
#include "monster.h"
#include "state.h"

bool actor::observable() const
{
    return (crawl_state.arena || this == &you || you.can_see(this));
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
    los_no_trans.update();
    actor::update_los();
}

