#pragma once

#include "item-prop-enum.h"
#include "job-type.h"
#include "species-type.h"

const char *get_job_abbrev(job_type which_job);
job_type get_job_by_abbrev(const char *abbrev);
const char *get_job_name(job_type which_job);
job_type get_job_by_name(const char *name);
bool job_recommends_species(job_type job, species_type species);

bool job_has_weapon_choice(job_type job);
bool job_gets_good_weapons(job_type job);
bool job_gets_ranged_weapons(job_type job);
void give_job_equipment(job_type job);
void give_job_skills(job_type job);

void job_stat_init(job_type job);

void debug_jobdata();
