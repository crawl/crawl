enum species_flag
{
    SPF_NONE        = 0,
    SPF_ELVEN       = 1 << 0, /// If this species counts as an elf
    SPF_DRACONIAN   = 1 << 1, /// If this is a draconian subspecies
    SPF_ORCISH      = 1 << 2, /// If this species is a kind of orc
    SPF_NO_HAIR     = 1 << 3, /// If members of the species are hairless
};
DEF_BITFIELD(species_flags, species_flag);

struct species_def
{
    const char* abbrev; ///< Two-letter abbreviation
    const char* name; ///< Main name
    const char* adj_name; ///< Adjectival form of name; if null, use name
    const char* genus_name; ///< Genus name; if null, use name
    species_flags flags; ///< Miscellaneous flags
    int xp_mod; ///< Experience level modifier
    int hp_mod; ///< HP modifier (in tenths)
    int mp_mod; ///< MP modifier (in tenths)
    monster_type monster_species; ///< Corresponding monster (for display)
    habitat_type habitat; ///< Where it can live; HT_WATER -> no penalties
    undead_state_type undeadness; ///< What kind of undead (if any)
};

static const map<species_type, species_def> species_data =
{

{ SP_CENTAUR, {
    "Ce",
    "Centaur", nullptr, nullptr,
    SPF_NONE,
    -1, 1, -1,
    MONS_CENTAUR,
    HT_LAND, US_ALIVE,
} },

{ SP_DEEP_DWARF, {
    "DD",
    "Deep Dwarf", "Dwarven", "Dwarf",
    SPF_NONE,
     -1, 2, 0,
    MONS_DEEP_DWARF,
    HT_LAND, US_ALIVE,
} },

{ SP_DEEP_ELF, {
    "DE",
    "Deep Elf", "Elven", "Elf",
    SPF_ELVEN,
     -1, -2, 3,
    MONS_ELF,
    HT_LAND, US_ALIVE,
} },

{ SP_DEMIGOD, {
    "Dg",
    "Demigod", "Divine", nullptr,
    SPF_NONE,
    -2, 1, 2,
    MONS_DEMIGOD,
    HT_LAND, US_ALIVE,
} },

// Keep this above the other draconians, so get_species_by_abbrev works
{ SP_BASE_DRACONIAN, {
    "Dr",
    "Draconian", nullptr, nullptr,
    SPF_DRACONIAN,
    -1, 1, 0,
    MONS_DRACONIAN,
    HT_LAND, US_ALIVE,
} },

{ SP_RED_DRACONIAN, {
    "Dr",
    "Red Draconian", "Draconian", "Draconian",
    SPF_DRACONIAN,
    -1, 1, 0,
    MONS_RED_DRACONIAN,
    HT_LAND, US_ALIVE,
} },

{ SP_WHITE_DRACONIAN, {
    "Dr",
    "White Draconian", "Draconian", "Draconian",
    SPF_DRACONIAN,
    -1, 1, 0,
    MONS_WHITE_DRACONIAN,
    HT_LAND, US_ALIVE,
} },

{ SP_GREEN_DRACONIAN, {
    "Dr",
    "Green Draconian", "Draconian", "Draconian",
    SPF_DRACONIAN,
    -1, 1, 0,
    MONS_GREEN_DRACONIAN,
    HT_LAND, US_ALIVE,
} },

{ SP_YELLOW_DRACONIAN, {
    "Dr",
    "Yellow Draconian", "Draconian", "Draconian",
    SPF_DRACONIAN,
    -1, 1, 0,
    MONS_YELLOW_DRACONIAN,
    HT_LAND, US_ALIVE,
} },

{ SP_GREY_DRACONIAN, {
    "Dr",
    "Grey Draconian", "Draconian", "Draconian",
    SPF_DRACONIAN,
    -1, 1, 0,
    MONS_GREY_DRACONIAN,
    HT_AMPHIBIOUS, US_ALIVE,
} },

{ SP_BLACK_DRACONIAN, {
    "Dr",
    "Black Draconian", "Draconian", "Draconian",
    SPF_DRACONIAN,
    -1, 1, 0,
    MONS_BLACK_DRACONIAN,
    HT_LAND, US_ALIVE,
} },

{ SP_PURPLE_DRACONIAN, {
    "Dr",
    "Purple Draconian", "Draconian", "Draconian",
    SPF_DRACONIAN,
    -1, 1, 0,
    MONS_PURPLE_DRACONIAN,
    HT_LAND, US_ALIVE,
} },

{ SP_MOTTLED_DRACONIAN, {
    "Dr",
    "Mottled Draconian", "Draconian", "Draconian",
    SPF_DRACONIAN,
    -1, 1, 0,
    MONS_MOTTLED_DRACONIAN,
    HT_LAND, US_ALIVE,
} },

{ SP_PALE_DRACONIAN, {
    "Dr",
    "Pale Draconian", "Draconian", "Draconian",
    SPF_DRACONIAN,
    -1, 1, 0,
    MONS_PALE_DRACONIAN,
    HT_LAND, US_ALIVE,
} },

{ SP_DEMONSPAWN, {
    "Ds",
    "Demonspawn", "Demonic", nullptr,
    SPF_NONE,
    -1, 0, 0,
    MONS_DEMONSPAWN,
    HT_LAND, US_ALIVE,
} },

{ SP_FELID, {
    "Fe",
    "Felid", "Feline", "Cat",
    SPF_NONE,
    -1, -4, 2,
    MONS_FELID,
    HT_LAND, US_ALIVE,
} },

{ SP_FORMICID, {
    "Fo",
    "Formicid", nullptr, "Ant",
    SPF_NONE,
    1, 0, 1,
    MONS_FORMICID,
    HT_LAND, US_ALIVE,
} },

{ SP_GHOUL, {
    "Gh",
    "Ghoul", "Ghoulish", nullptr,
    SPF_NO_HAIR,
    0, 1, -1,
    MONS_GHOUL,
    HT_LAND, US_HUNGRY_DEAD,
} },

{ SP_GARGOYLE, {
    "Gr",
    "Gargoyle", nullptr, nullptr,
    SPF_NO_HAIR,
    0, -2, 0,
    MONS_GARGOYLE,
    HT_LAND, US_ALIVE,
} },

{ SP_HALFLING, {
    "Ha",
    "Halfling", nullptr, nullptr,
    SPF_NONE,
    1, -1, 0,
    MONS_HALFLING,
    HT_LAND, US_ALIVE,
} },

{ SP_HIGH_ELF, {
    "HE",
    "High Elf", "Elven", "Elf",
    SPF_ELVEN,
    -1, -1, 2,
    MONS_ELF,
    HT_LAND, US_ALIVE,
} },

{ SP_HILL_ORC, {
    "HO",
    "Hill Orc", "Orcish", "Orc",
    SPF_ORCISH,
    0, 1, 0,
    MONS_ORC,
    HT_LAND, US_ALIVE,
} },

{ SP_HUMAN, {
    "Hu",
    "Human", nullptr, nullptr,
    SPF_NONE,
    1, 0, 0,
    MONS_HUMAN,
    HT_LAND, US_ALIVE,
} },

{ SP_KOBOLD, {
    "Ko",
    "Kobold", nullptr, nullptr,
    SPF_NONE,
    1, -2, 0,
    MONS_KOBOLD,
    HT_LAND, US_ALIVE,
} },

{ SP_MERFOLK, {
    "Mf",
    "Merfolk", "Merfolkian", nullptr,
    SPF_NONE,
    0, 0, 0,
    MONS_MERFOLK,
    HT_WATER, US_ALIVE,
} },

{ SP_MINOTAUR, {
    "Mi",
    "Minotaur", nullptr, nullptr,
    SPF_NONE,
    -1, 1, -2,
    MONS_MINOTAUR,
    HT_LAND, US_ALIVE,
} },

{ SP_MUMMY, {
    "Mu",
    "Mummy", nullptr, nullptr,
    SPF_NONE,
    -1, 0, 0,
    MONS_MUMMY,
    HT_LAND, US_UNDEAD,
} },

{ SP_NAGA, {
    "Na",
    "Naga", nullptr, nullptr,
    SPF_NONE,
    0, 2, 0,
    MONS_NAGA,
    HT_LAND, US_ALIVE,
} },

{ SP_OGRE, {
    "Og",
    "Ogre", "Ogreish", nullptr,
    SPF_NONE,
    0, 3, 0,
    MONS_OGRE,
    HT_LAND, US_ALIVE,
} },

{ SP_OCTOPODE, {
    "Op",
    "Octopode", "Octopoid", "Octopus",
    SPF_NO_HAIR,
    0, -1, 0,
    MONS_OCTOPODE,
    HT_WATER, US_ALIVE,
} },

{ SP_SPRIGGAN, {
    "Sp",
    "Spriggan", nullptr, nullptr,
    SPF_NONE,
    -1, -3, 3,
    MONS_SPRIGGAN,
    HT_LAND, US_ALIVE,
} },

{ SP_TENGU, {
    "Te",
    "Tengu", nullptr, nullptr,
    SPF_NO_HAIR,
    0, -2, 1,
    MONS_TENGU,
    HT_LAND, US_ALIVE,
} },

{ SP_TROLL, {
    "Tr",
    "Troll", "Trollish", nullptr,
    SPF_NONE,
    -1, 3, -2,
    MONS_TROLL,
    HT_LAND, US_ALIVE,
} },

{ SP_VAMPIRE, {
    "Vp",
    "Vampire", "Vampiric", nullptr,
    SPF_NONE,
    -1, 0, 0,
    MONS_VAMPIRE,
    HT_LAND, US_SEMI_UNDEAD,
} },

{ SP_VINE_STALKER, {
    "VS",
    "Vine Stalker", "Vine", "Vine",
    SPF_NONE,
    0, -3, 1,
    MONS_VINE_STALKER,
    HT_LAND, US_ALIVE,
} },
#if TAG_MAJOR_VERSION == 34
{ SP_SLUDGE_ELF, {
    "SE",
    "Sludge Elf", "Elven", "Elf",
    SPF_ELVEN,
    0, -1, 1,
    MONS_ELF,
    HT_LAND, US_ALIVE,
} },

{ SP_LAVA_ORC, {
    "LO",
    "Lava Orc", "Orcish", "Orc",
    SPF_ORCISH | SPF_NO_HAIR,
    -1, 1, 0,
    MONS_LAVA_ORC,
    HT_AMPHIBIOUS_LAVA, US_ALIVE,
} },

{ SP_DJINNI, {
    "Dj",
    "Djinni", "Djinn", nullptr,
    SPF_NONE,
    -1, -1, 0,
    MONS_DJINNI,
    HT_LAND, US_ALIVE,
} },
#endif
};
