/*  File:       mislead.cc
 *  Summary:    Handling of Mara's Mislead spell and stats, plus fakes.
 */

#include "AppHdr.h"
#include "mislead.h"

#include "enum.h"
#include "env.h"
#include "hints.h"
#include "message.h"
#include "monster.h"
#include "mon-iter.h"
#include "mon-util.h"
#include "random.h"
#ifdef USE_TILE
 #include "tilepick.h"
#endif
#include "view.h"
#include "xom.h"

bool unsuitable_misled_monster(monster_type mons)
{
    return (mons_is_unique(mons)
            || mons_is_mimic(mons)
            || mons_is_ghost_demon(mons)
            || mons_class_is_stationary(mons)
            || mons_class_is_zombified(mons)
            || mons_is_tentacle(mons)
            || mons_class_flag(mons, M_NO_POLY_TO)
            || mons_genus(mons) == MONS_DRACONIAN
            || mons == MONS_MANTICORE
            || mons == MONS_SLIME_CREATURE
            || mons == MONS_HYDRA
            || mons == MONS_KRAKEN
            || mons == MONS_BALLISTOMYCETE
            || mons == MONS_HYPERACTIVE_BALLISTOMYCETE
            || mons == MONS_SHAPESHIFTER
            || mons == MONS_GLOWING_SHAPESHIFTER
            || mons == MONS_KILLER_KLOWN
            || mons == MONS_GIANT_BAT);
}

monster_type get_misled_monster(monster* mons)
{
    monster_type mt = random_monster_at_grid(mons->pos());
    if (unsuitable_misled_monster(mt))
        mt = random_monster_at_grid(mons->pos());

    if (unsuitable_misled_monster(mt))
        return (MONS_GIANT_BAT);

    return mt;
}

bool update_mislead_monster(monster* mons)
{
    // Don't affect uniques, named monsters, and monsters with special tiles.
    if (mons_is_unique(mons->type) || !mons->mname.empty()
        || mons->props.exists("monster_tile")
        || mons->props.exists("mislead_as")
        || mons->type == MONS_ORB_OF_DESTRUCTION)
    {
        return (false);
    }

    short misled_as = get_misled_monster(mons);
    mons->props["mislead_as"] = misled_as;

    if (misled_as == MONS_GIANT_BAT)
        return (false);

    return (true);
}

int update_mislead_monsters(monster* caster)
{
    int count = 0;

    for (monster_iterator mi; mi; ++mi)
        if (*mi != caster && update_mislead_monster(*mi))
            count++;

    return count;
}

void mons_cast_mislead(monster* mons)
{
    // This really only affects the player; it causes confusion when cast on
    // non-player foes, but that is dealt with inside ye-great-Switch-of-Doom.
    if (mons->foe != MHITYOU)
        return;

    // We deal with pointless misleads in the right place now.

    if (player_mental_clarity(true))
    {
        mpr("Your vision blurs momentarily.");
        return;
    }

    update_mislead_monsters(mons);

    const int old_value = you.duration[DUR_MISLED];
    you.increase_duration(DUR_MISLED, mons->hit_dice * 12 / 3, 50);
    if (you.duration[DUR_MISLED] > old_value)
    {
        you.check_awaken(500);

        if (old_value <= 0)
        {
            mpr("But for a moment, strange images dance in front of your eyes.", MSGCH_WARN);
#ifdef USE_TILE
            tiles.add_overlay(you.pos(), tileidx_zap(MAGENTA));
            update_screen();
#else
            flash_view(MAGENTA);
#endif
            more();
        }
        else
            mpr("You are even more misled!", MSGCH_WARN);

        learned_something_new(HINT_YOU_ENCHANTED);

        xom_is_stimulated((you.duration[DUR_MISLED] - old_value)
                           / BASELINE_DELAY);
    }

    return;
}

int count_mara_fakes()
{
    int count = 0;
    for (monster_iterator mi; mi; ++mi)
    {
        if (mi->type == MONS_MARA_FAKE)
            count++;
    }

    return count;
}
