/**
 * @file
 * @brief Character choice restrictions.
 *
 * The functions in this file are "pure": They don't
 * access any global data.
**/

#include "AppHdr.h"

#include "ng-restr.h"

#include "jobs.h"
#include "newgame.h"
#include "newgame_def.h"
#include "species.h"

static bool _banned_combination(job_type job, species_type species)
{
    return species == SP_FELID
            && (job == JOB_GLADIATOR
                || job == JOB_ASSASSIN
                || job == JOB_HUNTER
                || job == JOB_ARCANE_MARKSMAN)
           || species == SP_DEMIGOD
               && (job == JOB_BERSERKER
                   || job == JOB_CHAOS_KNIGHT
                   || job == JOB_ABYSSAL_KNIGHT)
           || job == JOB_TRANSMUTER
              && (species_undead_type(species) == US_UNDEAD
                  || species_undead_type(species) == US_HUNGRY_DEAD);
}

char_choice_restriction species_allowed(job_type job, species_type speci)
{
    if (!is_starting_species(speci) || !is_starting_job(job))
        return CC_BANNED;

    if (_banned_combination(job, speci))
        return CC_BANNED;

    switch (job)
    {
    case JOB_FIGHTER:
        switch (speci)
        {
        case SP_DEEP_DWARF:
        case SP_HILL_ORC:
        case SP_TROLL:
        case SP_MINOTAUR:
        case SP_GARGOYLE:
        case SP_CENTAUR:
            return CC_UNRESTRICTED;
        default:
            return CC_RESTRICTED;
        }

    case JOB_GLADIATOR:
        switch (speci)
        {
        case SP_DEEP_DWARF:
        case SP_HILL_ORC:
        case SP_MERFOLK:
        case SP_MINOTAUR:
        case SP_GARGOYLE:
        case SP_CENTAUR:
            return CC_UNRESTRICTED;
        default:
            return CC_RESTRICTED;
        }

    case JOB_MONK:
        switch (speci)
        {
        case SP_HILL_ORC:
        case SP_TROLL:
        case SP_MINOTAUR:
        case SP_GARGOYLE:
        case SP_GHOUL:
        case SP_MERFOLK:
        case SP_VAMPIRE:
            return CC_UNRESTRICTED;
        default:
            return CC_RESTRICTED;
        }

    case JOB_BERSERKER:
        switch (speci)
        {
        case SP_HILL_ORC:
        case SP_HALFLING:
        case SP_OGRE:
        case SP_MERFOLK:
        case SP_MINOTAUR:
        case SP_GARGOYLE:
        case SP_DEMONSPAWN:
            return CC_UNRESTRICTED;
        default:
            return CC_RESTRICTED;
        }

    case JOB_CHAOS_KNIGHT:
        switch (speci)
        {
        case SP_HILL_ORC:
        case SP_TROLL:
        case SP_CENTAUR:
        case SP_MERFOLK:
        case SP_MINOTAUR:
        case SP_BASE_DRACONIAN:
        case SP_DEMONSPAWN:
            return CC_UNRESTRICTED;
        default:
            return CC_RESTRICTED;
        }

    case JOB_ABYSSAL_KNIGHT:
        switch (speci)
        {
        case SP_HILL_ORC:
        case SP_SPRIGGAN:
        case SP_TROLL:
        case SP_MERFOLK:
        case SP_BASE_DRACONIAN:
        case SP_DEMONSPAWN:
            return CC_UNRESTRICTED;
        default:
            return CC_RESTRICTED;
        }

    case JOB_SKALD:
        switch (speci)
        {
        case SP_HIGH_ELF:
        case SP_HALFLING:
        case SP_CENTAUR:
        case SP_MERFOLK:
        case SP_BASE_DRACONIAN:
        case SP_VAMPIRE:
            return CC_UNRESTRICTED;
        default:
            return CC_RESTRICTED;
        }

    case JOB_WARPER:
        switch (speci)
        {
        case SP_HALFLING:
        case SP_HIGH_ELF:
        case SP_DEEP_DWARF:
        case SP_SPRIGGAN:
        case SP_CENTAUR:
        case SP_BASE_DRACONIAN:
            return CC_UNRESTRICTED;
        default:
            return CC_RESTRICTED;
        }

    case JOB_WIZARD:
        switch (speci)
        {
        case SP_HIGH_ELF:
        case SP_DEEP_ELF:
        case SP_NAGA:
        case SP_BASE_DRACONIAN:
        case SP_OCTOPODE:
        case SP_HUMAN:
        case SP_MUMMY:
            return CC_UNRESTRICTED;
        default:
            return CC_RESTRICTED;
        }

    case JOB_CONJURER:
        switch (speci)
        {
        case SP_HIGH_ELF:
        case SP_DEEP_ELF:
        case SP_NAGA:
        case SP_TENGU:
        case SP_BASE_DRACONIAN:
        case SP_DEMIGOD:
            return CC_UNRESTRICTED;
        default:
            return CC_RESTRICTED;
        }

    case JOB_ENCHANTER:
        switch (speci)
        {
        case SP_DEEP_ELF:
        case SP_FELID:
        case SP_KOBOLD:
        case SP_SPRIGGAN:
        case SP_NAGA:
        case SP_VAMPIRE:
            return CC_UNRESTRICTED;
        default:
            return CC_RESTRICTED;
        }

    case JOB_SUMMONER:
        switch (speci)
        {
        case SP_DEEP_ELF:
        case SP_HILL_ORC:
        case SP_VINE_STALKER:
        case SP_MERFOLK:
        case SP_TENGU:
        case SP_VAMPIRE:
            return CC_UNRESTRICTED;
        default:
            return CC_RESTRICTED;
        }

    case JOB_NECROMANCER:
        switch (speci)
        {
        case SP_DEEP_ELF:
        case SP_DEEP_DWARF:
        case SP_HILL_ORC:
        case SP_DEMONSPAWN:
        case SP_MUMMY:
        case SP_VAMPIRE:
            return CC_UNRESTRICTED;
        default:
            return CC_RESTRICTED;
        }

    case JOB_TRANSMUTER:
        switch (speci)
        {
        case SP_NAGA:
        case SP_MERFOLK:
        case SP_BASE_DRACONIAN:
        case SP_DEMIGOD:
        case SP_DEMONSPAWN:
        case SP_TROLL:
            return CC_UNRESTRICTED;
        default:
            return CC_RESTRICTED;
        }

    case JOB_FIRE_ELEMENTALIST:
        switch (speci)
        {
        case SP_HIGH_ELF:
        case SP_DEEP_ELF:
        case SP_HILL_ORC:
        case SP_NAGA:
        case SP_TENGU:
        case SP_DEMIGOD:
        case SP_GARGOYLE:
            return CC_UNRESTRICTED;
        default:
            return CC_RESTRICTED;
        }

    case JOB_ICE_ELEMENTALIST:
        switch (speci)
        {
        case SP_HIGH_ELF:
        case SP_DEEP_ELF:
        case SP_MERFOLK:
        case SP_NAGA:
        case SP_BASE_DRACONIAN:
        case SP_DEMIGOD:
        case SP_GARGOYLE:
            return CC_UNRESTRICTED;
        default:
            return CC_RESTRICTED;
        }

    case JOB_AIR_ELEMENTALIST:
        switch (speci)
        {
        case SP_HIGH_ELF:
        case SP_DEEP_ELF:
        case SP_TENGU:
        case SP_BASE_DRACONIAN:
        case SP_NAGA:
        case SP_VINE_STALKER:
            return CC_UNRESTRICTED;
        default:
            return CC_RESTRICTED;
        }

    case JOB_EARTH_ELEMENTALIST:
        switch (speci)
        {
        case SP_DEEP_ELF:
        case SP_DEEP_DWARF:
        case SP_SPRIGGAN:
        case SP_GARGOYLE:
        case SP_DEMIGOD:
        case SP_GHOUL:
        case SP_OCTOPODE:
            return CC_UNRESTRICTED;
        default:
            return CC_RESTRICTED;
        }

    case JOB_VENOM_MAGE:
        switch (speci)
        {
        case SP_DEEP_ELF:
        case SP_SPRIGGAN:
        case SP_NAGA:
        case SP_MERFOLK:
        case SP_TENGU:
        case SP_FELID:
        case SP_DEMONSPAWN:
            return CC_UNRESTRICTED;
        default:
            return CC_RESTRICTED;
        }

    case JOB_ASSASSIN:
        switch (speci)
        {
        case SP_TROLL:
        case SP_HALFLING:
        case SP_SPRIGGAN:
        case SP_DEMONSPAWN:
        case SP_VAMPIRE:
        case SP_VINE_STALKER:
            return CC_UNRESTRICTED;
        default:
            return CC_RESTRICTED;
        }

    case JOB_HUNTER:
        switch (speci)
        {
        case SP_HIGH_ELF:
        case SP_HILL_ORC:
        case SP_HALFLING:
        case SP_KOBOLD:
        case SP_OGRE:
        case SP_TROLL:
        case SP_CENTAUR:
            return CC_UNRESTRICTED;
        default:
            return CC_RESTRICTED;
        }

    case JOB_ARTIFICER:
        switch (speci)
        {
        case SP_DEEP_DWARF:
        case SP_HALFLING:
        case SP_KOBOLD:
        case SP_SPRIGGAN:
        case SP_BASE_DRACONIAN:
        case SP_DEMONSPAWN:
            return CC_UNRESTRICTED;
        default:
            return CC_RESTRICTED;
        }

    case JOB_ARCANE_MARKSMAN:
        switch (speci)
        {
        case SP_HIGH_ELF:
        case SP_DEEP_ELF:
        case SP_KOBOLD:
        case SP_SPRIGGAN:
        case SP_TROLL:
        case SP_CENTAUR:
            return CC_UNRESTRICTED;
        default:
            return CC_RESTRICTED;
        }
    case JOB_WANDERER:
        switch (speci)
        {
        case SP_HILL_ORC:
        case SP_SPRIGGAN:
        case SP_CENTAUR:
        case SP_MERFOLK:
        case SP_BASE_DRACONIAN:
        case SP_HUMAN:
        case SP_DEMONSPAWN:
            return CC_UNRESTRICTED;
        default:
            return CC_RESTRICTED;
        }

    default:
        return CC_BANNED;
    }
}

