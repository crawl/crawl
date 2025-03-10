/**
 * @file
 * @brief Functions for handling player mutations.
**/

#pragma once

#include <string>
#include <vector>

#include "mutation-type.h"
#include "externs.h"

using std::vector;

class formatted_string;

#define EVOLUTION_MUTS_KEY "evolution_muts"

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

vector<mutation_type> get_removed_mutations();

void init_mut_index();

bool is_bad_mutation(mutation_type mut);
bool is_good_mutation(mutation_type mut);
bool is_body_facet(mutation_type mut);
bool is_slime_mutation(mutation_type mut);
bool is_makhleb_mark(mutation_type mut);

bool mutate(mutation_type which_mutation, const string &reason,
            bool failMsg = true,
            bool force_mutation = false, bool god_gift = false,
            bool beneficial = false,
            mutation_permanence_class mutclass = MUTCLASS_NORMAL);

int mut_check_conflict(mutation_type mut, bool innate_only = false);
mutation_activity_type mutation_activity_level(mutation_type mut);

void display_mutations();
string describe_mutations(bool center_title);
string terse_mutation_list();
string get_mutation_desc(mutation_type mut);

int get_mutation_cap(mutation_type mut);
void validate_mutations(bool debug_msg=false);

bool delete_mutation(mutation_type which_mutation, const string &reason,
                     bool failMsg = true,
                     bool force_mutation = false, bool god_gift = false);

bool delete_all_mutations(const string &reason);

const char* mutation_name(mutation_type mut, bool allow_category = false);
const char* category_mutation_name(mutation_type mut);
mutation_type mutation_from_name(string name, bool allow_category, vector<mutation_type> *partial_matches = nullptr);
mutation_type concretize_mut(mutation_type mut, mutation_permanence_class mutclass = MUTCLASS_NORMAL);

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
bool physiology_mutation_conflict(mutation_type mutat);
int augmentation_amount();
void reset_powered_by_death_duration();

bool delete_all_temp_mutations(const string &reason);
bool delete_temp_mutation();

tileidx_t get_mutation_tile(mutation_type mut);

void set_evolution_mut_xp(bool malignant);
