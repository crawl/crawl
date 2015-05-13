#include "AppHdr.h"

#include "jobs.h"

#include "enum.h"
#include "libutil.h"
#include "player.h"
#include "stringutil.h"

#include "job-data.h"

static const job_def& _job_def(job_type job)
{
    ASSERT_RANGE(job, 0, NUM_JOBS);
    return job_data.at(job);
}

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

const char *get_job_name(job_type which_job)
{
    if (which_job == JOB_UNKNOWN)
        return "Unemployed";

    return _job_def(which_job).name;
}

job_type get_job_by_name(const char *name)
{
    job_type job = JOB_UNKNOWN;

    const string low_name = lowercase_string(name);

    for (auto& entry : job_data)
    {
        string low_job = lowercase_string(entry.second.name);

        const size_t pos = low_job.find(low_name);
        if (pos != string::npos)
        {
            job = entry.first;
            if (!pos)  // prefix takes preference
                break;
        }
    }

    return job;
}

// Must be called after species_stat_init for the wanderer formula to work.
void job_stat_init(job_type job)
{
    you.hp_max_adj_perm = 0;

    you.base_stats[STAT_STR] += _job_def(job).s;
    you.base_stats[STAT_INT] += _job_def(job).i;
    you.base_stats[STAT_DEX] += _job_def(job).d;

    if (job == JOB_WANDERER)
    {
        for (int i = 0; i < 12; i++)
        {
            const stat_type stat = static_cast<stat_type>(random2(NUM_STATS));
            // Stats that are already high will be chosen half as often.
            if (you.base_stats[stat] > 17 && coinflip())
            {
                i--;
                continue;
            }

            you.base_stats[stat]++;
        }
    }
}

bool job_recommends_species(job_type job, species_type species)
{
    return find(_job_def(job).recommended_species.begin(),
                _job_def(job).recommended_species.end(),
                species) != _job_def(job).recommended_species.end();
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
