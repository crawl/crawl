/**
 * @file
 * @brief Report which species, jobs and combos are playable.
**/

#pragma once

#include <vector>
#include "enum.h"
#include "job-type.h"
#include "species-type.h"

struct combo_type
{
    species_type species;
    job_type job;

    string abbr() const;
};

template <typename Enum, typename Predicate>
vector<Enum> filter_enum(Enum max, Predicate filter);

vector<job_type> playable_jobs();
vector<species_type> playable_species();
vector<combo_type> playable_combos();
vector<string> playable_job_names();
vector<string> playable_species_names();
vector<string> playable_combo_names();

string playable_metadata_json();
