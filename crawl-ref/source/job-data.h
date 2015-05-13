struct job_def
{
    const char* abbrev; ///< Two-letter abbreviation
    const char* name; ///< Long name
    int s, i, d; ///< Starting Str, Dex, and Int
};

static const map<job_type, job_def> job_data =
{

{ JOB_ABYSSAL_KNIGHT, {
    "AK", "Abyssal Knight",
    4, 4, 4,
} },

{ JOB_AIR_ELEMENTALIST, {
    "AE", "Air Elementalist",
    0, 7, 5,
} },

{ JOB_ARCANE_MARKSMAN, {
    "AM", "Arcane Marksman",
    3, 5, 4,
} },

{ JOB_ARTIFICER, {
    "Ar", "Artificer",
    3, 4, 5,
} },

{ JOB_ASSASSIN, {
    "As", "Assassin",
    3, 3, 6,
} },

{ JOB_BERSERKER, {
    "Be", "Berserker",
    9, -1, 4,
} },

{ JOB_CHAOS_KNIGHT, {
    "CK", "Chaos Knight",
    4, 4, 4,
} },

{ JOB_CONJURER, {
    "Cj", "Conjurer",
    0, 7, 5,
} },

{ JOB_EARTH_ELEMENTALIST, {
    "EE", "Earth Elementalist",
    0, 7, 5,
} },

{ JOB_ENCHANTER, {
    "En", "Enchanter",
    0, 7, 5,
} },

{ JOB_FIGHTER, {
    "Fi", "Fighter",
    8, 0, 4,
} },

{ JOB_FIRE_ELEMENTALIST, {
    "FE", "Fire Elementalist",
    0, 7, 5,
} },

{ JOB_GLADIATOR, {
    "Gl", "Gladiator",
    7, 0, 5,
} },

{ JOB_HUNTER, {
    "Hu", "Hunter",
    4, 3, 5,
} },

{ JOB_ICE_ELEMENTALIST, {
    "IE", "Ice Elementalist",
    0, 7, 5,
} },

{ JOB_MONK, {
    "Mo", "Monk",
    3, 2, 7,
} },

{ JOB_NECROMANCER, {
    "Ne", "Necromancer",
    0, 7, 5,
} },

{ JOB_SKALD, {
    "Sk", "Skald",
    4, 4, 4,
} },

{ JOB_SUMMONER, {
    "Su", "Summoner",
    0, 7, 5,
} },

{ JOB_TRANSMUTER, {
    "Tm", "Transmuter",
    2, 5, 5,
} },

{ JOB_VENOM_MAGE, {
    "VM", "Venom Mage",
    0, 7, 5,
} },

{ JOB_WANDERER, {
    "Wn", "Wanderer",
    0, 0, 0, // Randomised
} },

{ JOB_WARPER, {
    "Wr", "Warper",
    3, 5, 4,
} },

{ JOB_WIZARD, {
    "Wz", "Wizard",
    -1, 10, 3,
} },
#if TAG_MAJOR_VERSION == 34
{ JOB_DEATH_KNIGHT, {
    "DK", "Death Knight",
    0, 0, 0,
} },

{ JOB_HEALER, {
    "He", "Healer",
    0, 0, 0,
} },

{ JOB_JESTER, {
    "Jr", "Jester",
    0, 0, 0,
} },

{ JOB_PRIEST, {
    "Pr", "Priest",
    0, 0, 0,
} },

{ JOB_STALKER, {
    "St", "Stalker",
    0, 0, 0,
} },
#endif
};
