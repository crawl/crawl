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
    switch (wpn)
    {
    case WPN_UNARMED:
        if (species_has_claws(ng.species))
            return CC_UNRESTRICTED;
        return CC_RESTRICTED;

    case WPN_SHORT_SWORD:
    case WPN_RAPIER:
        switch (ng.species)
        {
        case SP_NAGA:
        case SP_VAMPIRE:
            // The fighter's heavy armour hinders stabbing.
            if (ng.job == JOB_FIGHTER)
                return CC_RESTRICTED;
            // else fall through
        case SP_HIGH_ELF:
        case SP_DEEP_ELF:
        case SP_HALFLING:
        case SP_KOBOLD:
        case SP_SPRIGGAN:
        case SP_FORMICID:
            return CC_UNRESTRICTED;

        default:
            return CC_RESTRICTED;
        }

        // Maces and axes usually share the same restrictions.
    case WPN_MACE:
    case WPN_FLAIL:
        if (ng.species == SP_TROLL
            || ng.species == SP_OGRE
            || ng.species == SP_GARGOYLE)
        {
            return CC_UNRESTRICTED;
        }
        if (ng.species == SP_VAMPIRE)
            return CC_RESTRICTED;
        // else fall-through
    case WPN_HAND_AXE:
    case WPN_WAR_AXE:
        switch (ng.species)
        {
        case SP_HUMAN:
        case SP_DEEP_DWARF:
        case SP_HILL_ORC:
#if TAG_MAJOR_VERSION == 34
        case SP_LAVA_ORC:
#endif
        case SP_MUMMY:
        case SP_CENTAUR:
        case SP_NAGA:
        case SP_MINOTAUR:
        case SP_TENGU:
        case SP_DEMIGOD:
        case SP_DEMONSPAWN:
        case SP_VAMPIRE:
        case SP_OCTOPODE:
        case SP_BASE_DRACONIAN:
        case SP_FORMICID:
        case SP_VINE_STALKER:
            return CC_UNRESTRICTED;

        default:
            return CC_RESTRICTED;
        }

    case WPN_SPEAR:
    case WPN_TRIDENT:
        switch (ng.species)
        {
        case SP_HUMAN:
        case SP_HILL_ORC:
#if TAG_MAJOR_VERSION == 34
        case SP_LAVA_ORC:
#endif
        case SP_MERFOLK:
        case SP_NAGA:
        case SP_CENTAUR:
        case SP_MINOTAUR:
        case SP_TENGU:
        case SP_DEMIGOD:
        case SP_DEMONSPAWN:
        case SP_MUMMY:
        case SP_OCTOPODE:
        case SP_BASE_DRACONIAN:
        case SP_FORMICID:
        case SP_VINE_STALKER:
            return CC_UNRESTRICTED;

        case SP_SPRIGGAN:
            // Can't use them with a shield.
            if (ng.job == JOB_FIGHTER)
                return CC_BANNED;

        default:
            return CC_RESTRICTED;
        }
    case WPN_FALCHION:
    case WPN_LONG_SWORD:
        switch (ng.species)
        {
        case SP_HUMAN:
        case SP_HILL_ORC:
#if TAG_MAJOR_VERSION == 34
        case SP_LAVA_ORC:
#endif
        case SP_MERFOLK:
        case SP_NAGA:
        case SP_CENTAUR:
        case SP_MINOTAUR:
        case SP_HIGH_ELF:
        case SP_DEEP_DWARF:
        case SP_TENGU:
        case SP_DEMIGOD:
        case SP_DEMONSPAWN:
        case SP_MUMMY:
        case SP_VAMPIRE:
        case SP_OCTOPODE:
        case SP_BASE_DRACONIAN:
        case SP_FORMICID:
        case SP_VINE_STALKER:
        case SP_HALFLING:
            return CC_UNRESTRICTED;

        default:
            return CC_RESTRICTED;
        }

    case WPN_QUARTERSTAFF:
        if (ng.job != JOB_GLADIATOR)
            return CC_BANNED;
        switch (ng.species)
        {
        case SP_CENTAUR:
        case SP_DEEP_ELF:
        case SP_DEMIGOD:
        case SP_DEMONSPAWN:
        case SP_HIGH_ELF:
        case SP_HUMAN:
        case SP_TENGU:
        case SP_KOBOLD:
        case SP_MINOTAUR:
        case SP_MUMMY:
        case SP_OCTOPODE:
        case SP_BASE_DRACONIAN:
        case SP_FORMICID:
        case SP_GARGOYLE:
        case SP_VINE_STALKER:
            return CC_UNRESTRICTED;

        default:
            return CC_RESTRICTED;
        }

    case WPN_HAND_CROSSBOW:
        switch (ng.species)
        {
        case SP_DEEP_ELF:
        case SP_HIGH_ELF:
        case SP_HALFLING:
        case SP_MERFOLK:
        case SP_OGRE:
        case SP_HILL_ORC:
#if TAG_MAJOR_VERSION == 34
        case SP_LAVA_ORC:
#endif
        case SP_SPRIGGAN:
        case SP_TROLL:
            return CC_RESTRICTED;
        case SP_FELID:
            return CC_BANNED;
        default:
            return CC_UNRESTRICTED;
        }

    case WPN_SHORTBOW:
        switch (ng.species)
        {
        case SP_DEEP_DWARF:
        case SP_KOBOLD:
        case SP_MERFOLK:
        case SP_OGRE:
        case SP_TROLL:
        case SP_HILL_ORC:
#if TAG_MAJOR_VERSION == 34
        case SP_LAVA_ORC:
#endif
        case SP_FORMICID:
        case SP_SPRIGGAN:
            return CC_RESTRICTED;
        case SP_FELID:
            return CC_BANNED;
        default:
            return CC_UNRESTRICTED;
        }

    case WPN_HUNTING_SLING:
        switch (ng.species)
        {
        case SP_DEEP_ELF:
        case SP_HIGH_ELF:
        case SP_TENGU:
        case SP_MERFOLK:
        case SP_OGRE:
        case SP_HILL_ORC:
#if TAG_MAJOR_VERSION == 34
        case SP_LAVA_ORC:
#endif
        case SP_TROLL:
        case SP_GARGOYLE:
            return CC_RESTRICTED;
        case SP_FELID:
            return CC_BANNED;
        default:
            return CC_UNRESTRICTED;
        }

    case WPN_THROWN:
        switch (ng.species)
        {
        case SP_DEEP_DWARF:
            return CC_RESTRICTED;
        case SP_FELID:
            return CC_BANNED;
        default:
            return CC_UNRESTRICTED;
        }

    default:
        return CC_BANNED;
    }
}
