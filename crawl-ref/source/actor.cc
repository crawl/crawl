#include "AppHdr.h"

#include "actor.h"
#include "env.h"
#include "player.h"
#include "random.h"
#include "state.h"
#include "stuff.h"
#include "traps.h"

actor::actor()
    : los_no_trans(los_def(coord_def(0,0), opacity_no_trans()))
{
}

actor::~actor()
{
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

bool actor::check_res_magic(int power)
{
    const int mrs = res_magic();

    if (mrs == MAG_IMMUNE)
        return (true);

    // Evil, evil hack to make weak one hd monsters easier for first level
    // characters who have resistable 1st level spells. Six is a very special
    // value because mrs = hd * 2 * 3 for most monsters, and the weak, low
    // level monsters have been adjusted so that the "3" is typically a 1.
    // There are some notable one hd monsters that shouldn't fall under this,
    // so we do < 6, instead of <= 6...  or checking monster->hit_dice.  The
    // goal here is to make the first level easier for these classes and give
    // them a better shot at getting to level two or three and spells that can
    // help them out (or building a level or two of their base skill so they
    // aren't resisted as often). - bwr
    if (atype() == ACT_MONSTER && mrs < 6 && coinflip())
        return (false);

    power = stepdown_value(power, 30, 40, 100, 120);

    const int mrchance = (100 + mrs) - power;
    const int mrch2 = random2(100) + random2(101);

#if DEBUG_DIAGNOSTICS
    mprf(MSGCH_DIAGNOSTICS,
         "Power: %d, MR: %d, target: %d, roll: %d",
         power, mrs, mrchance, mrch2);
#endif

    return (mrch2 < mrchance);
}

void actor::set_position(const coord_def &c)
{
    position = c;
    los.set_center(c);
    los_no_trans.set_center(c);
}
