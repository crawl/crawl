struct species_def
{
    const char* abbrev; ///< Two-letter abbreviation for the species.
    const char *name; ///< Main name for the species.
    const char *adj_name; ///< Adjectival form of name; if null, use name.
    const char *genus_name; ///< Genus name; if null, use name.
};

static const map<species_type, species_def> species_data =
{

{ SP_CENTAUR, {
    "Ce",
    "Centaur", nullptr, nullptr,
} },

{ SP_DEEP_DWARF, {
    "DD",
    "Deep Dwarf", "Dwarven", "Dwarf",
} },

{ SP_DEEP_ELF, {
    "DE",
    "Deep Elf", "Elven", "Elf",
} },

{ SP_DEMIGOD, {
    "Dg",
    "Demigod", "Divine", nullptr,
} },

// Keep this above the other draconians, so get_species_by_abbrev works
{ SP_BASE_DRACONIAN, {
    "Dr",
    "Draconian", nullptr, nullptr,
} },

{ SP_RED_DRACONIAN, {
    "Dr",
    "Red Draconian", "Draconian", "Draconian",
} },

{ SP_WHITE_DRACONIAN, {
    "Dr",
    "White Draconian", "Draconian", "Draconian",
} },

{ SP_GREEN_DRACONIAN, {
    "Dr",
    "Green Draconian", "Draconian", "Draconian",
} },

{ SP_YELLOW_DRACONIAN, {
    "Dr",
    "Yellow Draconian", "Draconian", "Draconian",
} },

{ SP_GREY_DRACONIAN, {
    "Dr",
    "Grey Draconian", "Draconian", "Draconian",
} },

{ SP_BLACK_DRACONIAN, {
    "Dr",
    "Black Draconian", "Draconian", "Draconian",
} },

{ SP_PURPLE_DRACONIAN, {
    "Dr",
    "Purple Draconian", "Draconian", "Draconian",
} },

{ SP_MOTTLED_DRACONIAN, {
    "Dr",
    "Mottled Draconian", "Draconian", "Draconian",
} },

{ SP_PALE_DRACONIAN, {
    "Dr",
    "Pale Draconian", "Draconian", "Draconian",
} },

{ SP_DEMONSPAWN, {
    "Ds",
    "Demonspawn", "Demonic", nullptr,
} },

{ SP_FELID, {
    "Fe",
    "Felid", "Feline", "Cat",
} },

{ SP_FORMICID, {
    "Fo",
    "Formicid", nullptr, "Ant",
} },

{ SP_GHOUL, {
    "Gh",
    "Ghoul", "Ghoulish", nullptr,
} },

{ SP_GARGOYLE, {
    "Gr",
    "Gargoyle", nullptr, nullptr,
} },

{ SP_HALFLING, {
    "Ha",
    "Halfling", nullptr, nullptr,
} },

{ SP_HIGH_ELF, {
    "HE",
    "High Elf", "Elven", "Elf",
} },

{ SP_HILL_ORC, {
    "HO",
    "Hill Orc", "Orcish", "Orc",
} },

{ SP_HUMAN, {
    "Hu",
    "Human", nullptr, nullptr,
} },

{ SP_KOBOLD, {
    "Ko",
    "Kobold", nullptr, nullptr,
} },

{ SP_MERFOLK, {
    "Mf",
    "Merfolk", "Merfolkian", nullptr,
} },

{ SP_MINOTAUR, {
    "Mi",
    "Minotaur", nullptr, nullptr,
} },

{ SP_MUMMY, {
    "Mu",
    "Mummy", nullptr, nullptr,
} },

{ SP_NAGA, {
    "Na",
    "Naga", nullptr, nullptr,
} },

{ SP_OGRE, {
    "Og",
    "Ogre", "Ogreish", nullptr,
} },

{ SP_OCTOPODE, {
    "Op",
    "Octopode", "Octopoid", "Octopus",
} },

{ SP_SPRIGGAN, {
    "Sp",
    "Spriggan", nullptr, nullptr,
} },

{ SP_TENGU, {
    "Te",
    "Tengu", nullptr, nullptr,
} },

{ SP_TROLL, {
    "Tr",
    "Troll", "Trollish", nullptr,
} },

{ SP_VAMPIRE, {
    "Vp",
    "Vampire", "Vampiric", nullptr,
} },

{ SP_VINE_STALKER, {
    "VS",
    "Vine Stalker", "Vine", "Vine",
} },
#if TAG_MAJOR_VERSION == 34
{ SP_SLUDGE_ELF, {
    "SE",
    "Sludge Elf", "Elven", "Elf",
} },

{ SP_LAVA_ORC, {
    "LO",
    "Lava Orc", "Orcish", "Orc",
} },

{ SP_DJINNI, {
    "Dj",
    "Djinni", "Djinn", nullptr,
} },
#endif
};
