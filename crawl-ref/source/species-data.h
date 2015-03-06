enum species_flag
{
    SPF_NONE        = 0,
    SPF_ELVEN       = 1 << 0, /// If this species counts as an elf
    SPF_DRACONIAN   = 1 << 1, /// If this is a draconian subspecies
    SPF_ORCISH      = 1 << 2, /// If this species is a kind of orc
    SPF_NO_HAIR     = 1 << 3, /// If members of the species are hairless
    SPF_SMALL_TORSO = 1 << 4, /// Torso is smaller than body
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
    size_type size; ///< Size of body
    int s, i, d; ///< Starting stats contribution
    bool gain_s, gain_i, gain_d; ///< Which stats to gain on level-up
    int how_often; ///< When to level-up stats
};

static const map<species_type, species_def> species_data =
{

{ SP_CENTAUR, {
    "Ce",
    "Centaur", nullptr, nullptr,
    SPF_SMALL_TORSO,
    -1, 1, -1,
    MONS_CENTAUR,
    HT_LAND, US_ALIVE, SIZE_LARGE,
    10, 7, 4, // 21
    true, false, true, 4,
} },

{ SP_DEEP_DWARF, {
    "DD",
    "Deep Dwarf", "Dwarven", "Dwarf",
    SPF_NONE,
     -1, 2, 0,
    MONS_DEEP_DWARF,
    HT_LAND, US_ALIVE, SIZE_MEDIUM,
    11, 8, 8, // 27
    true, true, false, 4,
} },

{ SP_DEEP_ELF, {
    "DE",
    "Deep Elf", "Elven", "Elf",
    SPF_ELVEN,
     -1, -2, 3,
    MONS_ELF,
    HT_LAND, US_ALIVE, SIZE_MEDIUM,
    5, 12, 10, // 27
    false, true, false, 4,
} },

{ SP_DEMIGOD, {
    "Dg",
    "Demigod", "Divine", nullptr,
    SPF_NONE,
    -2, 1, 2,
    MONS_DEMIGOD,
    HT_LAND, US_ALIVE, SIZE_MEDIUM,
    11, 12, 11, // 34
    false, false, false, 28, // No natural stat gain (double chosen instead)
} },

// Keep this above the other draconians, so get_species_by_abbrev works
{ SP_BASE_DRACONIAN, {
    "Dr",
    "Draconian", nullptr, nullptr,
    SPF_DRACONIAN,
    -1, 1, 0,
    MONS_DRACONIAN,
    HT_LAND, US_ALIVE, SIZE_MEDIUM,
    10, 8, 6, // 24
    true, true, true, 4,
} },

{ SP_RED_DRACONIAN, {
    "Dr",
    "Red Draconian", "Draconian", "Draconian",
    SPF_DRACONIAN,
    -1, 1, 0,
    MONS_RED_DRACONIAN,
    HT_LAND, US_ALIVE, SIZE_MEDIUM,
    10, 8, 6, // 24
    true, true, true, 4,
} },

{ SP_WHITE_DRACONIAN, {
    "Dr",
    "White Draconian", "Draconian", "Draconian",
    SPF_DRACONIAN,
    -1, 1, 0,
    MONS_WHITE_DRACONIAN,
    HT_LAND, US_ALIVE, SIZE_MEDIUM,
    10, 8, 6, // 24
    true, true, true, 4,
} },

{ SP_GREEN_DRACONIAN, {
    "Dr",
    "Green Draconian", "Draconian", "Draconian",
    SPF_DRACONIAN,
    -1, 1, 0,
    MONS_GREEN_DRACONIAN,
    HT_LAND, US_ALIVE, SIZE_MEDIUM,
    10, 8, 6, // 24
    true, true, true, 4,
} },

{ SP_YELLOW_DRACONIAN, {
    "Dr",
    "Yellow Draconian", "Draconian", "Draconian",
    SPF_DRACONIAN,
    -1, 1, 0,
    MONS_YELLOW_DRACONIAN,
    HT_LAND, US_ALIVE, SIZE_MEDIUM,
    10, 8, 6, // 24
    true, true, true, 4,
} },

{ SP_GREY_DRACONIAN, {
    "Dr",
    "Grey Draconian", "Draconian", "Draconian",
    SPF_DRACONIAN,
    -1, 1, 0,
    MONS_GREY_DRACONIAN,
    HT_AMPHIBIOUS, US_ALIVE, SIZE_MEDIUM,
    10, 8, 6, // 24
    true, true, true, 4,
} },

{ SP_BLACK_DRACONIAN, {
    "Dr",
    "Black Draconian", "Draconian", "Draconian",
    SPF_DRACONIAN,
    -1, 1, 0,
    MONS_BLACK_DRACONIAN,
    HT_LAND, US_ALIVE, SIZE_MEDIUM,
    10, 8, 6, // 24
    true, true, true, 4,
} },

{ SP_PURPLE_DRACONIAN, {
    "Dr",
    "Purple Draconian", "Draconian", "Draconian",
    SPF_DRACONIAN,
    -1, 1, 0,
    MONS_PURPLE_DRACONIAN,
    HT_LAND, US_ALIVE, SIZE_MEDIUM,
    10, 8, 6, // 24
    true, true, true, 4,
} },

{ SP_MOTTLED_DRACONIAN, {
    "Dr",
    "Mottled Draconian", "Draconian", "Draconian",
    SPF_DRACONIAN,
    -1, 1, 0,
    MONS_MOTTLED_DRACONIAN,
    HT_LAND, US_ALIVE, SIZE_MEDIUM,
    10, 8, 6, // 24
    true, true, true, 4,
} },

{ SP_PALE_DRACONIAN, {
    "Dr",
    "Pale Draconian", "Draconian", "Draconian",
    SPF_DRACONIAN,
    -1, 1, 0,
    MONS_PALE_DRACONIAN,
    HT_LAND, US_ALIVE, SIZE_MEDIUM,
    10, 8, 6, // 24
    true, true, true, 4,
} },

{ SP_DEMONSPAWN, {
    "Ds",
    "Demonspawn", "Demonic", nullptr,
    SPF_NONE,
    -1, 0, 0,
    MONS_DEMONSPAWN,
    HT_LAND, US_ALIVE, SIZE_MEDIUM,
    8, 9, 8, // 25
    true, true, true, 4,
} },

{ SP_FELID, {
    "Fe",
    "Felid", "Feline", "Cat",
    SPF_NONE,
    -1, -4, 2,
    MONS_FELID,
    HT_LAND, US_ALIVE, SIZE_LITTLE,
    4, 9, 11, // 24
    false, true, true, 5,
} },

{ SP_FORMICID, {
    "Fo",
    "Formicid", nullptr, "Ant",
    SPF_NONE,
    1, 0, 1,
    MONS_FORMICID,
    HT_LAND, US_ALIVE, SIZE_MEDIUM,
    12, 7, 6, // 25
    true, true, false, 4,
} },

{ SP_GHOUL, {
    "Gh",
    "Ghoul", "Ghoulish", nullptr,
    SPF_NO_HAIR,
    0, 1, -1,
    MONS_GHOUL,
    HT_LAND, US_HUNGRY_DEAD, SIZE_MEDIUM,
    11, 3, 4, // 18
    true, false, false, 5,
} },

{ SP_GARGOYLE, {
    "Gr",
    "Gargoyle", nullptr, nullptr,
    SPF_NO_HAIR,
    0, -2, 0,
    MONS_GARGOYLE,
    HT_LAND, US_ALIVE, SIZE_MEDIUM,
    11, 8, 5, // 24
    true, true, false, 4,
} },

{ SP_HALFLING, {
    "Ha",
    "Halfling", nullptr, nullptr,
    SPF_NONE,
    1, -1, 0,
    MONS_HALFLING,
    HT_LAND, US_ALIVE, SIZE_SMALL,
    8, 7, 9, // 24
    false, false, true, 5,
} },

{ SP_HIGH_ELF, {
    "HE",
    "High Elf", "Elven", "Elf",
    SPF_ELVEN,
    -1, -1, 2,
    MONS_ELF,
    HT_LAND, US_ALIVE, SIZE_MEDIUM,
    7, 11, 10, // 28
    false, true, true, 3,
} },

{ SP_HILL_ORC, {
    "HO",
    "Hill Orc", "Orcish", "Orc",
    SPF_ORCISH,
    0, 1, 0,
    MONS_ORC,
    HT_LAND, US_ALIVE, SIZE_MEDIUM,
    10, 8, 6, // 24
    true, false, false, 5,
} },

{ SP_HUMAN, {
    "Hu",
    "Human", nullptr, nullptr,
    SPF_NONE,
    1, 0, 0,
    MONS_HUMAN,
    HT_LAND, US_ALIVE, SIZE_MEDIUM,
    8, 8, 8, // 24
    true, true, true, 4,
} },

{ SP_KOBOLD, {
    "Ko",
    "Kobold", nullptr, nullptr,
    SPF_NONE,
    1, -2, 0,
    MONS_KOBOLD,
    HT_LAND, US_ALIVE, SIZE_SMALL,
    6, 6, 11, // 23
    true, false, true, 5,
} },

{ SP_MERFOLK, {
    "Mf",
    "Merfolk", "Merfolkian", nullptr,
    SPF_NONE,
    0, 0, 0,
    MONS_MERFOLK,
    HT_WATER, US_ALIVE, SIZE_MEDIUM,
    8, 7, 9, // 24
    true, true, true, 5,
} },

{ SP_MINOTAUR, {
    "Mi",
    "Minotaur", nullptr, nullptr,
    SPF_NONE,
    -1, 1, -2,
    MONS_MINOTAUR,
    HT_LAND, US_ALIVE, SIZE_MEDIUM,
    12, 5, 5, // 22
    true, false, true, 4,
} },

{ SP_MUMMY, {
    "Mu",
    "Mummy", nullptr, nullptr,
    SPF_NONE,
    -1, 0, 0,
    MONS_MUMMY,
    HT_LAND, US_UNDEAD, SIZE_MEDIUM,
    11, 7,  7, // 25
    false, false, false, 28, // No stat gain
} },

{ SP_NAGA, {
    "Na",
    "Naga", nullptr, nullptr,
    SPF_SMALL_TORSO,
    0, 2, 0,
    MONS_NAGA,
    HT_LAND, US_ALIVE, SIZE_LARGE,
    10, 8, 6, // 24
    true, true, true, 4,
} },

{ SP_OGRE, {
    "Og",
    "Ogre", "Ogreish", nullptr,
    SPF_NONE,
    0, 3, 0,
    MONS_OGRE,
    HT_LAND, US_ALIVE, SIZE_LARGE,
    12, 7, 5, // 24
    true, false, false, 3,
} },

{ SP_OCTOPODE, {
    "Op",
    "Octopode", "Octopoid", "Octopus",
    SPF_NO_HAIR,
    0, -1, 0,
    MONS_OCTOPODE,
    HT_WATER, US_ALIVE, SIZE_MEDIUM,
    7, 10, 7, // 24
    true, true, true, 5,
} },

{ SP_SPRIGGAN, {
    "Sp",
    "Spriggan", nullptr, nullptr,
    SPF_NONE,
    -1, -3, 3,
    MONS_SPRIGGAN,
    HT_LAND, US_ALIVE, SIZE_LITTLE,
    4, 9, 11, // 24
    false, true, true, 5,
} },

{ SP_TENGU, {
    "Te",
    "Tengu", nullptr, nullptr,
    SPF_NO_HAIR,
    0, -2, 1,
    MONS_TENGU,
    HT_LAND, US_ALIVE, SIZE_MEDIUM,
    8, 8, 9, // 25
    true, true, true, 4,
} },

{ SP_TROLL, {
    "Tr",
    "Troll", "Trollish", nullptr,
    SPF_NONE,
    -1, 3, -2,
    MONS_TROLL,
    HT_LAND, US_ALIVE, SIZE_LARGE,
    15, 4, 5, // 24
    true, false, false, 3,
} },

{ SP_VAMPIRE, {
    "Vp",
    "Vampire", "Vampiric", nullptr,
    SPF_NONE,
    -1, 0, 0,
    MONS_VAMPIRE,
    HT_LAND, US_SEMI_UNDEAD, SIZE_MEDIUM,
    7, 10, 9, // 26
    false, false, false, 28, // No stat gain
} },

{ SP_VINE_STALKER, {
    "VS",
    "Vine Stalker", "Vine", "Vine",
    SPF_NONE,
    0, -3, 1,
    MONS_VINE_STALKER,
    HT_LAND, US_ALIVE, SIZE_MEDIUM,
    10, 8, 9, // 27
    true, false, true, 4,
} },
#if TAG_MAJOR_VERSION == 34
{ SP_SLUDGE_ELF, {
    "SE",
    "Sludge Elf", "Elven", "Elf",
    SPF_ELVEN,
    0, -1, 1,
    MONS_ELF,
    HT_LAND, US_ALIVE, SIZE_MEDIUM,
    8, 8, 8, // 24
    false, true, true, 4,
} },

{ SP_LAVA_ORC, {
    "LO",
    "Lava Orc", "Orcish", "Orc",
    SPF_ORCISH | SPF_NO_HAIR,
    -1, 1, 0,
    MONS_LAVA_ORC,
    HT_AMPHIBIOUS_LAVA, US_ALIVE, SIZE_MEDIUM,
    10, 8, 6, // 24
    true, false, false, 5,
} },

{ SP_DJINNI, {
    "Dj",
    "Djinni", "Djinn", nullptr,
    SPF_NONE,
    -1, -1, 0,
    MONS_DJINNI,
    HT_LAND, US_ALIVE, SIZE_MEDIUM,
    8, 8, 8, // 24
    true, true, true, 4,
} },
#endif
// Ideally this wouldn't be necessary...
{ SP_UNKNOWN, {
    "??",
    "Yak", nullptr, nullptr,
    SPF_NONE,
    0, 0, 0,
    MONS_PROGRAM_BUG,
    HT_LAND, US_ALIVE, SIZE_MEDIUM,
    0, 0, 0, // 0
    false, false, false, 28,
} }
};
