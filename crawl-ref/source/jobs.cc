#include "AppHdr.h"

#include "jobs.h"

#include "libutil.h"
#include "options.h"

static const char * Job_Abbrev_List[ NUM_JOBS ] =
    { "Fi", "Wz", "Pr",
      "Gl", "Ne",
#if TAG_MAJOR_VERSION == 32
      "Pa",
#endif
      "As", "Be", "Hu",
      "Cj", "En", "FE", "IE", "Su", "AE", "EE", "Sk",
      "VM",
      "CK", "Tm", "He",
#if TAG_MAJOR_VERSION == 32
      "Re",
#endif
      "St", "Mo", "Wr", "Wn", "Ar", "AM",
      "DK", "AK" };

static const char * Job_Name_List[ NUM_JOBS ] =
    { "Fighter", "Wizard", "Priest",
      "Gladiator", "Necromancer",
#if TAG_MAJOR_VERSION == 32
      "Paladin",
#endif
      "Assassin", "Berserker", "Hunter", "Conjurer", "Enchanter",
      "Fire Elementalist", "Ice Elementalist", "Summoner", "Air Elementalist",
      "Earth Elementalist", "Skald",
      "Venom Mage",
      "Chaos Knight", "Transmuter", "Healer",
#if TAG_MAJOR_VERSION == 32
      "Reaver",
#endif
      "Stalker",
      "Monk", "Warper", "Wanderer", "Artificer", "Arcane Marksman",
      "Death Knight", "Abyssal Knight" };

const char *get_job_abbrev(int which_job)
{
    if (which_job == JOB_UNKNOWN)
        return "Un";
    COMPILE_CHECK(ARRAYSZ(Job_Abbrev_List) == NUM_JOBS);
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

const char *get_job_name(int which_job)
{
    if (which_job == JOB_UNKNOWN)
        return "Unemployed";
    COMPILE_CHECK(ARRAYSZ(Job_Name_List) == NUM_JOBS);
    ASSERT(which_job >= 0 && which_job < NUM_JOBS);

    return (Job_Name_List[which_job]);
}

job_type get_job_by_name(const char *name)
{
    int i;
    job_type cl = JOB_UNKNOWN;

    std::string low_name = lowercase_string(name);

    for (i = 0; i < NUM_JOBS; i++)
    {
        std::string low_job = lowercase_string(Job_Name_List[i]);

        size_t pos = low_job.find(low_name);
        if (pos != std::string::npos)
        {
            cl = static_cast<job_type>(i);
            if (!pos)  // prefix takes preference
                break;
        }
    }

    return (cl);
}

bool is_valid_job(job_type job)
{
    return (job >= 0 && job < NUM_JOBS);
}
