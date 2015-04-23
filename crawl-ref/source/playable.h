/**
 * @file
 * @brief Report which species, jobs and combos are playable.
**/

#ifndef PLAYABLE_H
#define PLAYABLE_H

#include <vector>
#include "enum.h"

struct combo_type
{
    species_type species;
    job_type job;

    string abbr() const;
};

vector<job_type> playable_jobs();
vector<species_type> playable_species();
vector<combo_type> playable_combos();
vector<string> playable_job_names();
vector<string> playable_species_names();
vector<string> playable_combo_names();

#endif
