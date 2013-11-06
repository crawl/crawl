#include "AppHdr.h"

#include "species.h"

#include "libutil.h"
#include "random.h"

#include "version.h"

// March 2008: change order of species and jobs on character selection
// screen as suggested by Markus Maier. Summarizing comments below are
// copied directly from Markus' SourceForge comments. (jpeg)
//
// These are listed in two columns to match the selection screen output.
// Take care to list all valid species here, or they cannot be directly
// chosen.
//
// Fantasy staples and humanoid creatures come first, then diminutive and
// stealthy creatures, then monstrous creatures, then planetouched and after
// all living creatures finally the undead. (MM)
static species_type species_order[] =
{
    // comparatively human-like looks
    SP_HUMAN,          SP_HIGH_ELF,
    SP_DEEP_ELF,       SP_DEEP_DWARF,
    SP_HILL_ORC,       SP_LAVA_ORC,
    SP_MERFOLK,        SP_FORMICID,
    // small species
    SP_HALFLING,       SP_KOBOLD,
    SP_SPRIGGAN,
    // significantly different body type from human
    SP_NAGA,           SP_CENTAUR,
    SP_OGRE,           SP_TROLL,
    SP_MINOTAUR,       SP_TENGU,
    SP_BASE_DRACONIAN, SP_GARGOYLE,
    // celestial species
    SP_DEMIGOD,        SP_DEMONSPAWN,
    SP_DJINNI,
    // undead species
    SP_MUMMY,          SP_GHOUL,
    SP_VAMPIRE,
    // not humanoid at all
    SP_FELID,          SP_OCTOPODE,
};

species_type random_draconian_player_species()
{
    const int num_drac = SP_PALE_DRACONIAN - SP_RED_DRACONIAN + 1;
    return static_cast<species_type>(SP_RED_DRACONIAN + random2(num_drac));
}

species_type get_species(const int index)
{
    if (index < 0 || index >= ng_num_species())
        return SP_UNKNOWN;

    return species_order[index];
}

static const char * Species_Abbrev_List[NUM_SPECIES] =
{
      "Hu", "HE", "DE",
#if TAG_MAJOR_VERSION == 34
      "SE",
#endif
      "Ha", "HO", "Ko", "Mu", "Na", "Og", "Tr",
      // the draconians
      "Dr", "Dr", "Dr", "Dr", "Dr", "Dr", "Dr", "Dr", "Dr", "Dr",
      "Ce", "Dg", "Sp", "Mi", "Ds", "Gh", "Te", "Mf", "Vp", "DD",
      "Fe", "Op", "Dj", "LO", "Gr", "Fo",
      // placeholders
      "El", "HD", "OM", "GE", "Gn", "MD",
#if TAG_MAJOR_VERSION > 34
      "SE",
#endif
};

const char *get_species_abbrev(species_type which_species)
{
    ASSERT_RANGE(which_species, 0, NUM_SPECIES);

    return (Species_Abbrev_List[which_species]);
}

// Needed for debug.cc and hiscores.cc.
species_type get_species_by_abbrev(const char *abbrev)
{
    int i;
    COMPILE_CHECK(ARRAYSZ(Species_Abbrev_List) == NUM_SPECIES);
    for (i = 0; i < NUM_SPECIES; i++)
    {
        // This assumes untranslated abbreviations.
        if (toalower(abbrev[0]) == toalower(Species_Abbrev_List[i][0])
            && toalower(abbrev[1]) == toalower(Species_Abbrev_List[i][1]))
        {
            break;
        }
    }

    return ((i < NUM_SPECIES) ? static_cast<species_type>(i) : SP_UNKNOWN);
}

int ng_num_species()
{
    // The list musn't be longer than the number of actual species.
    COMPILE_CHECK(ARRAYSZ(species_order) <= NUM_SPECIES);
    return ARRAYSZ(species_order);
}

// Does a case-sensitive lookup of the species name supplied.
species_type str_to_species(const string &species)
{
    species_type sp;
    if (species.empty())
        return SP_UNKNOWN;

    for (int i = 0; i < NUM_SPECIES; ++i)
    {
        sp = static_cast<species_type>(i);
        if (species == species_name(sp))
            return sp;
    }

    return SP_UNKNOWN;
}

