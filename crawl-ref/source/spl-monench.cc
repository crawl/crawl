/**
 * @file
 * @brief Monster-affecting enchantment spells.
 *           Other targeted enchantments are handled in spl-zap.cc.
**/

#include "AppHdr.h"

#include "spl-monench.h"
#include "externs.h"

#include "areas.h"
#include "env.h"
#include "message.h"
#include "mon-stuff.h"
#include "random.h"
#include "spl-util.h"
#include "stuff.h"
#include "terrain.h"

int englaciate(coord_def where, int pow, int, actor *agent)
{
    actor *victim = actor_at(where);

    if (!victim || victim == agent)
        return 0;

    monster* mons = victim->as_monster();

    if (victim->res_cold() > 0
        || victim->is_stationary())
    {
        if (!mons)
            canned_msg(MSG_YOU_UNAFFECTED);
        else if (mons && !mons_is_firewood(mons))
            simple_monster_message(mons, " is unaffected.");
        return 0;
    }

    int duration = (roll_dice(3, pow) / 6
                    - random2(victim->get_experience_level()))
                    * BASELINE_DELAY;

    if (duration <= 0)
    {
        if (!mons)
            canned_msg(MSG_YOU_RESIST);
        else
            simple_monster_message(mons, " resists.");
        return 0;
    }

    if ((!mons && player_genus(GENPC_DRACONIAN)) // res_cold() checked above
        || (mons && mons_class_flag(mons->type, M_COLD_BLOOD)))
        duration *= 2;

    if (!mons)
        return slow_player(duration);

    return do_slow_monster(mons, agent, duration);
}

spret_type cast_englaciation(int pow, bool fail)
{
    fail_check();
    mpr("You radiate an aura of cold.");
    apply_area_visible(englaciate, pow, &you);
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
        && !mon->is_stationary()
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
