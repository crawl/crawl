struct job_def
{
    const char* abbrev; ///< Two-letter abbreviation
    const char* name; ///< Long name
};

static const map<job_type, job_def> job_data =
{

{ JOB_ABYSSAL_KNIGHT, {
    "AK", "Abyssal Knight",
} },

{ JOB_AIR_ELEMENTALIST, {
    "AE", "Air Elementalist",
} },

{ JOB_ARCANE_MARKSMAN, {
    "AM", "Arcane Marksman",
} },

{ JOB_ARTIFICER, {
    "Ar", "Artificer",
} },

{ JOB_ASSASSIN, {
    "As", "Assassin",
} },

{ JOB_BERSERKER, {
    "Be", "Berserker",
} },

{ JOB_CHAOS_KNIGHT, {
    "CK", "Chaos Knight",
} },

{ JOB_CONJURER, {
    "Cj", "Conjurer",
} },

{ JOB_EARTH_ELEMENTALIST, {
    "EE", "Earth Elementalist",
} },

{ JOB_ENCHANTER, {
    "En", "Enchanter",
} },

{ JOB_FIGHTER, {
    "Fi", "Fighter",
} },

{ JOB_FIRE_ELEMENTALIST, {
    "FE", "Fire Elementalist",
} },

{ JOB_GLADIATOR, {
    "Gl", "Gladiator",
} },

{ JOB_HUNTER, {
    "Hu", "Hunter",
} },

{ JOB_ICE_ELEMENTALIST, {
    "IE", "Ice Elementalist",
} },

{ JOB_MONK, {
    "Mo", "Monk",
} },

{ JOB_NECROMANCER, {
    "Ne", "Necromancer",
} },

{ JOB_SKALD, {
    "Sk", "Skald",
} },

{ JOB_SUMMONER, {
    "Su", "Summoner",
} },

{ JOB_TRANSMUTER, {
    "Tm", "Transmuter",
} },

{ JOB_VENOM_MAGE, {
    "VM", "Venom Mage",
} },

{ JOB_WANDERER, {
    "Wn", "Wanderer",
} },

{ JOB_WARPER, {
    "Wr", "Warper",
} },

{ JOB_WIZARD, {
    "Wz", "Wizard",
} },
#if TAG_MAJOR_VERSION == 34
{ JOB_DEATH_KNIGHT, {
    "DK", "Death Knight",
} },

{ JOB_HEALER, {
    "He", "Healer",
} },

{ JOB_JESTER, {
    "Jr", "Jester",
} },

{ JOB_PRIEST, {
    "Pr", "Priest",
} },

{ JOB_STALKER, {
    "St", "Stalker",
} },
#endif
};