char_choice_restriction job_allowed(species_type speci, job_type job)
{
    if (!is_starting_species(speci) || !is_starting_job(job))
        return CC_BANNED;

    if (_banned_combination(job, speci))
        return CC_BANNED;

    if (species_recommends_job(speci, job))
        return CC_UNRESTRICTED;

    return CC_RESTRICTED;
}

bool is_good_combination(species_type spc, job_type job, bool species_first,
                         bool good)
{
    const char_choice_restriction restrict =
        species_first ? job_allowed(spc, job) : species_allowed(job, spc);

    if (good)
        return restrict == CC_UNRESTRICTED;

    return restrict != CC_BANNED;
}

// Is the given god restricted for the character defined by ng?
// Only uses ng.species and ng.job.
char_choice_restriction weapon_restriction(weapon_type wpn,
                                           const newgame_def &ng)
{
    ASSERT_RANGE(ng.species, 0, NUM_SPECIES);
    ASSERT_RANGE(ng.job, 0, NUM_JOBS);
    ASSERT(ng.species == SP_BASE_DRACONIAN
           || !species_is_draconian(ng.species));

    // Some special cases:

    if (ng.species == SP_FELID && wpn != WPN_UNARMED)
        return CC_BANNED;

    // Can't use them with a shield.
    if (ng.species == SP_SPRIGGAN && ng.job == JOB_FIGHTER
        && wpn == WPN_TRIDENT)
    {
        return CC_BANNED;
    }

    // These recommend short blades because they're good at stabbing,
    // but the fighter's armour hinders that.
    if ((ng.species == SP_NAGA || ng.species == SP_VAMPIRE)
         && ng.job == JOB_FIGHTER && wpn == WPN_RAPIER)
    {
        return CC_RESTRICTED;
    }

    if (wpn == WPN_QUARTERSTAFF && ng.job != JOB_GLADIATOR)
        return CC_BANNED;

    // Javelins are always good, tomahawks not so much.
    if (wpn == WPN_THROWN)
    {
        return species_size(ng.species) >= SIZE_MEDIUM ? CC_UNRESTRICTED
                                                       : CC_RESTRICTED;
    }

    if (species_recommends_weapon(ng.species, wpn))
        return CC_UNRESTRICTED;

    return CC_RESTRICTED;
}
