struct species_def
{
    const char* abbrev; ///< Two-letter abbreviation for the species.
    const char* name; ///< Main name for the species.
    const char* adj_name; ///< Adjectival form of name; if null, use name.
    const char* genus_name; ///< Genus name; if null, use name.
    int xp_mod; ///< Experience level modifier of the species.
    int hp_mod; ///< HP modifier (in tenths).
    int mp_mod; ///< MP modifier (in tenths).
    monster_type monster_species; ///< Corresponding monster (for display).
};

static const map<species_type, species_def> species_data =
{

{ SP_CENTAUR, {
    "Ce",
    "Centaur", nullptr, nullptr,
    -1, 1, -1,
    MONS_CENTAUR,
} },

{ SP_DEEP_DWARF, {
    "DD",
    "Deep Dwarf", "Dwarven", "Dwarf",
    -1, 2, 0,
    MONS_DEEP_DWARF,
} },

{ SP_DEEP_ELF, {
    "DE",
    "Deep Elf", "Elven", "Elf",
    -1, -2, 3,
    MONS_ELF,
} },

{ SP_DEMIGOD, {
    "Dg",
    "Demigod", "Divine", nullptr,
    -2, 1, 2,
    MONS_DEMIGOD,
} },

// Keep this above the other draconians, so get_species_by_abbrev works
{ SP_BASE_DRACONIAN, {
    "Dr",
    "Draconian", nullptr, nullptr,
    -1, 1, 0,
    MONS_DRACONIAN,
} },

{ SP_RED_DRACONIAN, {
    "Dr",
    "Red Draconian", "Draconian", "Draconian",
    -1, 1, 0,
    MONS_RED_DRACONIAN,
} },

{ SP_WHITE_DRACONIAN, {
    "Dr",
    "White Draconian", "Draconian", "Draconian",
    -1, 1, 0,
    MONS_WHITE_DRACONIAN,
} },

{ SP_GREEN_DRACONIAN, {
    "Dr",
    "Green Draconian", "Draconian", "Draconian",
    -1, 1, 0,
    MONS_GREEN_DRACONIAN,
} },

{ SP_YELLOW_DRACONIAN, {
    "Dr",
    "Yellow Draconian", "Draconian", "Draconian",
    -1, 1, 0,
    MONS_YELLOW_DRACONIAN,
} },

{ SP_GREY_DRACONIAN, {
    "Dr",
    "Grey Draconian", "Draconian", "Draconian",
    -1, 1, 0,
    MONS_GREY_DRACONIAN,
} },

{ SP_BLACK_DRACONIAN, {
    "Dr",
    "Black Draconian", "Draconian", "Draconian",
    -1, 1, 0,
    MONS_BLACK_DRACONIAN,
} },

{ SP_PURPLE_DRACONIAN, {
    "Dr",
    "Purple Draconian", "Draconian", "Draconian",
    -1, 1, 0,
    MONS_PURPLE_DRACONIAN,
} },

{ SP_MOTTLED_DRACONIAN, {
    "Dr",
    "Mottled Draconian", "Draconian", "Draconian",
    -1, 1, 0,
    MONS_MOTTLED_DRACONIAN,
} },

{ SP_PALE_DRACONIAN, {
    "Dr",
    "Pale Draconian", "Draconian", "Draconian",
    -1, 1, 0,
    MONS_PALE_DRACONIAN,
} },

{ SP_DEMONSPAWN, {
    "Ds",
    "Demonspawn", "Demonic", nullptr,
    -1, 0, 0,
    MONS_DEMONSPAWN,
} },

{ SP_FELID, {
    "Fe",
    "Felid", "Feline", "Cat",
    -1, -4, 2,
    MONS_FELID,
} },

{ SP_FORMICID, {
    "Fo",
    "Formicid", nullptr, "Ant",
    1, 0, 1,
    MONS_FORMICID,
} },

{ SP_GHOUL, {
    "Gh",
    "Ghoul", "Ghoulish", nullptr,
    0, 1, -1,
    MONS_GHOUL,
} },

{ SP_GARGOYLE, {
    "Gr",
    "Gargoyle", nullptr, nullptr,
    0, -2, 0,
    MONS_GARGOYLE,
} },

{ SP_HALFLING, {
    "Ha",
    "Halfling", nullptr, nullptr,
    1, -1, 0,
    MONS_HALFLING,
} },

{ SP_HIGH_ELF, {
    "HE",
    "High Elf", "Elven", "Elf",
    -1, -1, 2,
    MONS_ELF,
} },

{ SP_HILL_ORC, {
    "HO",
    "Hill Orc", "Orcish", "Orc",
    0, 1, 0,
    MONS_ORC,
} },

{ SP_HUMAN, {
    "Hu",
    "Human", nullptr, nullptr,
    1, 0, 0,
    MONS_HUMAN,
} },

{ SP_KOBOLD, {
    "Ko",
    "Kobold", nullptr, nullptr,
    1, -2, 0,
    MONS_KOBOLD,
} },

{ SP_MERFOLK, {
    "Mf",
    "Merfolk", "Merfolkian", nullptr,
    0, 0, 0,
    MONS_MERFOLK,
} },

{ SP_MINOTAUR, {
    "Mi",
    "Minotaur", nullptr, nullptr,
    -1, 1, -2,
    MONS_MINOTAUR,
} },

{ SP_MUMMY, {
    "Mu",
    "Mummy", nullptr, nullptr,
    -1, 0, 0,
    MONS_MUMMY,
} },

{ SP_NAGA, {
    "Na",
    "Naga", nullptr, nullptr,
    0, 2, 0,
    MONS_NAGA,
} },

{ SP_OGRE, {
    "Og",
    "Ogre", "Ogreish", nullptr,
    0, 3, 0,
    MONS_OGRE,
} },

{ SP_OCTOPODE, {
    "Op",
    "Octopode", "Octopoid", "Octopus",
    0, -1, 0,
    MONS_OCTOPODE,
} },

{ SP_SPRIGGAN, {
    "Sp",
    "Spriggan", nullptr, nullptr,
    -1, -3, 3,
    MONS_SPRIGGAN,
} },

{ SP_TENGU, {
    "Te",
    "Tengu", nullptr, nullptr,
    0, -2, 1,
    MONS_TENGU,
} },

{ SP_TROLL, {
    "Tr",
    "Troll", "Trollish", nullptr,
    -1, 3, -2,
    MONS_TROLL,
} },

{ SP_VAMPIRE, {
    "Vp",
    "Vampire", "Vampiric", nullptr,
    -1, 0, 0,
    MONS_VAMPIRE,
} },

{ SP_VINE_STALKER, {
    "VS",
    "Vine Stalker", "Vine", "Vine",
    0, -3, 1,
    MONS_VINE_STALKER,
} },
#if TAG_MAJOR_VERSION == 34
{ SP_SLUDGE_ELF, {
    "SE",
    "Sludge Elf", "Elven", "Elf",
    0, -1, 1,
    MONS_ELF,
} },

{ SP_LAVA_ORC, {
    "LO",
    "Lava Orc", "Orcish", "Orc",
    -1, 1, 0,
    MONS_LAVA_ORC,
} },

{ SP_DJINNI, {
    "Dj",
    "Djinni", "Djinn", nullptr,
    -1, -1, 0,
    MONS_DJINNI,
} },
#endif
};