string species_name(species_type speci, bool genus, bool adj)
// defaults:                                 false       false
{
    string res;

    switch (species_genus(speci))
    {
    case GENPC_DRACONIAN:
        if (adj || genus)  // adj doesn't care about exact species
            res = "Draconian";
        else
        {
            switch (speci)
            {
            case SP_RED_DRACONIAN:     res = "Red Draconian";     break;
            case SP_WHITE_DRACONIAN:   res = "White Draconian";   break;
            case SP_GREEN_DRACONIAN:   res = "Green Draconian";   break;
            case SP_YELLOW_DRACONIAN:  res = "Yellow Draconian";  break;
            case SP_GREY_DRACONIAN:    res = "Grey Draconian";    break;
            case SP_BLACK_DRACONIAN:   res = "Black Draconian";   break;
            case SP_PURPLE_DRACONIAN:  res = "Purple Draconian";  break;
            case SP_MOTTLED_DRACONIAN: res = "Mottled Draconian"; break;
            case SP_PALE_DRACONIAN:    res = "Pale Draconian";    break;

            case SP_BASE_DRACONIAN:
            default:
                res = "Draconian";
                break;
            }
        }
        break;
    case GENPC_ELVEN:
        if (adj)  // doesn't care about species/genus
            res = "Elven";
        else if (genus)
            res = "Elf";
        else
        {
            switch (speci)
            {
            case SP_HIGH_ELF:   res = "High Elf";   break;
            case SP_DEEP_ELF:   res = "Deep Elf";   break;
#if TAG_MAJOR_VERSION == 34
            case SP_SLUDGE_ELF: res = "Sludge Elf"; break;
#endif
            default:            res = "Elf";        break;
            }
        }
        break;
    case GENPC_ORCISH:
        if (adj)  // doesn't care about species/genus
            res = "Orcish";
        else if (genus)
            res = "Orc";
        else
        {
            switch (speci)
            {
            case SP_HILL_ORC: res = "Hill Orc"; break;
            case SP_LAVA_ORC: res = "Lava Orc"; break;
            default:          res = "Orc";      break;
            }
        }
        break;
    case GENPC_NONE:
    default:
        switch (speci)
        {
        case SP_HUMAN:      res = "Human";                             break;
        case SP_HALFLING:   res = "Halfling";                          break;
        case SP_KOBOLD:     res = "Kobold";                            break;
        case SP_MUMMY:      res = "Mummy";                             break;
        case SP_NAGA:       res = "Naga";                              break;
        case SP_CENTAUR:    res = "Centaur";                           break;
        case SP_SPRIGGAN:   res = "Spriggan";                          break;
        case SP_MINOTAUR:   res = "Minotaur";                          break;
        case SP_TENGU:      res = "Tengu";                             break;
        case SP_GARGOYLE:   res = "Gargoyle";                          break;
        case SP_FORMICID:   res = "Formicid";                          break;

        case SP_DEEP_DWARF:
            res = (adj ? "Dwarven" : genus ? "Dwarf" : "Deep Dwarf");
            break;
        case SP_FELID:
            res = (adj ? "Feline" : genus ? "Cat" : "Felid");
            break;
        case SP_OCTOPODE:
            res = (adj ? "Octopoid" : genus ? "Octopus" : "Octopode");
            break;

        case SP_OGRE:       res = (adj ? "Ogreish"    : "Ogre");       break;
        case SP_TROLL:      res = (adj ? "Trollish"   : "Troll");      break;
        case SP_DEMIGOD:    res = (adj ? "Divine"     : "Demigod");    break;
        case SP_DEMONSPAWN: res = (adj ? "Demonic"    : "Demonspawn"); break;
        case SP_GHOUL:      res = (adj ? "Ghoulish"   : "Ghoul");      break;
        case SP_MERFOLK:    res = (adj ? "Merfolkian" : "Merfolk");    break;
        case SP_VAMPIRE:    res = (adj ? "Vampiric"   : "Vampire");    break;
        case SP_DJINNI:     res = (adj ? "Djinn"      : "Djinni");     break;
        default:            res = (adj ? "Yakish"     : "Yak");        break;
        }
    }
    return res;
}

int species_has_claws(species_type species, bool mut_level)
{
    if (species == SP_TROLL)
        return 3;

    if (species == SP_GHOUL)
        return 1;

    // Felid claws don't count as a claws mutation.  The claws mutation
    // does only hands, not paws.
    if (species == SP_FELID && !mut_level)
        return 1;

    return 0;
}

bool species_likes_water(species_type species)
{
    return (species == SP_MERFOLK || species == SP_GREY_DRACONIAN
            || species == SP_OCTOPODE);
}

bool species_likes_lava(species_type species)
{
    return (species == SP_LAVA_ORC);
}

