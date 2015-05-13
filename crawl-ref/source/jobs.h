#ifndef JOBS_H
#define JOBS_H

const char *get_job_abbrev(job_type which_job);
job_type get_job_by_abbrev(const char *abbrev);
const char *get_job_name(job_type which_job);
job_type get_job_by_name(const char *name);
void job_stat_init(job_type job);
bool job_recommends_species(job_type job, species_type species);

// Is the job valid for a new game?
bool is_starting_job(job_type job);

#endif
