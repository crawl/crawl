/**
 * @file
 * @brief Monster level-up code.
**/

#include <functional>

#include "AppHdr.h"

#include "mon-grow.h"

#include "message.h"
#include "mon-place.h"
#include "monster.h"
#include "mon-util.h"

// Base experience required by a monster to reach HD 1.
const int monster_xp_base       = 15;
// Experience multiplier to determine the experience needed to gain levels.
const int monster_xp_multiplier = 150;
const mons_experience_levels mexplevs;

// Monster growing-up sequences, for Beogh orcs. You can specify a chance to
// indicate that the monster only has a chance of changing type, otherwise the
// monster will grow up when it reaches the HD of the target form.
//
// No special cases are in place: make sure source and target forms are similar.
// If the target form requires special handling of some sort, add the handling
// to level_up_change().

static const monster_level_up mon_grow[] =
{
    monster_level_up(MONS_ORC, MONS_ORC_WARRIOR),
    monster_level_up(MONS_ORC_WARRIOR, MONS_ORC_KNIGHT),
    monster_level_up(MONS_ORC_KNIGHT, MONS_ORC_WARLORD),
    monster_level_up(MONS_ORC_PRIEST, MONS_ORC_HIGH_PRIEST),
    monster_level_up(MONS_ORC_WIZARD, MONS_ORC_SORCERER),
    monster_level_up(MONS_BLORK_THE_ORC, MONS_BLORK_THE_ORC_WARRIOR),
    monster_level_up(MONS_BLORK_THE_ORC_WARRIOR, MONS_BLORK_THE_ORC_KNIGHT),
    monster_level_up(MONS_BLORK_THE_ORC_KNIGHT, MONS_BLORK_THE_ORC_WARLORD),
    monster_level_up(MONS_URUG, MONS_URUG_II),
    monster_level_up(MONS_NERGALLE, MONS_NERGALLE_II),
    monster_level_up(MONS_ASCLEPIA, MONS_ASCLEPIA_II),   
    monster_level_up(MONS_BRANDAGOTH, MONS_BRANDAGOTH_II),

    monster_level_up(MONS_KOBOLD, MONS_BIG_KOBOLD),

    monster_level_up(MONS_UGLY_THING, MONS_VERY_UGLY_THING),

    monster_level_up(MONS_CENTAUR, MONS_CENTAUR_WARRIOR),
    monster_level_up(MONS_YAKTAUR, MONS_YAKTAUR_CAPTAIN),

    monster_level_up(MONS_NAGA, MONS_NAGA_WARRIOR),
    monster_level_up(MONS_NAGA_MAGE, MONS_NAGARAJA),

    monster_level_up(MONS_DEEP_ELF_SOLDIER, MONS_DEEP_ELF_FIGHTER),
    monster_level_up(MONS_DEEP_ELF_FIGHTER, MONS_DEEP_ELF_KNIGHT),

    // Deep elf magi can become either summoners or conjurers.
    monster_level_up(MONS_DEEP_ELF_MAGE, MONS_DEEP_ELF_SUMMONER, 500),
    monster_level_up(MONS_DEEP_ELF_MAGE, MONS_DEEP_ELF_CONJURER),

    monster_level_up(MONS_DEEP_ELF_PRIEST, MONS_DEEP_ELF_HIGH_PRIEST),
    monster_level_up(MONS_DEEP_ELF_CONJURER, MONS_DEEP_ELF_ANNIHILATOR),

    monster_level_up(MONS_DEEP_ELF_SUMMONER, MONS_DEEP_ELF_DEMONOLOGIST, 500),
    monster_level_up(MONS_DEEP_ELF_SUMMONER, MONS_DEEP_ELF_SORCERER),

    monster_level_up(MONS_BABY_ALLIGATOR, MONS_ALLIGATOR),

    monster_level_up(MONS_GNOLL, MONS_GNOLL_SERGEANT),

    monster_level_up(MONS_MERFOLK, MONS_MERFOLK_IMPALER),

    monster_level_up(MONS_FAUN, MONS_SATYR),
    monster_level_up(MONS_TENGU, MONS_TENGU_CONJURER, 500),
    monster_level_up(MONS_TENGU, MONS_TENGU_WARRIOR),
    monster_level_up(MONS_TENGU_CONJURER, MONS_TENGU_REAVER),
    monster_level_up(MONS_TENGU_WARRIOR, MONS_TENGU_REAVER),

    // mercenaries for JOB_CARAVAN
    monster_level_up(MONS_MERC_FIGHTER, MONS_MERC_KNIGHT),
    monster_level_up(MONS_MERC_KNIGHT, MONS_MERC_DEATH_KNIGHT, 500),
    monster_level_up(MONS_MERC_KNIGHT, MONS_MERC_PALADIN),
    
    monster_level_up(MONS_MERC_SKALD, MONS_MERC_INFUSER),
    monster_level_up(MONS_MERC_INFUSER, MONS_MERC_TIDEHUNTER),
    
    monster_level_up(MONS_MERC_WITCH, MONS_MERC_SORCERESS),
    monster_level_up(MONS_MERC_SORCERESS, MONS_MERC_ELEMENTALIST),
    
    monster_level_up(MONS_MERC_BRIGAND, MONS_MERC_ASSASSIN),
    monster_level_up(MONS_MERC_ASSASSIN, MONS_MERC_CLEANER),

};

