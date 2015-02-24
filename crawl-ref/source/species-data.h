struct species_def
{
    const char* abbrev;
};

static const map<species_type, species_def> species_data =
{

{ SP_CENTAUR, {
    "Ce",
} },

{ SP_DEEP_DWARF, {
    "DD",
} },

{ SP_DEEP_ELF, {
    "DE",
} },

{ SP_DEMIGOD, {
    "Dg",
} },

// Keep this above the other draconians, so get_species_by_abbrev works
{ SP_BASE_DRACONIAN, {
    "Dr",
} },

{ SP_RED_DRACONIAN, {
    "Dr",
} },

{ SP_WHITE_DRACONIAN, {
    "Dr",
} },

{ SP_GREEN_DRACONIAN, {
    "Dr",
} },

{ SP_YELLOW_DRACONIAN, {
    "Dr",
} },

{ SP_GREY_DRACONIAN, {
    "Dr",
} },

{ SP_BLACK_DRACONIAN, {
    "Dr",
} },

{ SP_PURPLE_DRACONIAN, {
    "Dr",
} },

{ SP_MOTTLED_DRACONIAN, {
    "Dr",
} },

{ SP_PALE_DRACONIAN, {
    "Dr",
} },

{ SP_DEMONSPAWN, {
    "Ds",
} },

{ SP_FELID, {
    "Fe",
} },

{ SP_FORMICID, {
    "Fo",
} },

{ SP_GHOUL, {
    "Gh",
} },

{ SP_GARGOYLE, {
    "Gr",
} },

{ SP_HALFLING, {
    "Ha",
} },

{ SP_HIGH_ELF, {
    "HE",
} },

{ SP_HILL_ORC, {
    "HO",
} },

{ SP_HUMAN, {
    "Hu",
} },

{ SP_KOBOLD, {
    "Ko",
} },

{ SP_MERFOLK, {
    "Mf",
} },

{ SP_MINOTAUR, {
    "Mi",
} },

{ SP_MUMMY, {
    "Mu",
} },

{ SP_NAGA, {
    "Na",
} },

{ SP_OGRE, {
    "Og",
} },

{ SP_OCTOPODE, {
    "Op",
} },

{ SP_SPRIGGAN, {
    "Sp",
} },

{ SP_TENGU, {
    "Te",
} },

{ SP_TROLL, {
    "Tr",
} },

{ SP_VAMPIRE, {
    "Vp",
} },

{ SP_VINE_STALKER, {
    "VS",
} },
#if TAG_MAJOR_VERSION == 34
{ SP_SLUDGE_ELF, {
    "SE",
} },

{ SP_LAVA_ORC, {
    "LO",
} },

{ SP_DJINNI, {
    "Dj",
} },
#endif
};
