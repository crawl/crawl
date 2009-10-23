#include "AppHdr.h"

#include "species.h"

#include "stuff.h"

// March 2008: change order of species and jobs on character selection
// screen as suggested by Markus Maier. Summarizing comments below are
// copied directly from Markus' SourceForge comments. (jpeg)
//
// These are listed in two columns to match the selection screen output.
// Take care to list all valid species here, or they cannot be directly
// chosen.
//
// The red draconian is later replaced by a random variant.
// The old and new lists are expected to have the same length.
static species_type old_species_order[] = {
    SP_HUMAN,          SP_HIGH_ELF,
    SP_DEEP_ELF,       SP_SLUDGE_ELF,
    SP_MOUNTAIN_DWARF, SP_HALFLING,
    SP_HILL_ORC,       SP_KOBOLD,
    SP_MUMMY,          SP_NAGA,
    SP_OGRE,           SP_TROLL,
    SP_RED_DRACONIAN,  SP_CENTAUR,
    SP_DEMIGOD,        SP_SPRIGGAN,
    SP_MINOTAUR,       SP_DEMONSPAWN,
    SP_GHOUL,          SP_KENKU,
    SP_MERFOLK,        SP_VAMPIRE,
    SP_DEEP_DWARF
};

// Fantasy staples and humanoid creatures come first, then diminutive and
// stealthy creatures, then monstrous creatures, then planetouched and after
// all living creatures finally the undead. (MM)
static species_type new_species_order[] = {
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
    SP_RED_DRACONIAN,
    // celestial species
    SP_DEMIGOD,        SP_DEMONSPAWN,
    // undead species
    SP_MUMMY,          SP_GHOUL,
    SP_VAMPIRE
};

species_type random_draconian_player_species()
{
    const int num_drac = SP_PALE_DRACONIAN - SP_RED_DRACONIAN + 1;
    return static_cast<species_type>(SP_RED_DRACONIAN + random2(num_drac));
}

species_type get_species(const int index)
{
    if (index < 0 || (unsigned int) index >= ARRAYSZ(old_species_order))
        return (SP_UNKNOWN);

    return (Options.use_old_selection_order ? old_species_order[index]
                                            : new_species_order[index]);
}

static const char * Species_Abbrev_List[NUM_SPECIES] =
    { "XX", "Hu", "HE", "DE", "SE", "MD", "Ha",
      "HO", "Ko", "Mu", "Na", "Og", "Tr",
      // the draconians
      "Dr", "Dr", "Dr", "Dr", "Dr", "Dr", "Dr", "Dr", "Dr", "Dr",
      "Ce", "DG", "Sp", "Mi", "DS", "Gh", "Ke", "Mf", "Vp", "DD",
      // placeholders
      "El", "HD", "OM", "GE", "Gn" };

int get_species_index_by_abbrev(const char *abbrev)
{
    COMPILE_CHECK(ARRAYSZ(Species_Abbrev_List) == NUM_SPECIES, c1);

    for (unsigned i = 0; i < ARRAYSZ(old_species_order); i++)
    {
        const int sp = (Options.use_old_selection_order ? old_species_order[i]
                                                        : new_species_order[i]);

        if (tolower( abbrev[0] ) == tolower( Species_Abbrev_List[sp][0] )
            && tolower( abbrev[1] ) == tolower( Species_Abbrev_List[sp][1] ))
        {
            return (i);
        }
    }

    return (-1);
}

int get_species_index_by_name( const char *name )
{
    unsigned int i;
    int sp = -1;

    std::string::size_type pos = std::string::npos;
    char lowered_buff[80];

    strncpy(lowered_buff, name, sizeof(lowered_buff));
    strlwr(lowered_buff);

    for (i = 0; i < ARRAYSZ(old_species_order); i++)
    {
        const species_type real_sp
                   = (Options.use_old_selection_order ? old_species_order[i]
                                                      : new_species_order[i]);

        const std::string lowered_species =
            lowercase_string(species_name(real_sp,1));
        pos = lowered_species.find( lowered_buff );
        if (pos != std::string::npos)
        {
            sp = i;
            if (pos == 0)  // prefix takes preference
                break;
        }
    }

    return (sp);
}

const char *get_species_abbrev( int which_species )
{
    ASSERT( which_species > 0 && which_species < NUM_SPECIES );

    return (Species_Abbrev_List[ which_species ]);
}

// Needed for debug.cc and hiscores.cc.
int get_species_by_abbrev( const char *abbrev )
{
    int i;
    COMPILE_CHECK(ARRAYSZ(Species_Abbrev_List) == NUM_SPECIES, c1);
    for (i = SP_HUMAN; i < NUM_SPECIES; i++)
    {
        if (tolower( abbrev[0] ) == tolower( Species_Abbrev_List[i][0] )
            && tolower( abbrev[1] ) == tolower( Species_Abbrev_List[i][1] ))
        {
            break;
        }
    }

    return ((i < NUM_SPECIES) ? i : -1);
}

int ng_num_species()
{
    // The list musn't be longer than the number of actual species.
    COMPILE_CHECK(ARRAYSZ(old_species_order) <= NUM_SPECIES, c1);
    // Check whether the two lists have the same size.
    COMPILE_CHECK(ARRAYSZ(old_species_order) == ARRAYSZ(new_species_order), c2);
    return (ARRAYSZ(old_species_order));
}

// Does a case-sensitive lookup of the species name supplied.
int str_to_species(const std::string &species)
{
    if (species.empty())
        return SP_HUMAN;

    // first look for full name (e.g. Green Draconian)
    for (int i = SP_HUMAN; i < NUM_SPECIES; ++i)
    {
        if (species == species_name(static_cast<species_type>(i), 10))
            return (i);
    }

    // nothing found, try again with plain name
    for (int i = SP_HUMAN; i < NUM_SPECIES; ++i)
    {
        if (species == species_name(static_cast<species_type>(i), 1))
            return (i);
    }

    return (SP_HUMAN);
}

std::string species_name(species_type speci, int level, bool genus, bool adj)
// defaults:                                            false       false
{
    std::string res;

    if (player_genus( GENPC_DRACONIAN, speci ))
    {
        if (adj || genus)  // adj doesn't care about exact species
            res = "Draconian";
        else
        {
            if (level < 7)
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
        }
    }
    else if (player_genus( GENPC_ELVEN, speci ))
    {
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
    }
    else if (player_genus(GENPC_DWARVEN, speci))
    {
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
    }
    else
    {
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
        default:            res = (adj ? "Yakish"     : "Yak");        break;
        }
    }
    return res;
}