bool species_can_throw_large_rocks(species_type species)
{
    return (species == SP_OGRE
            || species == SP_TROLL
            || species == SP_FORMICID);
}

genus_type species_genus(species_type species)
{
    switch (species)
    {
    case SP_RED_DRACONIAN:
    case SP_WHITE_DRACONIAN:
    case SP_GREEN_DRACONIAN:
    case SP_YELLOW_DRACONIAN:
    case SP_GREY_DRACONIAN:
    case SP_BLACK_DRACONIAN:
    case SP_PURPLE_DRACONIAN:
    case SP_MOTTLED_DRACONIAN:
    case SP_PALE_DRACONIAN:
    case SP_BASE_DRACONIAN:
        return GENPC_DRACONIAN;

    case SP_HIGH_ELF:
    case SP_DEEP_ELF:
    case SP_SLUDGE_ELF:
        return GENPC_ELVEN;

    case SP_HILL_ORC:
    case SP_LAVA_ORC:
        return GENPC_ORCISH;

    default:
        return GENPC_NONE;
    }
}

size_type species_size(species_type species, size_part_type psize)
{
    switch (species)
    {
    case SP_OGRE:
    case SP_TROLL:
        return SIZE_LARGE;
    case SP_NAGA:
    case SP_CENTAUR:
        return ((psize == PSIZE_TORSO) ? SIZE_MEDIUM : SIZE_LARGE);
    case SP_HALFLING:
    case SP_KOBOLD:
        return SIZE_SMALL;
    case SP_SPRIGGAN:
    case SP_FELID:
        return SIZE_LITTLE;

    default:
        return SIZE_MEDIUM;
    }
}

monster_type player_species_to_mons_species(species_type species)
{
    switch (species)
    {
    case SP_HUMAN:
        return MONS_HUMAN;
    case SP_HIGH_ELF:
    case SP_DEEP_ELF:
    case SP_SLUDGE_ELF:
        return MONS_ELF;
    case SP_HALFLING:
        return MONS_HALFLING;
    case SP_HILL_ORC:
        return MONS_ORC;
    case SP_LAVA_ORC:
        return MONS_LAVA_ORC;
    case SP_KOBOLD:
        return MONS_KOBOLD;
    case SP_MUMMY:
        return MONS_MUMMY;
    case SP_NAGA:
        return MONS_NAGA;
    case SP_OGRE:
        return MONS_OGRE;
    case SP_TROLL:
        return MONS_TROLL;
    case SP_RED_DRACONIAN:
        return MONS_RED_DRACONIAN;
    case SP_WHITE_DRACONIAN:
        return MONS_WHITE_DRACONIAN;
    case SP_GREEN_DRACONIAN:
        return MONS_GREEN_DRACONIAN;
    case SP_YELLOW_DRACONIAN:
        return MONS_YELLOW_DRACONIAN;
    case SP_GREY_DRACONIAN:
        return MONS_GREY_DRACONIAN;
    case SP_BLACK_DRACONIAN:
        return MONS_BLACK_DRACONIAN;
    case SP_PURPLE_DRACONIAN:
        return MONS_PURPLE_DRACONIAN;
    case SP_MOTTLED_DRACONIAN:
        return MONS_MOTTLED_DRACONIAN;
    case SP_PALE_DRACONIAN:
        return MONS_PALE_DRACONIAN;
    case SP_BASE_DRACONIAN:
        return MONS_DRACONIAN;
    case SP_CENTAUR:
        return MONS_CENTAUR;
    case SP_DEMIGOD:
        return MONS_DEMIGOD;
    case SP_SPRIGGAN:
        return MONS_SPRIGGAN;
    case SP_MINOTAUR:
        return MONS_MINOTAUR;
    case SP_DEMONSPAWN:
        return MONS_DEMONSPAWN;
    case SP_GARGOYLE:
        return MONS_GARGOYLE;
    case SP_GHOUL:
        return MONS_GHOUL;
    case SP_TENGU:
        return MONS_TENGU;
    case SP_MERFOLK:
        return MONS_MERFOLK;
    case SP_VAMPIRE:
        return MONS_VAMPIRE;
    case SP_DEEP_DWARF:
        return MONS_DEEP_DWARF;
    case SP_FELID:
        return MONS_FELID;
    case SP_OCTOPODE:
        return MONS_OCTOPODE;
    case SP_DJINNI:
        return MONS_DJINNI;
    case SP_FORMICID:
        return MONS_FORMICID;
    case SP_ELF:
    case SP_HILL_DWARF:
    case SP_MOUNTAIN_DWARF:
    case SP_OGRE_MAGE:
    case SP_GREY_ELF:
    case SP_GNOME:
    case NUM_SPECIES:
    case SP_UNKNOWN:
    case SP_RANDOM:
    case SP_VIABLE:
        die("player of an invalid species");
    default:
        return MONS_PROGRAM_BUG;
    }
}

