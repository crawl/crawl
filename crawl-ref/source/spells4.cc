/*
 *  File:       spells4.cc
 *  Summary:    new spells
 *  Written by: Copyleft Josh Fishman 1999-2000, All Rights Preserved
 */

#include "AppHdr.h"
#include "externs.h"

#include "areas.h"
#include "coord.h"
#include "delay.h"
#include "hints.h"
#include "makeitem.h"
#include "message.h"
#include "mon-place.h"
#include "spl-util.h"

void cast_see_invisible(int pow)
{
    if (you.can_see_invisible())
        mpr("You feel as though your vision will be sharpened longer.");
    else
    {
        mpr("Your vision seems to sharpen.");

        // We might have to turn autopickup back on again.
        // TODO: Once the spell times out we might want to check all monsters
        //       in LOS for invisibility and turn autopickup off again, if
        //       needed.
        autotoggle_autopickup(false);
    }

    // No message if you already are under the spell.
    you.increase_duration(DUR_SEE_INVISIBLE, 10 + random2(2 + pow/2), 100);
}

static int _sleep_monsters(coord_def where, int pow, int, actor *)
{
    monsters *monster = monster_at(where);
    if (!monster)
        return (0);

    if (!monster->can_hibernate(true))
        return (0);

    if (monster->check_res_magic(pow))
        return (0);

    const int res = monster->res_cold();
    if (res > 0 && one_chance_in(std::max(4 - res, 1)))
        return (0);

    if (monster->has_ench(ENCH_SLEEP_WARY) && !one_chance_in(3))
        return (0);

    monster->hibernate();
    monster->expose_to_element(BEAM_COLD, 2);
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
    monsters *monster = monster_at(where);
    if (monster == NULL)
        return 0;

    if (!_is_domesticated_animal(monster->type) || monster->friendly()
        || player_will_anger_monster(monster))
    {
        return 0;
    }

    // 50% bonus for dogs
    if (monster->type == MONS_HOUND || monster->type == MONS_WAR_DOG )
        pow += (pow / 2);

    if (you.species == SP_HILL_ORC && monster->type == MONS_WARG)
        pow += (pow / 2);

    if (monster->check_res_magic(pow))
        return 0;

    simple_monster_message(monster, " is tamed!");

    if (random2(100) < random2(pow / 10))
        monster->attitude = ATT_FRIENDLY;  // permanent
    else
        monster->add_ench(ENCH_CHARM);     // temporary
    mons_att_changed(monster);

    return 1;
}

void cast_tame_beasts(int pow)
{
    apply_area_visible(_tame_beast_monsters, pow);
}

void cast_silence(int pow)
{
    if (!you.attribute[ATTR_WAS_SILENCED])
        mpr("A profound silence engulfs you.");

    you.attribute[ATTR_WAS_SILENCED] = 1;

    you.increase_duration(DUR_SILENCE, 10 + random2avg(pow, 2), 100);
    invalidate_agrid(true);

    if (you.beheld())
        you.update_beholders();

    learned_something_new(HINT_YOU_SILENCE);
}

bool backlight_monsters(coord_def where, int pow, int garbage)
{
    UNUSED(pow);
    UNUSED(garbage);

    monsters *monster = monster_at(where);
    if (monster == NULL)
        return (false);

    // Already glowing.
    if (mons_class_flag(monster->type, M_GLOWS_LIGHT)
        || mons_class_flag(monster->type, M_GLOWS_RADIATION))
    {
        return (false);
    }

    mon_enchant bklt = monster->get_ench(ENCH_CORONA);
    const int lvl = bklt.degree;

    // This enchantment overrides invisibility (neat).
    if (monster->has_ench(ENCH_INVIS))
    {
        if (!monster->has_ench(ENCH_CORONA))
        {
            monster->add_ench(
                mon_enchant(ENCH_CORONA, 1, KC_OTHER, random_range(30, 50)));
            simple_monster_message(monster, " is lined in light.");
        }
        return (true);
    }

    monster->add_ench(mon_enchant(ENCH_CORONA, 1));

    if (lvl == 0)
        simple_monster_message(monster, " is outlined in light.");
    else if (lvl == 4)
        simple_monster_message(monster, " glows brighter for a moment.");
    else
        simple_monster_message(monster, " glows brighter.");

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
