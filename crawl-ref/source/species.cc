#include "AppHdr.h"

#include "species.h"

#include "libutil.h"
#include "options.h"
#include "random.h"

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
static species_type species_order[] = {
    // comparatively human-like looks
    SP_HUMAN,          SP_HIGH_ELF,
    SP_DEEP_ELF,       SP_SLUDGE_ELF,
    SP_MOUNTAIN_DWARF, SP_DEEP_DWARF,
    SP_HILL_ORC,       SP_MERFOLK,
    // small species
    SP_HALFLING,       SP_KOBOLD,
    SP_SPRIGGAN,
    // significantly different body type from human
    SP_NAGA,           SP_CENTAUR,
    SP_OGRE,           SP_TROLL,
    SP_MINOTAUR,       SP_KENKU,
    SP_BASE_DRACONIAN,
    // celestial species
    SP_DEMIGOD,        SP_DEMONSPAWN,
    // undead species
    SP_MUMMY,          SP_GHOUL,
    SP_VAMPIRE,
    // not humanoid at all
    SP_CAT
};

species_type random_draconian_player_species()
{
    const int num_drac = SP_PALE_DRACONIAN - SP_RED_DRACONIAN + 1;
    return static_cast<species_type>(SP_RED_DRACONIAN + random2(num_drac));
}

species_type get_species(const int index)
{
    if (index < 0 || index >= ng_num_species())
        return (SP_UNKNOWN);

    return (species_order[index]);
}

static const char * Species_Abbrev_List[NUM_SPECIES] =
    { "Hu", "HE", "DE", "SE", "MD", "Ha",
      "HO", "Ko", "Mu", "Na", "Og", "Tr",
      // the draconians
      "Dr", "Dr", "Dr", "Dr", "Dr", "Dr", "Dr", "Dr", "Dr", "Dr",
      "Ce", "DG", "Sp", "Mi", "DS", "Gh", "Ke", "Mf", "Vp", "DD",
      "Fe",
      // placeholders
      "El", "HD", "OM", "GE", "Gn" };

int get_species_index_by_abbrev(const char *abbrev)
{
    COMPILE_CHECK(ARRAYSZ(Species_Abbrev_List) == NUM_SPECIES, c1);

    for (unsigned i = 0; i < ARRAYSZ(species_order); i++)
    {
        const int sp = species_order[i];

        if (tolower(abbrev[0]) == tolower(Species_Abbrev_List[sp][0])
            && tolower(abbrev[1]) == tolower(Species_Abbrev_List[sp][1]))
        {
            return (i);
        }
    }

    return (-1);
}

int get_species_index_by_name(const char *name)
{
    unsigned int i;
    int sp = -1;

    std::string::size_type pos = std::string::npos;
    char lowered_buff[80];

    strncpy(lowered_buff, name, sizeof(lowered_buff));
    strlwr(lowered_buff);

    for (i = 0; i < ARRAYSZ(species_order); i++)
    {
        const species_type real_sp = species_order[i];

        const std::string lowered_species =
            lowercase_string(species_name(real_sp));
        pos = lowered_species.find(lowered_buff);
        if (pos != std::string::npos)
        {
            sp = i;
            if (pos == 0)  // prefix takes preference
                break;
        }
    }

    return (sp);
}

const char *get_species_abbrev(species_type which_species)
{
    ASSERT(which_species >= 0 && which_species < NUM_SPECIES);

    return (Species_Abbrev_List[which_species]);
}

// Needed for debug.cc and hiscores.cc.
species_type get_species_by_abbrev(const char *abbrev)
{
    int i;
    COMPILE_CHECK(ARRAYSZ(Species_Abbrev_List) == NUM_SPECIES, c1);
    for (i = 0; i < NUM_SPECIES; i++)
    {
        if (tolower(abbrev[0]) == tolower(Species_Abbrev_List[i][0])
            && tolower(abbrev[1]) == tolower(Species_Abbrev_List[i][1]))
        {
            break;
        }
    }

    return ((i < NUM_SPECIES) ? static_cast<species_type>(i) : SP_UNKNOWN);
}

int ng_num_species()
{
    // The list musn't be longer than the number of actual species.
    COMPILE_CHECK(ARRAYSZ(species_order) <= NUM_SPECIES, c1);
    return (ARRAYSZ(species_order));
}

// Does a case-sensitive lookup of the species name supplied.
species_type str_to_species(const std::string &species)
{
    species_type sp;
    if (species.empty())
        return SP_UNKNOWN;

    for (int i = 0; i < NUM_SPECIES; ++i)
    {
        sp = static_cast<species_type>(i);
        if (species == species_name(sp))
            return (sp);
    }

    return (SP_UNKNOWN);
}

std::string species_name(species_type speci, bool genus, bool adj)
// defaults:                                 false       false
{
    std::string res;

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
            case SP_SLUDGE_ELF: res = "Sludge Elf"; break;
            default:            res = "Elf";        break;
            }
        }
        break;
    case GENPC_DWARVEN:
        if (adj)  // doesn't care about species/genus
            res = "Dwarven";
        else if (genus)
            res = "Dwarf";
        else
        {
            switch (speci)
            {
            case SP_MOUNTAIN_DWARF: res = "Mountain Dwarf";            break;
            case SP_DEEP_DWARF:     res = "Deep Dwarf";                break;
            default:                res = "Dwarf";                     break;
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
        case SP_KENKU:      res = "Kenku";                             break;

        case SP_HILL_ORC:
            res = (adj ? "Orcish" : genus ? "Orc" : "Hill Orc");
            break;

        case SP_OGRE:       res = (adj ? "Ogreish"    : "Ogre");       break;
        case SP_TROLL:      res = (adj ? "Trollish"   : "Troll");      break;
        case SP_DEMIGOD:    res = (adj ? "Divine"     : "Demigod");    break;
        case SP_DEMONSPAWN: res = (adj ? "Demonic"    : "Demonspawn"); break;
        case SP_GHOUL:      res = (adj ? "Ghoulish"   : "Ghoul");      break;
        case SP_MERFOLK:    res = (adj ? "Merfolkian" : "Merfolk");    break;
        case SP_VAMPIRE:    res = (adj ? "Vampiric"   : "Vampire");    break;
        case SP_CAT:        res = (adj ? "Feline"     : "Felid");      break;
        default:            res = (adj ? "Yakish"     : "Yak");        break;
        }
    }
    return res;
}

