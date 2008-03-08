/*
 *  File:       mgrow.cc
 *  Summary:    Monster level-up code.
 *  Written by: dshaligram on Fri Oct 26 08:33:37 2007 UTC
 *
 *  Modified for Crawl Reference by $Author: dshaligram $ on $Date: 2007-10-25T09:49:39.320031Z $
 */

#include "AppHdr.h"
#include "enum.h"
#include "mgrow.h"
#include "mon-util.h"
#include "monstuff.h"
#include "stuff.h"

// Base experience required by a monster to reach HD 1.
const int monster_xp_base       = 15;
// Experience multiplier to determine the experience needed to gain levels.
const int monster_xp_multiplier = 150;
const mons_experience_levels mexplevs;

// Monster growing-up sequences. You can specify a chance to indicate that
// the monster only has a chance of changing type, otherwise the monster
// will grow up when it reaches the HD of the target form.
//
// No special cases are in place: make sure source and target forms are similar.
// If the target form requires special handling of some sort, add the handling
// to level_up_change().
//
static const monster_level_up mon_grow[] =
{
    monster_level_up(MONS_ORC, MONS_ORC_WARRIOR),
    monster_level_up(MONS_ORC_WARRIOR, MONS_ORC_KNIGHT),
    monster_level_up(MONS_ORC_KNIGHT, MONS_ORC_WARLORD),
    monster_level_up(MONS_ORC_PRIEST, MONS_ORC_HIGH_PRIEST),
    monster_level_up(MONS_ORC_WIZARD, MONS_ORC_SORCERER),

    monster_level_up(MONS_KOBOLD, MONS_BIG_KOBOLD),

    monster_level_up(MONS_UGLY_THING, MONS_VERY_UGLY_THING),

    monster_level_up(MONS_ANT_LARVA, MONS_GIANT_ANT),

    monster_level_up(MONS_KILLER_BEE_LARVA, MONS_KILLER_BEE),
    
    monster_level_up(MONS_CENTAUR, MONS_CENTAUR_WARRIOR),
    monster_level_up(MONS_YAKTAUR, MONS_YAKTAUR_CAPTAIN),

    monster_level_up(MONS_NAGA, MONS_NAGA_WARRIOR),
    monster_level_up(MONS_NAGA_MAGE, MONS_GREATER_NAGA),

    monster_level_up(MONS_DEEP_ELF_SOLDIER, MONS_DEEP_ELF_FIGHTER),
    monster_level_up(MONS_DEEP_ELF_FIGHTER, MONS_DEEP_ELF_KNIGHT),

    // deep elf magi can become either conjurers or summoners.
    monster_level_up(MONS_DEEP_ELF_MAGE, MONS_DEEP_ELF_SUMMONER, 500),
    monster_level_up(MONS_DEEP_ELF_MAGE, MONS_DEEP_ELF_CONJURER),
    monster_level_up(MONS_DEEP_ELF_PRIEST, MONS_DEEP_ELF_HIGH_PRIEST),
};

mons_experience_levels::mons_experience_levels()
{
    int experience = monster_xp_base;
    for (int i = 1; i <= MAX_MONS_HD; ++i)
    {
        mexp[i] = experience;

        int delta =
            (monster_xp_base + experience) * 2 * monster_xp_multiplier
            / 500;
        delta =
            std::min(
                std::max(delta, monster_xp_base * monster_xp_multiplier / 100),
                2000);
        experience += delta;
    }
}

static const monster_level_up *monster_level_up_target(
    monster_type type, int hit_dice)
{
    for (unsigned i = 0; i < ARRAYSIZE(mon_grow); ++i)
    {
        const monster_level_up &mlup(mon_grow[i]);
        if (mlup.before == type)
        {
            const monsterentry *me = get_monster_data(mlup.after);
            if (static_cast<int>(me->hpdice[0]) == hit_dice
                && random2(1000) < mlup.chance)
            {
                return (&mlup);
            }
        }
    }
    return (NULL);
}

bool monsters::level_up_change()
{
    if (const monster_level_up *lup =
        monster_level_up_target(static_cast<monster_type>(type), hit_dice))
    {
        const monsterentry *orig = get_monster_data(type);
        // Ta-da!
        type   = lup->after;

        // Initialise a dummy monster to save work.
        monsters dummy;
        dummy.type = type;
        define_monster(dummy);

        colour = dummy.colour;
        speed  = dummy.speed;
        spells = dummy.spells;
        fix_speed();

        const monsterentry *m = get_monster_data(type);
        ac += m->AC - orig->AC;
        ev += m->ev - orig->ev;

        if (lup->adjust_hp)
        {
            const int minhp = dummy.max_hit_points;
            if (max_hit_points < minhp)
            {
                hit_points    += minhp - max_hit_points;
                max_hit_points = minhp;
                hit_points     = std::min(hit_points, max_hit_points);
            }
        }
        return (true);
    }
    return (false);
}

bool monsters::level_up()
{
    if (hit_dice >= MAX_MONS_HD)
        return (false);

    ++hit_dice;

    // A little maxhp boost.
    if (max_hit_points < 1000)
    {
        int hpboost =
            (hit_dice > 3? max_hit_points / 8 : max_hit_points / 4)
            + random2(5);
        
        // Not less than 3 hp, not more than 25.
        hpboost = std::min(std::max(hpboost, 3), 25);

#ifdef DEBUG_DIAGNOSTICS
        mprf(MSGCH_DIAGNOSTICS, "%s: HD: %d, maxhp: %d, boost: %d",
             name(DESC_PLAIN).c_str(), hit_dice, max_hit_points, hpboost);
#endif

        max_hit_points += hpboost;
        hit_points     += hpboost;
        hit_points      = std::min(hit_points, max_hit_points);
    }
    
    level_up_change();

    return (true);
}

void monsters::init_experience()
{
    if (experience || !alive())
        return;
    hit_dice   = std::max(hit_dice, 1);
    experience = mexplevs[std::min(hit_dice, MAX_MONS_HD)];
}

void monsters::gain_exp(int exp)
{
    if (!alive())
        return;

    init_experience();
    if (hit_dice >= MAX_MONS_HD)
        return;

    // Only natural monsters can level-up.
    if (holiness() != MH_NATURAL)
        return;
    
    // Avoid wrap-around.
    if (experience + exp < experience)
        return;

    experience += exp;

    const monsters mcopy(*this);
    int levels_gained = 0;
    // Monsters can gain a maximum of two levels from one kill.
    while (hit_dice < MAX_MONS_HD
           && experience >= mexplevs[hit_dice + 1]
           && level_up()
           && ++levels_gained < 2);

    if (levels_gained)
    {
        if (mons_intel(type) >= I_NORMAL)
            simple_monster_message(&mcopy, " looks more experienced.");
        else
            simple_monster_message(&mcopy, " looks stronger.");
    }

    if (hit_dice < MAX_MONS_HD && experience >= mexplevs[hit_dice + 1])
        experience = (mexplevs[hit_dice] + mexplevs[hit_dice + 1]) / 2;
}
