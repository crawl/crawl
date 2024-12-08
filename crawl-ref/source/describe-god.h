/**
 * @file
 * @brief Functions used to print information about gods.
 **/

#pragma once

#include "god-type.h"
#include "species-type.h"

int god_favour_rank(god_type which_god);
string god_title(god_type which_god, species_type which_species, int piety);
void describe_god(god_type which_god);
bool describe_god_with_join(god_type which_god);