static const map<monster_type, mon_lev_up_cond> mon_grow_cond =
{
    { MONS_ASCLEPIA, {[](const monster &caster) {return (you.religion == GOD_BEOGH && caster.attitude > ATT_NEUTRAL);}} },
    { MONS_BRANDAGOTH, {[](const monster &caster) {return caster.blessed;}} }
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
            min(max(delta, monster_xp_base * monster_xp_multiplier / 100),
                40000);
        experience += delta;
    }
}

static const monster_level_up *_monster_level_up_target(monster_type type,
                                                        int hit_dice)
{
    for (const monster_level_up &mlup : mon_grow)
    {
        if (mlup.before == type)
        {
            const monsterentry *me = get_monster_data(mlup.after);
            if (me->HD == hit_dice && x_chance_in_y(mlup.chance, 1000))
                return &mlup;
        }
    }
    return nullptr;
}

void monster::upgrade_type(monster_type after, bool adjust_hd,
                            bool adjust_hp)
{
    // Ta-da!
    type   = after;

    // Initialise a dummy monster to save work.
    monster dummy;
    dummy.type         = after;
    dummy.base_monster = base_monster;
    dummy.number       = number;
    define_monster(dummy); // assumes after is not a zombie

    colour = dummy.colour;
    speed  = dummy.speed;
    spells = dummy.spells;
    calc_speed();

    if (adjust_hd)
        set_hit_dice(max(get_experience_level(), dummy.get_hit_dice()));

    if (adjust_hp)
    {
        const int minhp = dummy.max_hit_points;
        if (max_hit_points < minhp)
        {
            hit_points    += minhp - max_hit_points;
            max_hit_points = minhp;
            hit_points     = min(hit_points, max_hit_points);
        }
    }
}

bool monster::level_up_change()
{
    const monster_level_up *lup =
        _monster_level_up_target(type, get_experience_level());

    const mon_lev_up_cond *lup_cond;
    if (lup)
    {
        lup_cond = map_find(mon_grow_cond, lup->before);
        if (!lup_cond || lup_cond->condition(*this))
        {
            upgrade_type(lup->after, false, lup->adjust_hp);
            return true;
        }
    }
    return false;
}

bool monster::level_up()
{
    if (get_experience_level() >= MAX_MONS_HD)
        return false;

    set_hit_dice(get_experience_level() + 1);

    // A little maxhp boost.
    if (max_hit_points < 1000)
    {
        int hpboost =
            (get_experience_level() > 3? max_hit_points / 8 : max_hit_points / 4)
            + random2(5);

        // Not less than 3 hp, not more than 25.
        hpboost = min(max(hpboost, 3), 25);

        dprf("%s: HD: %d, maxhp: %d, boost: %d",
             name(DESC_PLAIN).c_str(), get_experience_level(),
             max_hit_points, hpboost);

        max_hit_points += hpboost;
        hit_points     += hpboost;
        hit_points      = min(hit_points, max_hit_points);
    }

    level_up_change();

    return true;
}

void monster::init_experience()
{
    if (experience || !alive())
        return;
    set_hit_dice(max(get_experience_level(), 1));
    experience = mexplevs[min(get_experience_level(), MAX_MONS_HD)];
}

bool monster::gain_exp(int exp, int max_levels_to_gain)
{
    if (!alive())
        return false;

    init_experience();
    if (get_experience_level() >= MAX_MONS_HD)
        return false;

    // Only natural monsters can level-up.
    if (!(holiness() & MH_NATURAL))
        return false;

    // Only monsters that you can gain XP from can level-up.
    if (!mons_class_gives_xp(type) || is_summoned())
        return false;

    // Avoid wrap-around.
    if (experience + exp < experience)
        return false;

    experience += exp;

    const monster mcopy(*this);
    int levels_gained = 0;
    // Monsters can normally gain a maximum of two levels from one kill.
    while (get_experience_level() < MAX_MONS_HD
           && experience >= mexplevs[get_experience_level() + 1]
           && level_up()
           && ++levels_gained < max_levels_to_gain);

    if (levels_gained)
    {
        if (mons_intel(*this) >= I_HUMAN)
            simple_monster_message(mcopy, " looks more experienced.");
        else
            simple_monster_message(mcopy, " looks stronger.");
    }

    if (get_experience_level() < MAX_MONS_HD
        && experience >= mexplevs[get_experience_level() + 1])
    {
        experience = (mexplevs[get_experience_level()]
                      + mexplevs[get_experience_level() + 1]) / 2;
    }

    // If the monster has leveled up to a monster that will be angered
    // by the player, handle it properly.
    player_angers_monster(this);

    return levels_gained > 0;
}