int species_has_claws(species_type species, bool mut_level)
{
    if (species == SP_TROLL)
        return (3);

    if (species == SP_GHOUL)
        return (1);

    // Felid claws don't count as a claws mutation.  The claws mutation
    // does only hands, not paws.
    if (species == SP_CAT && !mut_level)
        return (1);

    return (0);
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
        return (GENPC_DRACONIAN);

    case SP_HIGH_ELF:
    case SP_DEEP_ELF:
    case SP_SLUDGE_ELF:
        return (GENPC_ELVEN);

    case SP_MOUNTAIN_DWARF:
    case SP_DEEP_DWARF:
        return (GENPC_DWARVEN);

    case SP_OGRE:
        return (GENPC_OGREISH);

    default:
        return (GENPC_NONE);
    }
}

size_type species_size(species_type species, size_part_type psize)
{
    switch (species)
    {
    case SP_OGRE:
    case SP_TROLL:
        return (SIZE_LARGE);
    case SP_NAGA:
        // Most of their body is on the ground giving them a low profile.
        if (psize == PSIZE_TORSO || psize == PSIZE_PROFILE)
            return (SIZE_MEDIUM);
        else
            return (SIZE_LARGE);
    case SP_CENTAUR:
        return ((psize == PSIZE_TORSO) ? SIZE_MEDIUM : SIZE_LARGE);
    case SP_HALFLING:
    case SP_KOBOLD:
        return (SIZE_SMALL);
    case SP_SPRIGGAN:
        return (SIZE_LITTLE);
    case SP_CAT:
        return (SIZE_TINY);

    default:
        return (SIZE_MEDIUM);
    }
}

monster_type player_species_to_mons_species(species_type species)
{
    switch (species)
    {
    case SP_HUMAN:
        return (MONS_HUMAN);
    case SP_HIGH_ELF:
    case SP_DEEP_ELF:
    case SP_SLUDGE_ELF:
        return (MONS_ELF);
    case SP_MOUNTAIN_DWARF:
        return (MONS_DWARF);
    case SP_HALFLING:
        return (MONS_HALFLING);
    case SP_HILL_ORC:
        return (MONS_ORC);
    case SP_KOBOLD:
        return (MONS_KOBOLD);
    case SP_MUMMY:
        return (MONS_MUMMY);
    case SP_NAGA:
        return (MONS_NAGA);
    case SP_OGRE:
        return (MONS_OGRE);
    case SP_TROLL:
        return (MONS_TROLL);
    case SP_RED_DRACONIAN:
        return (MONS_RED_DRACONIAN);
    case SP_WHITE_DRACONIAN:
        return (MONS_WHITE_DRACONIAN);
    case SP_GREEN_DRACONIAN:
        return (MONS_GREEN_DRACONIAN);
    case SP_YELLOW_DRACONIAN:
        return (MONS_YELLOW_DRACONIAN);
    case SP_GREY_DRACONIAN:
        return (MONS_GREY_DRACONIAN);
    case SP_BLACK_DRACONIAN:
        return (MONS_BLACK_DRACONIAN);
    case SP_PURPLE_DRACONIAN:
        return (MONS_PURPLE_DRACONIAN);
    case SP_MOTTLED_DRACONIAN:
        return (MONS_MOTTLED_DRACONIAN);
    case SP_PALE_DRACONIAN:
        return (MONS_PALE_DRACONIAN);
    case SP_BASE_DRACONIAN:
        return (MONS_DRACONIAN);
    case SP_CENTAUR:
        return (MONS_CENTAUR);
    case SP_DEMIGOD:
        return (MONS_DEMIGOD);
    case SP_SPRIGGAN:
        return (MONS_SPRIGGAN);
    case SP_MINOTAUR:
        return (MONS_MINOTAUR);
    case SP_DEMONSPAWN:
        return (MONS_DEMONSPAWN);
    case SP_GHOUL:
        return (MONS_GHOUL);
    case SP_KENKU:
        return (MONS_KENKU);
    case SP_MERFOLK:
        return (MONS_MERFOLK);
    case SP_VAMPIRE:
        return (MONS_VAMPIRE);
    case SP_DEEP_DWARF:
        return (MONS_DEEP_DWARF);
    case SP_CAT:
        return (MONS_FELID);
    case SP_ELF:
    case SP_HILL_DWARF:
    case SP_OGRE_MAGE:
    case SP_GREY_ELF:
    case SP_GNOME:
    case NUM_SPECIES:
    case SP_UNKNOWN:
    case SP_RANDOM:
    case SP_VIABLE:
        ASSERT(!"player of an invalid species");
    default:
        return (MONS_PROGRAM_BUG);
    }
}

bool is_valid_species(species_type species)
{
    return (species >= 0 && species < NUM_SPECIES);
}
