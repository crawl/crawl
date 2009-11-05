#include "AppHdr.h"

#include "actor.h"
#include "env.h"
#include "player.h"
#include "state.h"
#include "traps.h"

actor::~actor()
{
}

bool actor::observable() const
{
    return (crawl_state.arena || this == &you || you.can_see(this));
}

bool actor::has_equipped(equipment_type eq, int sub_type) const
{
    const item_def *item = slot_item(eq);
    return (item && item->sub_type == sub_type);
}

bool actor::will_trigger_shaft() const
{
    return (!airborne() && total_weight() > 0 && is_valid_shaft_level());
}

level_id actor::shaft_dest() const
{
    return generic_shaft_dest(pos());
}

bool actor::airborne() const
{
    return (is_levitating() || (flight_mode() == FL_FLY && !cannot_move()));
}

bool actor::can_wield(const item_def* item, bool ignore_curse,
                      bool ignore_brand, bool ignore_shield,
                      bool ignore_transform) const
{
    if (item == NULL)
    {
        // Unarmed combat.
        item_def fake;
        fake.base_type = OBJ_UNASSIGNED;
        return can_wield(fake, ignore_curse, ignore_brand, ignore_transform);
    }
    else
        return can_wield(*item, ignore_curse, ignore_brand, ignore_transform);
}

bool actor::can_pass_through(int x, int y) const
{
    return can_pass_through_feat(grd[x][y]);
}

bool actor::can_pass_through(const coord_def &c) const
{
    return can_pass_through_feat(grd(c));
}

bool actor::is_habitable(const coord_def &_pos) const
{
    return is_habitable_feat(grd(_pos));
}

bool actor::handle_trap()
{
    trap_def* trap = find_trap(pos());
    if (trap)
        trap->trigger(*this);
    return (trap != NULL);
}
