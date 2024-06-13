#pragma once

#include <vector>

#include "item-prop-enum.h"
#include "job-def.h"
#include "job-type.h"
#include "species-type.h"
#include "spell-type.h"

using std::vector;

const char *get_job_abbrev(job_type which_job);
job_type get_job_by_abbrev(const char *abbrev);
const char *get_job_name(job_type which_job);
job_type get_job_by_name(const char *name);
bool job_recommends_species(job_type job, species_type species);
vector<species_type> job_recommended_species(job_type job);

bool job_has_weapon_choice(job_type job);
bool job_gets_good_weapons(job_type job);
void give_job_equipment(job_type job);
void give_job_skills(job_type job);
vector<spell_type> get_job_spells(job_type job);

void job_stat_init(job_type job);

void debug_jobdata();
job_type random_starting_job();
bool is_starting_job(job_type job);

bool job_is_removed(job_type job);

static inline bool job_type_valid(job_type job)
{
    return 0 <= job && job < NUM_JOBS;
}
