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

char_choice_restriction species_allowed(job_type job, species_type speci)
{
    if (!is_starting_species(speci) || !is_starting_job(job))
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
        case SP_FELID:
            return CC_BANNED;
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
        case SP_DEMIGOD:
            return CC_BANNED;
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
        case SP_DEMIGOD:
            return CC_BANNED;
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
        case SP_DEMIGOD:
            return CC_BANNED;
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
        case SP_MUMMY:
        case SP_GHOUL:
            return CC_BANNED;
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
        case SP_FELID:
            return CC_BANNED;
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
        case SP_FELID:
            return CC_BANNED;
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
        case SP_FELID:
            return CC_BANNED;
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

    switch (speci)
    {
    case SP_HUMAN:
        switch (job)
        {
            case JOB_BERSERKER:
            case JOB_CONJURER:
            case JOB_NECROMANCER:
            case JOB_FIRE_ELEMENTALIST:
            case JOB_ICE_ELEMENTALIST:
                return CC_UNRESTRICTED;
            default:
                return CC_RESTRICTED;
        }
    case SP_HIGH_ELF:
        switch (job)
        {
            case JOB_HUNTER:
            case JOB_SKALD:
            case JOB_WIZARD:
            case JOB_CONJURER:
            case JOB_FIRE_ELEMENTALIST:
            case JOB_ICE_ELEMENTALIST:
            case JOB_AIR_ELEMENTALIST:
                return CC_UNRESTRICTED;
            default:
                return CC_RESTRICTED;
        }
    case SP_DEEP_ELF:
        switch (job)
        {
            case JOB_WIZARD:
            case JOB_CONJURER:
            case JOB_SUMMONER:
            case JOB_NECROMANCER:
            case JOB_FIRE_ELEMENTALIST:
            case JOB_ICE_ELEMENTALIST:
            case JOB_AIR_ELEMENTALIST:
            case JOB_EARTH_ELEMENTALIST:
            case JOB_VENOM_MAGE:
                return CC_UNRESTRICTED;
            default:
                return CC_RESTRICTED;
        }
    case SP_HALFLING:
        switch (job)
        {
            case JOB_FIGHTER:
            case JOB_HUNTER:
            case JOB_ASSASSIN:
            case JOB_BERSERKER:
            case JOB_ENCHANTER:
            case JOB_AIR_ELEMENTALIST:
                return CC_UNRESTRICTED;
            default:
                return CC_RESTRICTED;
        }
    case SP_HILL_ORC:
        switch (job)
        {
            case JOB_FIGHTER:
            case JOB_GLADIATOR:
            case JOB_BERSERKER:
            case JOB_ABYSSAL_KNIGHT:
            case JOB_NECROMANCER:
            case JOB_FIRE_ELEMENTALIST:
                return CC_UNRESTRICTED;
            default:
                return CC_RESTRICTED;
        }
    case SP_KOBOLD:
        switch (job)
        {
            case JOB_HUNTER:
            case JOB_ASSASSIN:
            case JOB_BERSERKER:
            case JOB_ARCANE_MARKSMAN:
            case JOB_ENCHANTER:
            case JOB_FIRE_ELEMENTALIST:
            case JOB_ICE_ELEMENTALIST:
                return CC_UNRESTRICTED;
            default:
                return CC_RESTRICTED;
        }
    case SP_MUMMY:
        switch (job)
        {
            case JOB_TRANSMUTER:
                return CC_BANNED;
            case JOB_WIZARD:
            case JOB_CONJURER:
            case JOB_NECROMANCER:
            case JOB_ICE_ELEMENTALIST:
            case JOB_FIRE_ELEMENTALIST:
            case JOB_SUMMONER:
                return CC_UNRESTRICTED;
            default:
                return CC_RESTRICTED;
        }
    case SP_NAGA:
        switch (job)
        {
            case JOB_BERSERKER:
            case JOB_TRANSMUTER:
            case JOB_ENCHANTER:
            case JOB_FIRE_ELEMENTALIST:
            case JOB_ICE_ELEMENTALIST:
            case JOB_WARPER:
            case JOB_WIZARD:
            case JOB_VENOM_MAGE:
                return CC_UNRESTRICTED;
            default:
                return CC_RESTRICTED;
        }
    case SP_OGRE:
        switch (job)
        {
            case JOB_HUNTER:
            case JOB_BERSERKER:
            case JOB_ARCANE_MARKSMAN:
            case JOB_WIZARD:
            case JOB_FIRE_ELEMENTALIST:
                return CC_UNRESTRICTED;
            default:
                return CC_RESTRICTED;
        }
    case SP_TROLL:
        switch (job)
        {
            case JOB_FIGHTER:
            case JOB_MONK:
            case JOB_HUNTER:
            case JOB_BERSERKER:
            case JOB_WARPER:
            case JOB_EARTH_ELEMENTALIST:
            case JOB_WIZARD:
                return CC_UNRESTRICTED;
            default:
                return CC_RESTRICTED;
        }
    case SP_BASE_DRACONIAN:
        switch (job)
        {
            case JOB_BERSERKER:
            case JOB_TRANSMUTER:
            case JOB_CONJURER:
            case JOB_FIRE_ELEMENTALIST:
            case JOB_ICE_ELEMENTALIST:
            case JOB_AIR_ELEMENTALIST:
            case JOB_EARTH_ELEMENTALIST:
            case JOB_VENOM_MAGE:
                return CC_UNRESTRICTED;
            default:
                return CC_RESTRICTED;
        }
    case SP_CENTAUR:
        switch (job)
        {
            case JOB_FIGHTER:
            case JOB_GLADIATOR:
            case JOB_HUNTER:
            case JOB_WARPER:
            case JOB_ARCANE_MARKSMAN:
                return CC_UNRESTRICTED;
            default:
                return CC_RESTRICTED;
        }
    case SP_DEMIGOD:
        switch (job)
        {
            case JOB_BERSERKER:
            case JOB_ABYSSAL_KNIGHT:
            case JOB_CHAOS_KNIGHT:
                return CC_BANNED;
            case JOB_TRANSMUTER:
            case JOB_CONJURER:
            case JOB_FIRE_ELEMENTALIST:
            case JOB_ICE_ELEMENTALIST:
            case JOB_AIR_ELEMENTALIST:
            case JOB_EARTH_ELEMENTALIST:
                return CC_UNRESTRICTED;
            default:
                return CC_RESTRICTED;
        }
    case SP_SPRIGGAN:
        switch (job)
        {
            case JOB_ASSASSIN:
            case JOB_ARTIFICER:
            case JOB_ABYSSAL_KNIGHT:
            case JOB_WARPER:
            case JOB_ENCHANTER:
            case JOB_CONJURER:
            case JOB_EARTH_ELEMENTALIST:
            case JOB_VENOM_MAGE:
                return CC_UNRESTRICTED;
            default:
                return CC_RESTRICTED;
        }
    case SP_MINOTAUR:
        switch (job)
        {
            case JOB_FIGHTER:
            case JOB_GLADIATOR:
            case JOB_MONK:
            case JOB_HUNTER:
            case JOB_BERSERKER:
                return CC_UNRESTRICTED;
            default:
                return CC_RESTRICTED;
        }
    case SP_DEMONSPAWN:
        switch (job)
        {
            case JOB_GLADIATOR:
            case JOB_BERSERKER:
            case JOB_ABYSSAL_KNIGHT:
            case JOB_WIZARD:
            case JOB_NECROMANCER:
            case JOB_FIRE_ELEMENTALIST:
            case JOB_ICE_ELEMENTALIST:
            case JOB_VENOM_MAGE:
                return CC_UNRESTRICTED;
            default:
                return CC_RESTRICTED;
        }
    case SP_GHOUL:
        switch (job)
        {
            case JOB_TRANSMUTER:
                return CC_BANNED;
            case JOB_WARPER:
            case JOB_GLADIATOR:
            case JOB_MONK:
            case JOB_NECROMANCER:
            case JOB_ICE_ELEMENTALIST:
            case JOB_EARTH_ELEMENTALIST:
                return CC_UNRESTRICTED;
            default:
                return CC_RESTRICTED;
        }
    case SP_TENGU:
        switch (job)
        {
            case JOB_BERSERKER:
            case JOB_WIZARD:
            case JOB_CONJURER:
            case JOB_SUMMONER:
            case JOB_FIRE_ELEMENTALIST:
            case JOB_AIR_ELEMENTALIST:
            case JOB_VENOM_MAGE:
                return CC_UNRESTRICTED;
            default:
                return CC_RESTRICTED;
        }
    case SP_MERFOLK:
        switch (job)
        {
            case JOB_GLADIATOR:
            case JOB_BERSERKER:
            case JOB_SKALD:
            case JOB_TRANSMUTER:
            case JOB_SUMMONER:
            case JOB_ICE_ELEMENTALIST:
            case JOB_VENOM_MAGE:
                return CC_UNRESTRICTED;
            default:
                return CC_RESTRICTED;
        }
    case SP_VAMPIRE:
        switch (job)
        {
            case JOB_MONK:
            case JOB_ASSASSIN:
            case JOB_ENCHANTER:
            case JOB_EARTH_ELEMENTALIST:
            case JOB_NECROMANCER:
            case JOB_ICE_ELEMENTALIST:
                return CC_UNRESTRICTED;
            default:
                return CC_RESTRICTED;
        }
    case SP_DEEP_DWARF:
        switch (job)
        {
            case JOB_FIGHTER:
            case JOB_HUNTER:
            case JOB_BERSERKER:
            case JOB_NECROMANCER:
            case JOB_EARTH_ELEMENTALIST:
                return CC_UNRESTRICTED;
            default:
                return CC_RESTRICTED;
        }
    case SP_FELID:
        switch (job)
        {
            case JOB_GLADIATOR:
            case JOB_HUNTER:
            case JOB_ASSASSIN:
            case JOB_ARCANE_MARKSMAN:
                return CC_BANNED;
            case JOB_BERSERKER:
            case JOB_ENCHANTER:
            case JOB_TRANSMUTER:
            case JOB_ICE_ELEMENTALIST:
            case JOB_CONJURER:
            case JOB_SUMMONER:
            case JOB_AIR_ELEMENTALIST:
            case JOB_VENOM_MAGE:
                return CC_UNRESTRICTED;
            default:
                return CC_RESTRICTED;
        }
    case SP_OCTOPODE:
        switch (job)
        {
            case JOB_TRANSMUTER:
            case JOB_WIZARD:
            case JOB_CONJURER:
            case JOB_ASSASSIN:
            case JOB_FIRE_ELEMENTALIST:
            case JOB_ICE_ELEMENTALIST:
            case JOB_EARTH_ELEMENTALIST:
            case JOB_VENOM_MAGE:
                return CC_UNRESTRICTED;
            default:
                return CC_RESTRICTED;
        }
#if TAG_MAJOR_VERSION == 34
    case SP_LAVA_ORC:
        switch (job)
        {
            case JOB_FIGHTER:
            case JOB_GLADIATOR:
            case JOB_BERSERKER:
            case JOB_ABYSSAL_KNIGHT:
            case JOB_NECROMANCER:
            case JOB_FIRE_ELEMENTALIST:
                return CC_UNRESTRICTED;
            default:
                return CC_RESTRICTED;
        }
#endif
    case SP_GARGOYLE:
        switch (job)
        {
            case JOB_FIGHTER:
            case JOB_GLADIATOR:
            case JOB_MONK:
            case JOB_BERSERKER:
            case JOB_FIRE_ELEMENTALIST:
            case JOB_ICE_ELEMENTALIST:
            case JOB_EARTH_ELEMENTALIST:
            case JOB_VENOM_MAGE:
                return CC_UNRESTRICTED;
            default:
                return CC_RESTRICTED;
        }
    case SP_FORMICID:
        switch (job)
        {
            case JOB_FIGHTER:
            case JOB_HUNTER:
            case JOB_ABYSSAL_KNIGHT:
            case JOB_ARCANE_MARKSMAN:
            case JOB_EARTH_ELEMENTALIST:
            case JOB_VENOM_MAGE:
                return CC_UNRESTRICTED;
            default:
                return CC_RESTRICTED;
        }
    case SP_VINE_STALKER:
        switch (job)
        {
            case JOB_MONK:
            case JOB_ASSASSIN:
            case JOB_ENCHANTER:
            case JOB_CONJURER:
            case JOB_NECROMANCER:
            case JOB_AIR_ELEMENTALIST:
            case JOB_ICE_ELEMENTALIST:
                return CC_UNRESTRICTED;
            default:
                return CC_RESTRICTED;
        }
    default:
        return CC_BANNED;
    }
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
           || species_genus(ng.species) != GENPC_DRACONIAN);
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

    // intentional fall-through
    default:
        return CC_BANNED;
    }
}