bool is_valid_species(species_type species)
{
    return (species >= 0 && species <= LAST_VALID_SPECIES);
}

bool is_species_valid_choice(species_type species)
{
#if TAG_MAJOR_VERSION == 34
    if (species == SP_SLUDGE_ELF)
        return false;
#endif
    if ((species == SP_LAVA_ORC || species == SP_DJINNI)
        && Version::ReleaseType != VER_ALPHA)
    {
        return false;
    }

    // Non-base draconians cannot be selected either.
    return is_valid_species(species)
        && !(species >= SP_RED_DRACONIAN && species < SP_BASE_DRACONIAN);
}

int species_exp_modifier(species_type species)
{
    switch (species) // table: Experience
    {
    case SP_HUMAN:
    case SP_HALFLING:
    case SP_KOBOLD:
    case SP_FORMICID:
        return 1;
    case SP_HILL_ORC:
    case SP_OGRE:
    case SP_SLUDGE_ELF:
    case SP_NAGA:
    case SP_GHOUL:
    case SP_MERFOLK:
    case SP_OCTOPODE:
    case SP_TENGU:
    case SP_GARGOYLE:
        return 0;
    case SP_SPRIGGAN:
    case SP_DEEP_DWARF:
    case SP_MINOTAUR:
    case SP_BASE_DRACONIAN:
    case SP_RED_DRACONIAN:
    case SP_WHITE_DRACONIAN:
    case SP_GREEN_DRACONIAN:
    case SP_YELLOW_DRACONIAN:
    case SP_GREY_DRACONIAN:
    case SP_BLACK_DRACONIAN:
    case SP_PURPLE_DRACONIAN:
    case SP_MOTTLED_DRACONIAN:
    case SP_PALE_DRACONIAN:
    case SP_DEEP_ELF:
    case SP_CENTAUR:
    case SP_MUMMY:
    case SP_FELID:
    case SP_HIGH_ELF:
    case SP_VAMPIRE:
    case SP_TROLL:
    case SP_DEMONSPAWN:
    case SP_DJINNI:
    case SP_LAVA_ORC:
        return -1;
    case SP_DEMIGOD:
        return -2;
    default:
        return 0;
    }
}

int species_hp_modifier(species_type species)
{
    switch (species) // table: Hit Points
    {
    case SP_FELID:
        return -4;
    case SP_SPRIGGAN:
        return -3;
    case SP_DEEP_ELF:
    case SP_TENGU:
    case SP_KOBOLD:
    case SP_FORMICID:
    case SP_GARGOYLE:
        return -2;
    case SP_HIGH_ELF:
    case SP_SLUDGE_ELF:
    case SP_HALFLING:
    case SP_OCTOPODE:
    case SP_DJINNI:
        return -1;
    default:
        return 0;
    case SP_CENTAUR:
    case SP_DEMIGOD:
    case SP_BASE_DRACONIAN:
    case SP_RED_DRACONIAN:
    case SP_WHITE_DRACONIAN:
    case SP_GREEN_DRACONIAN:
    case SP_YELLOW_DRACONIAN:
    case SP_GREY_DRACONIAN:
    case SP_BLACK_DRACONIAN:
    case SP_PURPLE_DRACONIAN:
    case SP_MOTTLED_DRACONIAN:
    case SP_PALE_DRACONIAN:
    case SP_GHOUL:
    case SP_HILL_ORC:
    case SP_LAVA_ORC:
    case SP_MINOTAUR:
        return 1;
    case SP_DEEP_DWARF:
    case SP_NAGA:
        return 2;
    case SP_OGRE:
    case SP_TROLL:
        return 3;
    }
}

int species_mp_modifier(species_type species)
{
    switch (species) // table: Magic Points
    {
    case SP_TROLL:
    case SP_MINOTAUR:
        return -2;
    case SP_CENTAUR:
    case SP_GHOUL:
        return -1;
    default:
        return 0;
    case SP_SLUDGE_ELF:
    case SP_TENGU:
        return 1;
    case SP_FELID:
    case SP_HIGH_ELF:
    case SP_DEMIGOD:
        return 2;
    case SP_DEEP_ELF:
    case SP_SPRIGGAN:
        return 3;
    }
}
