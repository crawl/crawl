#include "AppHdr.h"

#include "jobs.h"

#include "libutil.h"

static const char * Job_Abbrev_List[] =
    { "Fi", "Wz",
#if TAG_MAJOR_VERSION == 34
      "Pr",
#endif
      "Gl", "Ne",
      "As", "Be", "Hu",
      "Cj", "En", "FE", "IE", "Su", "AE", "EE", "Sk",
      "VM",
      "CK", "Tm", "He",
#if TAG_MAJOR_VERSION == 34
      "St",
#endif
      "Mo", "Wr", "Wn", "Ar", "AM",
      "DK", "AK",
#if TAG_MAJOR_VERSION == 34
      "Jr",
#endif
};

static const char * Job_Name_List[] =
    { "Fighter", "Wizard",
#if TAG_MAJOR_VERSION == 34
      "Priest",
#endif
      "Gladiator", "Necromancer",
      "Assassin", "Berserker", "Hunter", "Conjurer", "Enchanter",
      "Fire Elementalist", "Ice Elementalist", "Summoner", "Air Elementalist",
      "Earth Elementalist", "Skald",
      "Venom Mage",
      "Chaos Knight", "Transmuter", "Healer",
#if TAG_MAJOR_VERSION == 34
      "Stalker",
#endif
      "Monk", "Warper", "Wanderer", "Artificer", "Arcane Marksman",
      "Death Knight", "Abyssal Knight",
#if TAG_MAJOR_VERSION == 34
      "Jester",
#endif
};

const char *get_job_abbrev(int which_job)
{
    if (which_job == JOB_UNKNOWN)
        return "Un";
    COMPILE_CHECK(ARRAYSZ(Job_Abbrev_List) == NUM_JOBS);
    ASSERT_RANGE(which_job, 0, NUM_JOBS);

    return Job_Abbrev_List[which_job];
}

job_type get_job_by_abbrev(const char *abbrev)
{
    int i;

    for (i = 0; i < NUM_JOBS; i++)
    {
        // This assumes untranslated abbreviations.
        if (toalower(abbrev[0]) == toalower(Job_Abbrev_List[i][0])
            && toalower(abbrev[1]) == toalower(Job_Abbrev_List[i][1]))
        {
            break;
        }
    }

    return (i < NUM_JOBS) ? static_cast<job_type>(i) : JOB_UNKNOWN;
}

const char *get_job_name(int which_job)
{
    if (which_job == JOB_UNKNOWN)
        return "Unemployed";
    COMPILE_CHECK(ARRAYSZ(Job_Name_List) == NUM_JOBS);
    ASSERT_RANGE(which_job, 0, NUM_JOBS);

    return Job_Name_List[which_job];
}

job_type get_job_by_name(const char *name)
{
    int i;
    job_type cl = JOB_UNKNOWN;

    string low_name = lowercase_string(name);

    for (i = 0; i < NUM_JOBS; i++)
    {
        string low_job = lowercase_string(Job_Name_List[i]);

        size_t pos = low_job.find(low_name);
        if (pos != string::npos)
        {
            cl = static_cast<job_type>(i);
            if (!pos)  // prefix takes preference
                break;
        }
    }

    return cl;
}

// Determines if a job is valid in a saved game. This is a pure bounds check.
bool is_valid_job(job_type job)
{
    return job >= 0 && job < NUM_JOBS;
}

// Determines if a job is valid for a new game.
bool is_job_valid_choice(job_type job)
{
    return is_valid_job(job)
#if TAG_MAJOR_VERSION == 34
        && job != JOB_STALKER && job != JOB_JESTER && job != JOB_PRIEST
#endif
        ;
}
