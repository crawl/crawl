struct species_def
{
    const char* abbrev; ///< Two-letter abbreviation for the species.
    const char* name; ///< Main name for the species.
    const char* adj_name; ///< Adjectival form of name; if null, use name.
    const char* genus_name; ///< Genus name; if null, use name.
    int xp_mod; ///< Experience level modifier of the species.
    int hp_mod; ///< HP modifier (in tenths).
    int mp_mod; ///< MP modifier (in tenths).
};

static const map<species_type, species_def> species_data =
{

{ SP_CENTAUR, {
    "Ce",
    "Centaur", nullptr, nullptr,
    -1, 1, -1,
} },

{ SP_DEEP_DWARF, {
    "DD",
    "Deep Dwarf", "Dwarven", "Dwarf",
    -1, 2, 0,
} },

{ SP_DEEP_ELF, {
    "DE",
    "Deep Elf", "Elven", "Elf",
    -1, -2, 3,
} },

{ SP_DEMIGOD, {
    "Dg",
    "Demigod", "Divine", nullptr,
    -2, 1, 2,
} },

// Keep this above the other draconians, so get_species_by_abbrev works
{ SP_BASE_DRACONIAN, {
    "Dr",
    "Draconian", nullptr, nullptr,
    -1, 1, 0,
} },

{ SP_RED_DRACONIAN, {
    "Dr",
    "Red Draconian", "Draconian", "Draconian",
    -1, 1, 0,
} },

{ SP_WHITE_DRACONIAN, {
    "Dr",
    "White Draconian", "Draconian", "Draconian",
    -1, 1, 0,
} },

{ SP_GREEN_DRACONIAN, {
    "Dr",
    "Green Draconian", "Draconian", "Draconian",
    -1, 1, 0,
} },

{ SP_YELLOW_DRACONIAN, {
    "Dr",
    "Yellow Draconian", "Draconian", "Draconian",
    -1, 1, 0,
} },

{ SP_GREY_DRACONIAN, {
    "Dr",
    "Grey Draconian", "Draconian", "Draconian",
    -1, 1, 0,
} },

{ SP_BLACK_DRACONIAN, {
    "Dr",
    "Black Draconian", "Draconian", "Draconian",
    -1, 1, 0,
} },

{ SP_PURPLE_DRACONIAN, {
    "Dr",
    "Purple Draconian", "Draconian", "Draconian",
    -1, 1, 0,
} },

{ SP_MOTTLED_DRACONIAN, {
    "Dr",
    "Mottled Draconian", "Draconian", "Draconian",
    -1, 1, 0,
} },

{ SP_PALE_DRACONIAN, {
    "Dr",
    "Pale Draconian", "Draconian", "Draconian",
    -1, 1, 0,
} },

{ SP_DEMONSPAWN, {
    "Ds",
    "Demonspawn", "Demonic", nullptr,
    -1, 0, 0,
} },

{ SP_FELID, {
    "Fe",
    "Felid", "Feline", "Cat",
    -1, -4, 2,
} },

{ SP_FORMICID, {
    "Fo",
    "Formicid", nullptr, "Ant",
    1, 0, 1,
} },

{ SP_GHOUL, {
    "Gh",
    "Ghoul", "Ghoulish", nullptr,
    0, 1, -1,
} },

{ SP_GARGOYLE, {
    "Gr",
    "Gargoyle", nullptr, nullptr,
    0, -2, 0,
} },

{ SP_HALFLING, {
    "Ha",
    "Halfling", nullptr, nullptr,
    1, -1, 0,
} },

{ SP_HIGH_ELF, {
    "HE",
    "High Elf", "Elven", "Elf",
    -1, -1, 2,
} },

{ SP_HILL_ORC, {
    "HO",
    "Hill Orc", "Orcish", "Orc",
    0, 1, 0,
} },

{ SP_HUMAN, {
    "Hu",
    "Human", nullptr, nullptr,
    1, 0, 0,
} },

{ SP_KOBOLD, {
    "Ko",
    "Kobold", nullptr, nullptr,
    1, -2, 0,
} },

{ SP_MERFOLK, {
    "Mf",
    "Merfolk", "Merfolkian", nullptr,
    0, 0, 0,
} },

{ SP_MINOTAUR, {
    "Mi",
    "Minotaur", nullptr, nullptr,
    -1, 1, -2,
} },

{ SP_MUMMY, {
    "Mu",
    "Mummy", nullptr, nullptr,
    -1, 0, 0,
} },

{ SP_NAGA, {
    "Na",
    "Naga", nullptr, nullptr,
    0, 2, 0,
} },

{ SP_OGRE, {
    "Og",
    "Ogre", "Ogreish", nullptr,
    0, 3, 0,
} },

{ SP_OCTOPODE, {
    "Op",
    "Octopode", "Octopoid", "Octopus",
    0, -1, 0,
} },

{ SP_SPRIGGAN, {
    "Sp",
    "Spriggan", nullptr, nullptr,
    -1, -3, 3,
} },

{ SP_TENGU, {
    "Te",
    "Tengu", nullptr, nullptr,
    0, -2, 1,
} },

{ SP_TROLL, {
    "Tr",
    "Troll", "Trollish", nullptr,
    -1, 3, -2
} },

{ SP_VAMPIRE, {
    "Vp",
    "Vampire", "Vampiric", nullptr,
    -1, 0, 0,
} },

{ SP_VINE_STALKER, {
    "VS",
    "Vine Stalker", "Vine", "Vine",
    0, -3, 1,
} },
#if TAG_MAJOR_VERSION == 34
{ SP_SLUDGE_ELF, {
    "SE",
    "Sludge Elf", "Elven", "Elf",
    0, -1, 1,
} },

{ SP_LAVA_ORC, {
    "LO",
    "Lava Orc", "Orcish", "Orc",
    -1, 1, 0,
} },

{ SP_DJINNI, {
    "Dj",
    "Djinni", "Djinn", nullptr,
    -1, -1, 0,
} },
#endif
};
