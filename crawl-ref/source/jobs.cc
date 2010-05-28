#include "AppHdr.h"

#include "jobs.h"

#include "options.h"

// First plain fighters, then religious fighters, then spell-casting
// fighters, then primary spell-casters, then stabbers and shooters. (MM)
static job_type jobs_order[] = {
    // fighters
    JOB_FIGHTER,            JOB_GLADIATOR,
    JOB_MONK,               JOB_BERSERKER,
    // religious professions (incl. Berserker above)
    JOB_PALADIN,            JOB_PRIEST,
    JOB_HEALER,             JOB_CHAOS_KNIGHT,
    JOB_CRUSADER,
    // general and niche spellcasters (incl. Crusader above)
    JOB_REAVER,             JOB_WARPER,
    JOB_WIZARD,             JOB_CONJURER,
    JOB_ENCHANTER,          JOB_SUMMONER,
    JOB_NECROMANCER,        JOB_TRANSMUTER,
    JOB_FIRE_ELEMENTALIST,  JOB_ICE_ELEMENTALIST,
    JOB_AIR_ELEMENTALIST,   JOB_EARTH_ELEMENTALIST,
    // poison specialists and stabbers
    JOB_VENOM_MAGE,         JOB_STALKER,
    JOB_ASSASSIN,
    JOB_HUNTER,             JOB_ARTIFICER,
    JOB_ARCANE_MARKSMAN,    JOB_WANDERER
};

job_type get_job(const int index)
{
    if (index < 0 || index >= ng_num_jobs())
        return (JOB_UNKNOWN);

    return (jobs_order[index]);
}

static const char * Job_Abbrev_List[ NUM_JOBS ] =
    { "Fi", "Wz", "Pr",
      "Gl", "Ne", "Pa", "As", "Be", "Hu",
      "Cj", "En", "FE", "IE", "Su", "AE", "EE", "Cr",
      "VM",
      "CK", "Tm", "He", "Re", "St", "Mo", "Wr", "Wn", "Ar", "AM" };

static const char * Job_Name_List[ NUM_JOBS ] =
    { "Fighter", "Wizard", "Priest",
      "Gladiator", "Necromancer",
      "Paladin", "Assassin", "Berserker", "Hunter", "Conjurer", "Enchanter",
      "Fire Elementalist", "Ice Elementalist", "Summoner", "Air Elementalist",
      "Earth Elementalist", "Crusader",
      "Venom Mage",
      "Chaos Knight", "Transmuter", "Healer", "Reaver", "Stalker",
      "Monk", "Warper", "Wanderer", "Artificer", "Arcane Marksman" };

int get_job_index_by_abbrev(const char *abbrev)
{
    COMPILE_CHECK(ARRAYSZ(Job_Abbrev_List) == NUM_JOBS, c1);

    unsigned int job;
    for (unsigned int i = 0; i < ARRAYSZ(jobs_order); i++)
    {
        job = jobs_order[i];

        if (tolower( abbrev[0] ) == tolower( Job_Abbrev_List[job][0] )
            && tolower( abbrev[1] ) == tolower( Job_Abbrev_List[job][1] ))
        {
            return i;
        }
    }

    return (-1);
}

const char *get_job_abbrev(int which_job)
{
    ASSERT(which_job >= 0 && which_job < NUM_JOBS);

    return (Job_Abbrev_List[which_job]);
}

job_type get_job_by_abbrev(const char *abbrev)
{
    int i;

    for (i = 0; i < NUM_JOBS; i++)
    {
        if (tolower(abbrev[0]) == tolower(Job_Abbrev_List[i][0])
            && tolower(abbrev[1]) == tolower(Job_Abbrev_List[i][1]))
        {
            break;
        }
    }

    return ((i < NUM_JOBS) ? static_cast<job_type>(i) : JOB_UNKNOWN);
}

int get_job_index_by_name(const char *name)
{
    COMPILE_CHECK(ARRAYSZ(Job_Name_List) == NUM_JOBS, c1);

    char *ptr;
    char lowered_buff[80];
    char lowered_job[80];

    strncpy(lowered_buff, name, sizeof(lowered_buff));
    strlwr(lowered_buff);

    int cl = -1;
    unsigned int job;
    for (unsigned int i = 0; i < ARRAYSZ(jobs_order); i++)
    {
        job = jobs_order[i];

        strncpy( lowered_job, Job_Name_List[job], sizeof( lowered_job ) );
        strlwr( lowered_job );

        ptr = strstr( lowered_job, lowered_buff );
        if (ptr != NULL)
        {
            cl = i;
            if (ptr == lowered_job)  // prefix takes preference
                break;
        }
    }

    return (cl);
}

const char *get_job_name( int which_job )
{
    ASSERT(which_job >= 0 && which_job < NUM_JOBS);

    return (Job_Name_List[which_job]);
}

job_type get_job_by_name(const char *name)
{
    int i;
    job_type cl = JOB_UNKNOWN;

    char *ptr;
    char lowered_buff[80];
    char lowered_job[80];

    strncpy(lowered_buff, name, sizeof(lowered_buff));
    strlwr(lowered_buff);

    for (i = 0; i < NUM_JOBS; i++)
    {
        strncpy(lowered_job, Job_Name_List[i], sizeof(lowered_job));
        strlwr(lowered_job);

        ptr = strstr(lowered_job, lowered_buff);
        if (ptr != NULL)
        {
            cl = static_cast<job_type>(i);
            if (ptr == lowered_job)  // prefix takes preference
                break;
        }
    }

    return (cl);
}

int ng_num_jobs()
{
    // The list musn't be longer than the number of actual jobs.
    COMPILE_CHECK(ARRAYSZ(jobs_order) <= NUM_JOBS, c1);
    return ARRAYSZ(jobs_order);
}

bool is_valid_job(job_type job)
{
    return (job >= 0 && job < NUM_JOBS);
}
