/*
 *  File:     spl-monench.cc
 *  Summary:  Monster-affecting enchantment spells.
 *            Other targeted enchantments are handled in spl-zap.cc.
 */

#include "AppHdr.h"

#include "spl-monench.h"
#include "externs.h"

#include "areas.h"
#include "coord.h"
#include "env.h"
#include "message.h"
#include "mon-place.h"
#include "mon-stuff.h"
#include "random.h"
#include "shout.h"
#include "spl-util.h"
#include "stuff.h"
#include "terrain.h"
#include "viewmap.h"

static int _sleep_monsters(coord_def where, int pow, int, actor *)
{
    monsters* mons = monster_at(where);
    if (!mons)
        return (0);

    if (!mons->can_hibernate(true))
        return (0);

    if (mons->check_res_magic(pow))
        return (0);

    const int res = mons->res_cold();
    if (res > 0 && one_chance_in(std::max(4 - res, 1)))
        return (0);

    if (mons->has_ench(ENCH_SLEEP_WARY) && !one_chance_in(3))
        return (0);

    mons->hibernate();
    mons->expose_to_element(BEAM_COLD, 2);
    return (1);
}

void cast_mass_sleep(int pow)
{
    apply_area_visible(_sleep_monsters, pow);
}

// This is a hack until we set an is_beast flag in the monster data
// (which we might never do, this is sort of minor.)
// It's a list of monster types which can be affected by beast taming.
static bool _is_domesticated_animal(int type)
{
    const monster_type types[] = {
        MONS_GIANT_BAT, MONS_HOUND, MONS_JACKAL, MONS_RAT,
        MONS_YAK, MONS_WYVERN, MONS_HIPPOGRIFF, MONS_GRIFFON,
        MONS_DEATH_YAK, MONS_WAR_DOG, MONS_GREY_RAT,
        MONS_GREEN_RAT, MONS_ORANGE_RAT, MONS_SHEEP,
        MONS_HOG, MONS_GIANT_FROG, MONS_GIANT_TOAD,
        MONS_SPINY_FROG, MONS_BLINK_FROG, MONS_WOLF, MONS_WARG,
        MONS_BEAR, MONS_GRIZZLY_BEAR, MONS_POLAR_BEAR, MONS_BLACK_BEAR
    };

    for (unsigned int i = 0; i < ARRAYSZ(types); ++i)
        if (types[i] == type)
            return (true);

    return (false);
}

static int _tame_beast_monsters(coord_def where, int pow, int, actor *)
{
    monsters* mons = monster_at(where);
    if (mons == NULL)
        return 0;

    if (!_is_domesticated_animal(mons->type) || mons->friendly()
        || player_will_anger_monster(mons))
    {
        return 0;
    }

    // 50% bonus for dogs
    if (mons->type == MONS_HOUND || mons->type == MONS_WAR_DOG )
        pow += (pow / 2);

    if (you.species == SP_HILL_ORC && mons->type == MONS_WARG)
        pow += (pow / 2);

    if (mons->check_res_magic(pow))
        return 0;

    simple_monster_message(mons, " is tamed!");

    if (random2(100) < random2(pow / 10))
        mons->attitude = ATT_FRIENDLY;  // permanent
    else
        mons->add_ench(ENCH_CHARM);     // temporary
    mons_att_changed(mons);

    return 1;
}

void cast_tame_beasts(int pow)
{
    apply_area_visible(_tame_beast_monsters, pow);
}

bool backlight_monsters(coord_def where, int pow, int garbage)
{
    UNUSED(pow);
    UNUSED(garbage);

    monsters* mons = monster_at(where);
    if (mons == NULL)
        return (false);

    // Already glowing.
    if (mons_class_flag(mons->type, M_GLOWS_LIGHT)
        || mons_class_flag(mons->type, M_GLOWS_RADIATION))
    {
        return (false);
    }

    mon_enchant bklt = mons->get_ench(ENCH_CORONA);
    const int lvl = bklt.degree;

    // This enchantment overrides invisibility (neat).
    if (mons->has_ench(ENCH_INVIS))
    {
        if (!mons->has_ench(ENCH_CORONA))
        {
            mons->add_ench(
                mon_enchant(ENCH_CORONA, 1, KC_OTHER, random_range(30, 50)));
            simple_monster_message(mons, " is lined in light.");
        }
        return (true);
    }

    mons->add_ench(mon_enchant(ENCH_CORONA, 1));

    if (lvl == 0)
        simple_monster_message(mons, " is outlined in light.");
    else if (lvl == 4)
        simple_monster_message(mons, " glows brighter for a moment.");
    else
        simple_monster_message(mons, " glows brighter.");

    return (true);
}

bool do_slow_monster(monsters* mon, kill_category whose_kill)
{
    // Try to remove haste, if monster is hasted.
    if (mon->del_ench(ENCH_HASTE, true))
    {
        if (simple_monster_message(mon, " is no longer moving quickly."))
            return (true);
    }

    // Not hasted, slow it.
    if (!mon->has_ench(ENCH_SLOW)
        && !mons_is_stationary(mon)
        && mon->add_ench(mon_enchant(ENCH_SLOW, 0, whose_kill)))
    {
        if (!mon->paralysed() && !mon->petrified()
            && simple_monster_message(mon, " seems to slow down."))
        {
            return (true);
        }
    }

    return (false);
}

bool project_noise()
{
    bool success = false;

    coord_def pos(1, 0);
    level_pos lpos;

    mpr("Choose the noise's source (press '.' or delete to select).");
    more();

    // Might abort with SIG_HUP despite !allow_esc.
    if (!show_map(lpos, false, false, false))
        lpos = level_pos::current();
    pos = lpos.pos;

    redraw_screen();

    dprf("Target square (%d,%d)", pos.x, pos.y);

    if (!in_bounds(pos) || !silenced(pos))
    {
        if (in_bounds(pos) && !feat_is_solid(grd(pos)))
        {
            noisy(30, pos);
            success = true;
        }

        if (!silenced(you.pos()))
        {
            if (success)
            {
                mprf(MSGCH_SOUND, "You hear a %svoice call your name.",
                     (!you.see_cell(pos) ? "distant " : ""));
            }
            else
                mprf(MSGCH_SOUND, "You hear a dull thud.");
        }
    }

    return (success);
}
