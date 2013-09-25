/**
 * @file
 * @brief Character choice restrictions.
 *
 * The functions in this file are "pure": They don't
 * access any global data.
**/

#include "AppHdr.h"
#include "ng-restr.h"

#include "newgame_def.h"
#include "species.h"
#include "jobs.h"

char_choice_restriction job_allowed(species_type speci, job_type job)
{
    switch (job)
    {
    case JOB_FIGHTER:
        switch (speci)
        {
        case SP_DEEP_ELF:
        case SP_SLUDGE_ELF:
        case SP_MERFOLK:
        case SP_KOBOLD:
        case SP_SPRIGGAN:
        case SP_NAGA:
        case SP_OGRE:
        case SP_BASE_DRACONIAN:
        case SP_DEMIGOD:
        case SP_MUMMY:
        case SP_VAMPIRE:
        case SP_OCTOPODE:
            return CC_RESTRICTED;
        default:
            return CC_UNRESTRICTED;
        }

    case JOB_GLADIATOR:
        switch (speci)
        {
        case SP_FELID:
            return CC_BANNED;
        case SP_DEEP_ELF:
        case SP_SLUDGE_ELF:
        case SP_HALFLING:
        case SP_KOBOLD:
        case SP_SPRIGGAN:
        case SP_NAGA:
        case SP_OGRE:
        case SP_BASE_DRACONIAN:
        case SP_DEMIGOD:
        case SP_MUMMY:
        case SP_VAMPIRE:
        case SP_OCTOPODE:
        case SP_DJINNI:
            return CC_RESTRICTED;
        default:
            return CC_UNRESTRICTED;
        }

    case JOB_MONK:
        switch (speci)
        {
        case SP_HIGH_ELF:
        case SP_DEEP_ELF:
        case SP_DEEP_DWARF:
        case SP_HALFLING:
        case SP_KOBOLD:
        case SP_SPRIGGAN:
        case SP_NAGA:
        case SP_OGRE:
        case SP_DEMIGOD:
        case SP_MUMMY:
        case SP_OCTOPODE:
        case SP_DJINNI:
            return CC_RESTRICTED;
        default:
            return CC_UNRESTRICTED;
        }

    case JOB_BERSERKER:
        switch (speci)
        {
        case SP_DEMIGOD:
            return CC_BANNED;
        case SP_HIGH_ELF:
        case SP_DEEP_ELF:
        case SP_SLUDGE_ELF:
        case SP_SPRIGGAN:
        case SP_MUMMY:
        case SP_GHOUL:
        case SP_VAMPIRE:
        case SP_OCTOPODE:
        case SP_DJINNI:
            return CC_RESTRICTED;
        default:
            return CC_UNRESTRICTED;
        }

    case JOB_CHAOS_KNIGHT:
        switch (speci)
        {
        case SP_DEMIGOD:
            return CC_BANNED;
        default:
            return CC_RESTRICTED;
        }

    case JOB_DEATH_KNIGHT:
        switch (speci)
        {
        case SP_DEMIGOD:
        case SP_GARGOYLE:
            return CC_BANNED;
        case SP_HIGH_ELF:
        case SP_DEEP_ELF:
        case SP_SLUDGE_ELF:
        case SP_DEEP_DWARF:
        case SP_HALFLING:
        case SP_KOBOLD:
        case SP_SPRIGGAN:
        case SP_OGRE:
        case SP_MUMMY:
        case SP_GHOUL:
        case SP_VAMPIRE:
        case SP_FELID:
        case SP_OCTOPODE:
            return CC_RESTRICTED;
        default:
            return CC_UNRESTRICTED;
        }

    case JOB_ABYSSAL_KNIGHT:
        switch (speci)
        {
        case SP_DEMIGOD:
            return CC_BANNED;
        case SP_HIGH_ELF:
        case SP_DEEP_ELF:
        case SP_SLUDGE_ELF:
        case SP_DEEP_DWARF:
        case SP_KOBOLD:
        case SP_OGRE:
        case SP_TROLL:
        case SP_MINOTAUR:
        case SP_TENGU:
        case SP_MUMMY:
        case SP_GHOUL:
        case SP_VAMPIRE:
        case SP_FELID:
        case SP_OCTOPODE:
        case SP_GARGOYLE:
            return CC_RESTRICTED;
        default:
            return CC_UNRESTRICTED;
        }

    case JOB_HEALER:
        switch (speci)
        {
        case SP_DEMIGOD:
        case SP_DEMONSPAWN:
        case SP_MUMMY:
        case SP_GHOUL:
        case SP_VAMPIRE:
            return CC_BANNED;
        case SP_HIGH_ELF:
        case SP_DEEP_ELF:
        case SP_HALFLING:
        case SP_SPRIGGAN:
        case SP_OGRE:
        case SP_MINOTAUR:
        case SP_TENGU:
        case SP_FELID:
        case SP_OCTOPODE:
            return CC_RESTRICTED;
        default:
            return CC_UNRESTRICTED;
        }

    case JOB_SKALD:
        switch (speci)
        {
        case SP_DEEP_ELF:
        case SP_SLUDGE_ELF:
        case SP_DEEP_DWARF:
        case SP_KOBOLD:
        case SP_SPRIGGAN:
        case SP_CENTAUR:
        case SP_OGRE:
        case SP_TROLL:
        case SP_MINOTAUR:
        case SP_TENGU:
        case SP_MUMMY:
        case SP_GHOUL:
        case SP_FELID:
        case SP_OCTOPODE:
        case SP_DJINNI:
            return CC_RESTRICTED;
        default:
            return CC_UNRESTRICTED;
        }

    case JOB_WARPER:
        switch (speci)
        {
        case SP_DEEP_ELF:
        case SP_SLUDGE_ELF:
        case SP_HILL_ORC:
        case SP_LAVA_ORC:
        case SP_MERFOLK:
        case SP_KOBOLD:
        case SP_CENTAUR:
        case SP_OGRE:
        case SP_TROLL:
        case SP_MINOTAUR:
        case SP_TENGU:
        case SP_BASE_DRACONIAN:
        case SP_DEMIGOD:
        case SP_MUMMY:
        case SP_GHOUL:
        case SP_VAMPIRE:
        case SP_OCTOPODE:
            return CC_RESTRICTED;
        default:
            return CC_UNRESTRICTED;
        }

    case JOB_WIZARD:
        switch (speci)
        {
        case SP_DEEP_DWARF:
        case SP_HILL_ORC:
        case SP_LAVA_ORC:
        case SP_MERFOLK:
        case SP_HALFLING:
        case SP_KOBOLD:
        case SP_CENTAUR:
        case SP_MINOTAUR:
        case SP_GHOUL:
        case SP_VAMPIRE:
        case SP_FELID:
            return CC_RESTRICTED;
        default:
            return CC_UNRESTRICTED;
        }

    case JOB_CONJURER:
        switch (speci)
        {
        case SP_SLUDGE_ELF:
        case SP_DEEP_DWARF:
        case SP_LAVA_ORC:
        case SP_MERFOLK:
        case SP_HALFLING:
        case SP_KOBOLD:
        case SP_CENTAUR:
        case SP_OGRE:
        case SP_TROLL:
        case SP_MINOTAUR:
        case SP_GHOUL:
        case SP_VAMPIRE:
            return CC_RESTRICTED;
        default:
            return CC_UNRESTRICTED;
        }

    case JOB_ENCHANTER:
        switch (speci)
        {
        case SP_HIGH_ELF:
        case SP_SLUDGE_ELF:
        case SP_DEEP_DWARF:
        case SP_HILL_ORC:
        case SP_LAVA_ORC:
        case SP_CENTAUR:
        case SP_OGRE:
        case SP_TROLL:
        case SP_MINOTAUR:
        case SP_TENGU:
        case SP_BASE_DRACONIAN:
        case SP_DEMIGOD:
        case SP_DEMONSPAWN:
        case SP_MUMMY:
        case SP_GHOUL:
        case SP_FELID:
        case SP_OCTOPODE:
            return CC_RESTRICTED;
        default:
            return CC_UNRESTRICTED;
        }

    case JOB_SUMMONER:
        switch (speci)
        {
        case SP_HIGH_ELF:
        case SP_DEEP_DWARF:
        case SP_HILL_ORC:
        case SP_LAVA_ORC:
        case SP_MERFOLK:
        case SP_HALFLING:
        case SP_SPRIGGAN:
        case SP_NAGA:
        case SP_CENTAUR:
        case SP_OGRE:
        case SP_TROLL:
        case SP_MINOTAUR:
        case SP_MUMMY:
        case SP_GHOUL:
            return CC_RESTRICTED;
        default:
            return CC_UNRESTRICTED;
        }

    case JOB_NECROMANCER:
        switch (speci)
        {
        case SP_HIGH_ELF:
        case SP_MERFOLK:
        case SP_HALFLING:
        case SP_CENTAUR:
        case SP_OGRE:
        case SP_TROLL:
        case SP_MINOTAUR:
        case SP_DJINNI:
        case SP_GARGOYLE:
            return CC_RESTRICTED;
        default:
            return CC_UNRESTRICTED;
        }

    case JOB_TRANSMUTER:
        switch (speci)
        {
        case SP_MUMMY:
        case SP_GHOUL:
            return CC_BANNED;
        case SP_HIGH_ELF:
        case SP_DEEP_ELF:
        case SP_DEEP_DWARF:
        case SP_HILL_ORC:
        case SP_HALFLING:
        case SP_KOBOLD:
        case SP_OGRE:
        case SP_TROLL:
        case SP_MINOTAUR:
        case SP_TENGU:
        case SP_VAMPIRE:
        case SP_DJINNI:
        case SP_GARGOYLE:
            return CC_RESTRICTED;
        default:
            return CC_UNRESTRICTED;
        }

    case JOB_FIRE_ELEMENTALIST:
        switch (speci)
        {
        case SP_DEEP_DWARF:
        case SP_MERFOLK:
        case SP_HALFLING:
        case SP_SPRIGGAN:
        case SP_CENTAUR:
        case SP_OGRE:
        case SP_TROLL:
        case SP_MINOTAUR:
        case SP_MUMMY:
        case SP_GHOUL:
        case SP_VAMPIRE:
        case SP_FELID:
            return CC_RESTRICTED;
        default:
            return CC_UNRESTRICTED;
        }

    case JOB_ICE_ELEMENTALIST:
        switch (speci)
        {
        case SP_DEEP_DWARF:
        case SP_HILL_ORC:
        case SP_LAVA_ORC:
        case SP_HALFLING:
        case SP_SPRIGGAN:
        case SP_CENTAUR:
        case SP_OGRE:
        case SP_TROLL:
        case SP_MINOTAUR:
        case SP_TENGU:
        case SP_FELID:
        case SP_DJINNI:
            return CC_RESTRICTED;
        default:
            return CC_UNRESTRICTED;
        }

    case JOB_AIR_ELEMENTALIST:
        switch (speci)
        {
        case SP_DEEP_DWARF:
        case SP_HILL_ORC:
        case SP_LAVA_ORC:
        case SP_MERFOLK:
        case SP_CENTAUR:
        case SP_OGRE:
        case SP_TROLL:
        case SP_MINOTAUR:
        case SP_MUMMY:
        case SP_GHOUL:
        case SP_VAMPIRE:
        case SP_FELID:
        case SP_GARGOYLE:
            return CC_RESTRICTED;
        default:
            return CC_UNRESTRICTED;
        }

    case JOB_EARTH_ELEMENTALIST:
        switch (speci)
        {
        case SP_HIGH_ELF:
        case SP_MERFOLK:
        case SP_HALFLING:
        case SP_CENTAUR:
        case SP_OGRE:
        case SP_TROLL:
        case SP_MINOTAUR:
        case SP_TENGU:
        case SP_MUMMY:
        case SP_VAMPIRE:
        case SP_FELID:
        case SP_DJINNI:
            return CC_RESTRICTED;
        default:
            return CC_UNRESTRICTED;
        }

    case JOB_VENOM_MAGE:
        switch (speci)
        {
        case SP_HIGH_ELF:
        case SP_DEEP_DWARF:
        case SP_HILL_ORC:
        case SP_LAVA_ORC:
        case SP_HALFLING:
        case SP_CENTAUR:
        case SP_OGRE:
        case SP_TROLL:
        case SP_MINOTAUR:
        case SP_MUMMY:
        case SP_GHOUL:
        case SP_VAMPIRE:
        case SP_DJINNI:
            return CC_RESTRICTED;
        default:
            return CC_UNRESTRICTED;
        }

    case JOB_ASSASSIN:
        switch (speci)
        {
        case SP_FELID:
            return CC_BANNED;
        case SP_HIGH_ELF:
        case SP_DEEP_ELF:
        case SP_DEEP_DWARF:
        case SP_HILL_ORC:
        case SP_LAVA_ORC:
        case SP_NAGA:
        case SP_CENTAUR:
        case SP_OGRE:
        case SP_TROLL:
        case SP_MINOTAUR:
        case SP_BASE_DRACONIAN:
        case SP_DEMIGOD:
        case SP_DEMONSPAWN:
        case SP_MUMMY:
        case SP_GHOUL:
        case SP_OCTOPODE:
        case SP_DJINNI:
            return CC_RESTRICTED;
        default:
            return CC_UNRESTRICTED;
        }

    case JOB_HUNTER:
        switch (speci)
        {
        case SP_FELID:
            return CC_BANNED;
        case SP_DEEP_ELF:
        case SP_HILL_ORC:
        case SP_LAVA_ORC:
        case SP_MERFOLK:
        case SP_SPRIGGAN:
        case SP_NAGA:
        case SP_TENGU:
        case SP_BASE_DRACONIAN:
        case SP_DEMIGOD:
        case SP_MUMMY:
        case SP_GHOUL:
        case SP_VAMPIRE:
        case SP_OCTOPODE:
            return CC_RESTRICTED;
        default:
            return CC_UNRESTRICTED;
        }

    case JOB_ARTIFICER:
        switch (speci)
        {
        case SP_FELID:
            return CC_BANNED;
        case SP_HIGH_ELF:
        case SP_DEEP_ELF:
        case SP_SLUDGE_ELF:
        case SP_NAGA:
        case SP_CENTAUR:
        case SP_OGRE:
        case SP_TROLL:
        case SP_MINOTAUR:
        case SP_TENGU:
        case SP_DEMIGOD:
        case SP_MUMMY:
        case SP_GHOUL:
        case SP_VAMPIRE:
        case SP_OCTOPODE:
            return CC_RESTRICTED;
        default:
            return CC_UNRESTRICTED;
        }

    case JOB_ARCANE_MARKSMAN:
        switch (speci)
        {
        case SP_FELID:
            return CC_BANNED;
        case SP_DEEP_DWARF:
        case SP_HILL_ORC:
        case SP_LAVA_ORC:
        case SP_MERFOLK:
        case SP_NAGA:
        case SP_TROLL:
        case SP_MINOTAUR:
        case SP_TENGU:
        case SP_BASE_DRACONIAN:
        case SP_DEMIGOD:
        case SP_MUMMY:
        case SP_GHOUL:
        case SP_OCTOPODE:
            return CC_RESTRICTED;
        default:
            return CC_UNRESTRICTED;
        }

    case JOB_WANDERER:
        return CC_RESTRICTED;

    default:
        return CC_BANNED;
    }
}

