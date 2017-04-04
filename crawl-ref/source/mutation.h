/**
 * @file
 * @brief Functions for handling player mutations.
**/

#pragma once

#include <string>

#include "equipment-type.h"
#include "mutation-type.h"

class formatted_string;

enum class mutation_activity_type
{
    INACTIVE, // form-based mutations in most forms
    PARTIAL,  // scales on statues
    FULL,     // other mutations
};

enum mutation_permanence_class
{
    // Temporary mutations wear off after awhile
    MUTCLASS_TEMPORARY,
    // Normal mutations, permanent unless cured
    MUTCLASS_NORMAL,
    // Innate, permanent traits, like draconian breath
    MUTCLASS_INNATE
};

void init_mut_index();

bool is_body_facet(mutation_type mut);
bool is_slime_mutation(mutation_type mut);
bool undead_mutation_rot();

bool mutate(mutation_type which_mutation, const string &reason,
            bool failMsg = true,
            bool force_mutation = false, bool god_gift = false,
            bool beneficial = false,
            mutation_permanence_class mutclass = MUTCLASS_NORMAL,
            bool no_rot = false);

void display_mutations();
int mut_check_conflict(mutation_type mut, bool innate_only = false);
mutation_activity_type mutation_activity_level(mutation_type mut);
string describe_mutations(bool center_title);

bool delete_mutation(mutation_type which_mutation, const string &reason,
                     bool failMsg = true,
                     bool force_mutation = false, bool god_gift = false,
                     bool disallow_mismatch = false);

bool delete_all_mutations(const string &reason);

const char* mutation_name(mutation_type mut, bool allow_category = false);
const char* category_mutation_name(mutation_type mut);
mutation_type mutation_from_name(string name, bool allow_category, vector<mutation_type> *partial_matches = nullptr);

string mut_upgrade_summary(mutation_type mut);
int mutation_max_levels(mutation_type mut);
string mutation_desc(mutation_type which_mutat, int level = -1,
                          bool colour = false, bool is_sacrifice = false);

void roll_demonspawn_mutations();

bool perma_mutate(mutation_type which_mut, int how_much, const string &reason);
bool temp_mutate(mutation_type which_mut, const string &reason);
int temp_mutation_roll();
bool temp_mutation_wanes();

void check_demonic_guardian();
void check_monster_detect();
equipment_type beastly_slot(int mut);
bool physiology_mutation_conflict(mutation_type mutat);
int augmentation_amount();
void reset_powered_by_death_duration();

bool delete_all_temp_mutations(const string &reason);
bool delete_temp_mutation();
