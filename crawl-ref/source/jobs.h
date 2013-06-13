#ifndef JOBS_H
#define JOBS_H

const char *get_job_abbrev(int which_job);
job_type get_job_by_abbrev(const char *abbrev);
const char *get_job_name(int which_job);
job_type get_job_by_name(const char *name);

// job_type bounds checking.
bool is_valid_job(job_type job);

// Is the job valid for a new game?
bool is_job_valid_choice(job_type job);

#endif
