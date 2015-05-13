struct job_def
{
    const char* abbrev; ///< Two-letter abbreviation
};

static const map<job_type, job_def> job_data =
{

{ JOB_ABYSSAL_KNIGHT, {
    "AK",
} },

{ JOB_AIR_ELEMENTALIST, {
    "AE",
} },

{ JOB_ARCANE_MARKSMAN, {
    "AM",
} },

{ JOB_ARTIFICER, {
    "Ar",
} },

{ JOB_ASSASSIN, {
    "As",
} },

{ JOB_BERSERKER, {
    "Be",
} },

{ JOB_CHAOS_KNIGHT, {
    "CK",
} },

{ JOB_CONJURER, {
    "Cj",
} },

{ JOB_EARTH_ELEMENTALIST, {
    "EE",
} },

{ JOB_ENCHANTER, {
    "En",
} },

{ JOB_FIGHTER, {
    "Fi",
} },

{ JOB_FIRE_ELEMENTALIST, {
    "FE",
} },

{ JOB_GLADIATOR, {
    "Gl",
} },

{ JOB_HUNTER, {
    "Hu",
} },

{ JOB_ICE_ELEMENTALIST, {
    "IE",
} },

{ JOB_MONK, {
    "Mo",
} },

{ JOB_NECROMANCER, {
    "Ne",
} },

{ JOB_SKALD, {
    "Sk",
} },

{ JOB_SUMMONER, {
    "Su",
} },

{ JOB_TRANSMUTER, {
    "Tm",
} },

{ JOB_VENOM_MAGE, {
    "VM",
} },

{ JOB_WANDERER, {
    "Wn",
} },

{ JOB_WARPER, {
    "Wr",
} },

{ JOB_WIZARD, {
    "Wz",
} },
#if TAG_MAJOR_VERSION == 34
{ JOB_DEATH_KNIGHT, {
    "DK",
} },

{ JOB_HEALER, {
    "He",
} },

{ JOB_JESTER, {
    "Jr",
} },

{ JOB_PRIEST, {
    "Pr",
} },

{ JOB_STALKER, {
    "St",
} },
#endif
};
