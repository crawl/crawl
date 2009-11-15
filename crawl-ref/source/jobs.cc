#include "AppHdr.h"

#include "jobs.h"

#include "options.h"

// Listed in two columns to match the selection screen output.
// Take care to list all valid classes here, or they cannot be directly chosen.
// The old and new lists are expected to have the same length.
static job_type old_jobs_order[] = {
    JOB_FIGHTER,            JOB_WIZARD,
    JOB_PRIEST,             JOB_THIEF,
    JOB_GLADIATOR,          JOB_NECROMANCER,
    JOB_PALADIN,            JOB_ASSASSIN,
    JOB_BERSERKER,          JOB_HUNTER,
    JOB_CONJURER,           JOB_ENCHANTER,
    JOB_FIRE_ELEMENTALIST,  JOB_ICE_ELEMENTALIST,
    JOB_SUMMONER,           JOB_AIR_ELEMENTALIST,
    JOB_EARTH_ELEMENTALIST, JOB_CRUSADER,
    JOB_DEATH_KNIGHT,       JOB_VENOM_MAGE,
    JOB_CHAOS_KNIGHT,       JOB_TRANSMUTER,
    JOB_HEALER,             JOB_REAVER,
    JOB_STALKER,            JOB_MONK,
    JOB_WARPER,             JOB_WANDERER,
    JOB_ARTIFICER,          JOB_ARCANE_MARKSMAN
};

// First plain fighters, then religious fighters, then spell-casting
// fighters, then primary spell-casters, then stabbers and shooters. (MM)
static job_type new_jobs_order[] = {
    // fighters
    JOB_FIGHTER,            JOB_GLADIATOR,
    JOB_MONK,               JOB_BERSERKER,
    // religious professions (incl. Berserker above)
    JOB_PALADIN,            JOB_PRIEST,
    JOB_HEALER,             JOB_CHAOS_KNIGHT,
    JOB_DEATH_KNIGHT,       JOB_CRUSADER,
    // general and niche spellcasters (incl. Crusader above)
    JOB_REAVER,             JOB_WARPER,
    JOB_WIZARD,             JOB_CONJURER,
    JOB_ENCHANTER,          JOB_SUMMONER,
    JOB_NECROMANCER,        JOB_TRANSMUTER,
    JOB_FIRE_ELEMENTALIST,  JOB_ICE_ELEMENTALIST,
    JOB_AIR_ELEMENTALIST,   JOB_EARTH_ELEMENTALIST,
    // poison specialists and stabbers
    JOB_VENOM_MAGE,         JOB_STALKER,
    JOB_THIEF,              JOB_ASSASSIN,
    JOB_HUNTER,             JOB_ARTIFICER,
    JOB_ARCANE_MARKSMAN,    JOB_WANDERER
};

job_type get_class(const int index)
{
    if (index < 0 || index >= ng_num_classes())
        return (JOB_UNKNOWN);

    return (Options.use_old_selection_order? old_jobs_order[index]
                                           : new_jobs_order[index]);
}

static const char * Class_Abbrev_List[ NUM_JOBS ] =
    { "Fi", "Wz", "Pr", "Th", "Gl", "Ne", "Pa", "As", "Be", "Hu",
      "Cj", "En", "FE", "IE", "Su", "AE", "EE", "Cr", "DK", "VM",
      "CK", "Tm", "He", "Re", "St", "Mo", "Wr", "Wn", "Ar", "AM" };

static const char * Class_Name_List[ NUM_JOBS ] =
    { "Fighter", "Wizard", "Priest", "Thief", "Gladiator", "Necromancer",
      "Paladin", "Assassin", "Berserker", "Hunter", "Conjurer", "Enchanter",
      "Fire Elementalist", "Ice Elementalist", "Summoner", "Air Elementalist",
      "Earth Elementalist", "Crusader", "Death Knight", "Venom Mage",
      "Chaos Knight", "Transmuter", "Healer", "Reaver", "Stalker",
      "Monk", "Warper", "Wanderer", "Artificer", "Arcane Marksman" };

int get_class_index_by_abbrev(const char *abbrev)
{
    COMPILE_CHECK(ARRAYSZ(Class_Abbrev_List) == NUM_JOBS, c1);

    unsigned int job;
    for (unsigned int i = 0; i < ARRAYSZ(old_jobs_order); i++)
    {
        job = (Options.use_old_selection_order ? old_jobs_order[i]
                                               : new_jobs_order[i]);

        if (tolower( abbrev[0] ) == tolower( Class_Abbrev_List[job][0] )
            && tolower( abbrev[1] ) == tolower( Class_Abbrev_List[job][1] ))
        {
            return i;
        }
    }

    return (-1);
}

const char *get_class_abbrev(int which_job)
{
    ASSERT(which_job >= 0 && which_job < NUM_JOBS);

    return (Class_Abbrev_List[which_job]);
}

job_type get_class_by_abbrev(const char *abbrev)
{
    int i;

    for (i = 0; i < NUM_JOBS; i++)
    {
        if (tolower(abbrev[0]) == tolower(Class_Abbrev_List[i][0])
            && tolower(abbrev[1]) == tolower(Class_Abbrev_List[i][1]))
        {
            break;
        }
    }

    return ((i < NUM_JOBS) ? static_cast<job_type>(i) : JOB_UNKNOWN);
}

int get_class_index_by_name(const char *name)
{
    COMPILE_CHECK(ARRAYSZ(Class_Name_List) == NUM_JOBS, c1);

    char *ptr;
    char lowered_buff[80];
    char lowered_class[80];

    strncpy(lowered_buff, name, sizeof(lowered_buff));
    strlwr(lowered_buff);

    int cl = -1;
    unsigned int job;
    for (unsigned int i = 0; i < ARRAYSZ(old_jobs_order); i++)
    {
        job = (Options.use_old_selection_order ? old_jobs_order[i]
                                               : new_jobs_order[i]);

        strncpy( lowered_class, Class_Name_List[job], sizeof( lowered_class ) );
        strlwr( lowered_class );

        ptr = strstr( lowered_class, lowered_buff );
        if (ptr != NULL)
        {
            cl = i;
            if (ptr == lowered_class)  // prefix takes preference
                break;
        }
    }

    return (cl);
}

const char *get_class_name( int which_job )
{
    ASSERT(which_job >= 0 && which_job < NUM_JOBS);

    return (Class_Name_List[which_job]);
}

job_type get_class_by_name(const char *name)
{
    int i;
    job_type cl = JOB_UNKNOWN;

    char *ptr;
    char lowered_buff[80];
    char lowered_class[80];

    strncpy(lowered_buff, name, sizeof(lowered_buff));
    strlwr(lowered_buff);

    for (i = 0; i < NUM_JOBS; i++)
    {
        strncpy(lowered_class, Class_Name_List[i], sizeof(lowered_class));
        strlwr(lowered_class);

        ptr = strstr(lowered_class, lowered_buff);
        if (ptr != NULL)
        {
            cl = static_cast<job_type>(i);
            if (ptr == lowered_class)  // prefix takes preference
                break;
        }
    }

    return (cl);
}

int ng_num_classes()
{
    // The list musn't be longer than the number of actual classes.
    COMPILE_CHECK(ARRAYSZ(old_jobs_order) <= NUM_JOBS, c1);
    // Check whether the two lists have the same size.
    COMPILE_CHECK(ARRAYSZ(old_jobs_order) == ARRAYSZ(new_jobs_order), c2);
    return ARRAYSZ(old_jobs_order);
}

bool is_valid_job(job_type job)
{
    return (job >= 0 && job < NUM_JOBS);
}
