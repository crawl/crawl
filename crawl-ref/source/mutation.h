/**
 * @file
 * @brief Functions for handling player mutations.
**/

#pragma once

#include <string>
#include <vector>

#include "bane-type.h"
#include "mutation-type.h"
#include "transformation.h"
#include "externs.h"

using std::vector;

class formatted_string;

#define EVOLUTION_MUTS_KEY "evolution_muts"

#define HOARD_POTIONS_TIMER_KEY "hoard_potions_timer"
#define HOARD_SCROLLS_TIMER_KEY "hoard_scrolls_timer"

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

bool _delete_single_mutation_level(mutation_type mutat,
                                   const string &reason, bool transient);

int mut_check_conflict(mutation_type mut, bool innate_only = false);
bool mut_is_compatible(mutation_type mut, bool base_only = false);

void display_mutations();
string describe_mutations(bool center_title);
string terse_mutation_list();
string get_mutation_desc(mutation_type mut);
string get_mutation_tags(mutation_type mut);

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
bool temp_mutation_wanes();
int temp_mutation_count();

void check_demonic_guardian();
void check_monster_detect();
int augmentation_amount();
void reset_powered_by_death_duration();
int protean_grace_amount();

bool delete_all_temp_mutations(const string &reason);
bool delete_temp_mutation();

tileidx_t get_mutation_tile(mutation_type mut);

void set_evolution_mut_xp(bool malignant);

const string bane_desc(bane_type bane);
const string bane_name(bane_type mut, bool dbkey = false);
int bane_base_duration(bane_type bane);
bane_type bane_from_name(string name);
bool add_bane(bane_type bane = NUM_BANES, int duration = 0);
void remove_bane(bane_type bane);
int xl_to_remove_bane(bane_type bane);

bool skill_has_dilettante_penalty(skill_type skill);

void maybe_apply_bane_to_monster(monster& mons);
