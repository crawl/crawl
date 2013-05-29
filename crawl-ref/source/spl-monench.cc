/**
 * @file
 * @brief Monster-affecting enchantment spells.
 *           Other targeted enchantments are handled in spl-zap.cc.
**/

#include "AppHdr.h"

#include "spl-monench.h"
#include "externs.h"

#include "areas.h"
#include "coord.h"
#include "env.h"
#include "message.h"
#include "mon-stuff.h"
#include "random.h"
#include "shout.h"
#include "spl-util.h"
#include "stuff.h"
#include "terrain.h"
#include "viewmap.h"

static int _englaciate_monsters(coord_def where, int pow, int, actor *actor)
{
    monster* mons = monster_at(where);

    if (!mons)
        return 0;

    if (mons->res_cold() > 0 || mons_is_stationary(mons))
    {
        if (!mons_is_firewood(mons))
            simple_monster_message(mons, " is unaffected.");
        return 0;
    }

    int duration = (roll_dice(3, pow) / 6 - random2(mons->get_experience_level()))
                    * BASELINE_DELAY;

    if (duration <= 0)
    {
        simple_monster_message(mons, " resists.");
        return 0;
    }

    if (mons_class_flag(mons->type, M_COLD_BLOOD))
        duration *= 2;

    return do_slow_monster(mons, actor, duration);
}

spret_type cast_englaciation(int pow, bool fail)
{
    fail_check();
    mpr("You radiate an aura of cold.");
    apply_area_visible(_englaciate_monsters, pow, &you);
    return SPRET_SUCCESS;
}

bool backlight_monsters(coord_def where, int pow, int garbage)
{
    UNUSED(pow);
    UNUSED(garbage);

    monster* mons = monster_at(where);
    if (mons == NULL)
        return false;

    // Already glowing.
    if (mons->glows_naturally())
        return false;

    mon_enchant bklt = mons->get_ench(ENCH_CORONA);
    mon_enchant zin_bklt = mons->get_ench(ENCH_SILVER_CORONA);
    const int lvl = bklt.degree + zin_bklt.degree;

    // This enchantment overrides invisibility (neat).
    if (mons->has_ench(ENCH_INVIS))
    {
        if (!mons->has_ench(ENCH_CORONA) && !mons->has_ench(ENCH_SILVER_CORONA))
        {
            mons->add_ench(
                mon_enchant(ENCH_CORONA, 1, 0, random_range(30, 50)));
            simple_monster_message(mons, " is lined in light.");
        }
        return true;
    }

    mons->add_ench(mon_enchant(ENCH_CORONA, 1));

    if (lvl == 0)
        simple_monster_message(mons, " is outlined in light.");
    else if (lvl == 4)
        simple_monster_message(mons, " glows brighter for a moment.");
    else
        simple_monster_message(mons, " glows brighter.");

    return true;
}

bool do_slow_monster(monster* mon, const actor* agent, int dur)
{
    if (mon->check_stasis(false))
        return true;

    if (!mon->has_ench(ENCH_SLOW)
        && !mons_is_stationary(mon)
        && mon->add_ench(mon_enchant(ENCH_SLOW, 0, agent, dur)))
    {
        if (!mon->paralysed() && !mon->petrified()
            && simple_monster_message(mon, " seems to slow down."))
        {
            return true;
        }
    }

    return false;
}
