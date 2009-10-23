#ifndef JOBS_H
#define JOBS_H

int ng_num_classes();
job_type get_class(const int index);
int get_class_index_by_abbrev(const char *abbrev);
const char *get_class_abbrev(int which_job);
job_type get_class_by_abbrev(const char *abbrev);
int get_class_index_by_name(const char *name);
const char *get_class_name(int which_job);
job_type get_class_by_name(const char *name);

#endif
