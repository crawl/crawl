#include "AppHdr.h"

#include "jobs.h"

#include "libutil.h"
#include "stringutil.h"

#include "job-data.h"

static const job_def& _job_def(job_type job)
{
    ASSERT_RANGE(job, 0, NUM_JOBS);
    return job_data.at(job);
}

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
      "Chaos Knight", "Transmuter",
#if TAG_MAJOR_VERSION == 34
      "Healer", "Stalker",
#endif
      "Monk", "Warper", "Wanderer", "Artificer", "Arcane Marksman",
#if TAG_MAJOR_VERSION == 34
      "Death Knight",
#endif
      "Abyssal Knight",
#if TAG_MAJOR_VERSION == 34
      "Jester",
#endif
};

const char *get_job_abbrev(job_type which_job)
{
    if (which_job == JOB_UNKNOWN)
        return "Un";
    return _job_def(which_job).abbrev;
}

job_type get_job_by_abbrev(const char *abbrev)
{
    for (auto& entry : job_data)
        if (lowercase_string(abbrev) == lowercase_string(entry.second.abbrev))
            return entry.first;

    return JOB_UNKNOWN;
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
    job_type cl = JOB_UNKNOWN;

    string low_name = lowercase_string(name);

    for (int i = 0; i < NUM_JOBS; i++)
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

// Determines if a job is valid for a new game.
bool is_starting_job(job_type job)
{
    return job >= 0 && job < NUM_JOBS
#if TAG_MAJOR_VERSION == 34
        && job != JOB_STALKER && job != JOB_JESTER && job != JOB_PRIEST
        && job != JOB_DEATH_KNIGHT && job != JOB_HEALER
#endif
        ;
}
