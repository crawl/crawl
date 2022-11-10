/**
 * @file
 * @brief Character choice restrictions.
 *
 * The functions in this file are "pure": They don't
 * access any global data.
**/

#include "AppHdr.h"

#include "ng-restr.h"

#include "item-prop.h" // item_attack_skill
#include "jobs.h"
#include "mutation-type.h"
#include "newgame.h"
#include "newgame-def.h"
#include "size-type.h"
#include "species.h"

static bool _banned_combination(job_type job, species_type species)
{
    if (species::mutation_level(species, MUT_NO_GRASPING)
        && (job == JOB_GLADIATOR
            || job == JOB_BRIGAND
            || job == JOB_HUNTER
            || job == JOB_HEXSLINGER))
    {
        return true;
    }

    if (species::mutation_level(species, MUT_FORLORN)
        && (job == JOB_BERSERKER
            || job == JOB_CHAOS_KNIGHT
            || job == JOB_CINDER_ACOLYTE
            || job == JOB_MONK))
    {
        return true;
    }

    if (job == JOB_TRANSMUTER && species::undead_type(species) == US_UNDEAD)
        return true;

    return false;
}

char_choice_restriction species_allowed(job_type job, species_type speci)
{
    if (!species::is_starting_species(speci) || !is_starting_job(job))
        return CC_BANNED;

    if (_banned_combination(job, speci))
        return CC_BANNED;

    if (job_recommends_species(job, speci))
        return CC_UNRESTRICTED;

    return CC_RESTRICTED;
}

bool character_is_allowed(species_type species, job_type job)
{
    return !_banned_combination(job, species);
}

char_choice_restriction job_allowed(species_type speci, job_type job)
{
    if (!species::is_starting_species(speci) || !is_starting_job(job))
        return CC_BANNED;

    if (_banned_combination(job, speci))
        return CC_BANNED;

    if (species::recommends_job(speci, job))
        return CC_UNRESTRICTED;

    return CC_RESTRICTED;
}

// Is the given god restricted for the character defined by ng?
// Only uses ng.species and ng.job.
char_choice_restriction weapon_restriction(weapon_type wpn,
                                           const newgame_def &ng)
{
    ASSERT(species::is_starting_species(ng.species));
    ASSERT(is_starting_job(ng.job));

    // Some special cases:

    if (species::mutation_level(ng.species, MUT_NO_GRASPING) && wpn != WPN_UNARMED)
        return CC_BANNED;

    if (wpn == WPN_QUARTERSTAFF && ng.job != JOB_GLADIATOR
        && !(ng.job == JOB_FIGHTER
             // formicids are allowed to have shield + quarterstaff
             && species::mutation_level(ng.species, MUT_QUADRUMANOUS)))
    {
        return CC_BANNED;
    }

    if (species::recommends_weapon(ng.species, wpn)
        // Don't recommend short blades for fighters - stabbing and heavy
        // armour + no stab enablers aren't an amazing combo.
        && (ng.job != JOB_FIGHTER
            || wpn == WPN_UNARMED
            || item_attack_skill(OBJ_WEAPONS, wpn) != SK_SHORT_BLADES))
    {
        return CC_UNRESTRICTED;
    }

    return CC_RESTRICTED;
}