bool is_good_combination(species_type spc, job_type job, bool good)
{
    const char_choice_restriction restrict = job_allowed(spc, job);

    if (good)
        return (restrict == CC_UNRESTRICTED);

    return (restrict != CC_BANNED);
}

// Is the given god restricted for the character defined by ng?
// Only uses ng.species and ng.job.
char_choice_restriction weapon_restriction(weapon_type wpn,
                                           const newgame_def &ng)
{
    ASSERT(is_valid_species(ng.species));
    ASSERT(is_valid_job(ng.job));
    ASSERT(ng.species == SP_BASE_DRACONIAN
           || species_genus(ng.species) != GENPC_DRACONIAN);
    switch (wpn)
    {
    case WPN_UNARMED:
        if (species_has_claws(ng.species))
            return CC_UNRESTRICTED;
        return CC_BANNED;

    case WPN_SHORT_SWORD:
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
            // Sludge elves have bad aptitudes with short swords (-1), but
            // are still better with them than any other starting weapon.
        case SP_SLUDGE_ELF:
        case SP_HALFLING:
        case SP_KOBOLD:
        case SP_SPRIGGAN:
            return CC_UNRESTRICTED;

        default:
            return CC_RESTRICTED;
        }

        // Maces and hand axes usually share the same restrictions.
    case WPN_MACE:
        if (ng.species == SP_TROLL
            || ng.species == SP_OGRE
            || ng.species == SP_GARGOYLE
            || ng.species == SP_DJINNI)
        {
            return CC_UNRESTRICTED;
        }
        if (ng.species == SP_VAMPIRE)
            return CC_RESTRICTED;
        // else fall-through
    case WPN_HAND_AXE:
        switch (ng.species)
        {
        case SP_HUMAN:
        case SP_DEEP_DWARF:
        case SP_HILL_ORC:
        case SP_LAVA_ORC:
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
            return CC_UNRESTRICTED;

        default:
            return CC_RESTRICTED;
        }

    case WPN_SPEAR:
        switch (ng.species)
        {
        case SP_HUMAN:
        case SP_HILL_ORC:
        case SP_LAVA_ORC:
        case SP_MERFOLK:
        case SP_NAGA:
        case SP_OGRE:
        case SP_CENTAUR:
        case SP_MINOTAUR:
        case SP_TENGU:
        case SP_DEMIGOD:
        case SP_DEMONSPAWN:
        case SP_MUMMY:
        case SP_OCTOPODE:
        case SP_BASE_DRACONIAN:
        case SP_DJINNI:
            return CC_UNRESTRICTED;

        case SP_SPRIGGAN:
        case SP_HALFLING:
        case SP_KOBOLD:
            // Can't use them with a shield, terrible upgrade path;
            // jobs with other benefits like Warper, Skald or godlies
            // are better with two-handers.
            if (ng.job == JOB_FIGHTER || ng.job == JOB_GLADIATOR)
                return CC_BANNED;

        default:
            return CC_RESTRICTED;
        }
    case WPN_FALCHION:
        switch (ng.species)
        {
        case SP_HUMAN:
        case SP_HILL_ORC:
        case SP_LAVA_ORC:
        case SP_MERFOLK:
        case SP_NAGA:
        case SP_CENTAUR:
        case SP_MINOTAUR:
        case SP_HIGH_ELF:
        case SP_SLUDGE_ELF:
        case SP_DEEP_DWARF:
        case SP_TENGU:
        case SP_DEMIGOD:
        case SP_DEMONSPAWN:
        case SP_MUMMY:
        case SP_VAMPIRE:
        case SP_OCTOPODE:
        case SP_BASE_DRACONIAN:
        case SP_DJINNI:
            return CC_UNRESTRICTED;

        default:
            return CC_RESTRICTED;
        }

    case WPN_TRIDENT:
        if (ng.job != JOB_FIGHTER && ng.job != JOB_GLADIATOR
            || species_size(ng.species, PSIZE_BODY) < SIZE_MEDIUM)
        {
            return CC_BANNED;
        }

        // Tridents are strictly better than spears, so unrestrict them
        // for some species whose Polearm aptitudes are not too bad.
        switch (ng.species)
        {
        case SP_DEEP_DWARF:
        case SP_GHOUL:
        case SP_VAMPIRE:
            return CC_UNRESTRICTED;
        default:
            break;
        }

        // Both are polearms, right?
        return weapon_restriction(WPN_SPEAR, ng);

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
        case SP_SLUDGE_ELF:
        case SP_BASE_DRACONIAN:
        case SP_DJINNI:
        case SP_GARGOYLE:
            return CC_UNRESTRICTED;

        default:
            return CC_RESTRICTED;
        }

    case WPN_CROSSBOW:
        switch (ng.species)
        {
        case SP_DEEP_ELF:
        case SP_HIGH_ELF:
        case SP_HALFLING:
        case SP_MERFOLK:
        case SP_OGRE:
        case SP_HILL_ORC:
        case SP_LAVA_ORC:
        case SP_SPRIGGAN:
        case SP_TROLL:
        case SP_SLUDGE_ELF:
            return CC_RESTRICTED;
        case SP_FELID:
            return CC_BANNED;
        default:
            return CC_UNRESTRICTED;
        }

    case WPN_BOW:
        switch (ng.species)
        {
        case SP_DEEP_DWARF:
        case SP_KOBOLD:
        case SP_MERFOLK:
        case SP_OGRE:
        case SP_TROLL:
        case SP_HILL_ORC:
        case SP_LAVA_ORC:
        case SP_SLUDGE_ELF:
        case SP_DJINNI:
            return CC_RESTRICTED;
        case SP_FELID:
            return CC_BANNED;
        default:
            return CC_UNRESTRICTED;
        }

    case WPN_SLING:
        switch (ng.species)
        {
        case SP_DEEP_ELF:
        case SP_HIGH_ELF:
        case SP_SLUDGE_ELF:
        case SP_TENGU:
        case SP_MERFOLK:
        case SP_OGRE:
        case SP_HILL_ORC:
        case SP_LAVA_ORC:
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
        case SP_DJINNI:
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
