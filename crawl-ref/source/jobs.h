#ifndef JOBS_H
#define JOBS_H

int ng_num_jobs();
job_type get_job(const int index);
const char *get_job_abbrev(int which_job);
job_type get_job_by_abbrev(const char *abbrev);
const char *get_job_name(int which_job);
job_type get_job_by_name(const char *name);

// job_type bounds checking.
bool is_valid_job(job_type job);

#endif
