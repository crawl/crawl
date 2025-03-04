#pragma once

#include <vector>

#include "enum.h"
#include "ability-type.h"
#include "item-prop-enum.h"
#include "job-type.h"
#include "size-part-type.h"
#include "size-type.h"
#include "species-def.h"
#include "species-type.h"

using std::vector;

const species_def& get_species_def(species_type species);

namespace species
{
    enum name_type
    {
        SPNAME_PLAIN,
        SPNAME_GENUS,
        SPNAME_ADJ
    };

    string name(species_type speci, name_type spname = SPNAME_PLAIN);
    const char *get_abbrev(species_type which_species);
    species_type from_abbrev(const char *abbrev);
    species_type from_str(const string &species);
    species_type from_str_loose(const string &species_str,
                                                    bool initial_only = false);

    bool is_elven(species_type species);
    bool is_undead(species_type species);
    bool is_draconian(species_type species);
    undead_state_type undead_type(species_type species) PURE;
    monster_type to_mons_species(species_type species);

    monster_type dragon_form(species_type s);
    const char* scale_type(species_type species);
    ability_type draconian_breath(species_type species);
    species_type random_draconian_colour();

    int mutation_level(species_type species, mutation_type mut, int mut_level=1);
    const vector<string>& fake_mutations(species_type species, bool terse);

    bool has_blood(species_type species);
    bool has_hair(species_type species);
    bool has_bones(species_type species);
    bool has_feet(species_type species);
    bool has_ears(species_type species);

    bool can_throw_large_rocks(species_type species);
    bool wears_barding(species_type species);
    bool has_claws(species_type species);
    bool is_nonliving(species_type species);
    bool can_swim(species_type species);
    bool likes_water(species_type species);
    size_type size(species_type species, size_part_type psize = PSIZE_TORSO);

    string walking_verb(species_type sp);
    string walking_title(species_type sp);
    string child_name(species_type species);
    string orc_name(species_type species);
    string prayer_action(species_type species);
    string shout_verb(species_type sp, int screaminess, bool directed);
    string skin_name(species_type sp, bool adj=false);
    string arm_name(species_type species);
    string hand_name(species_type species);
    int arm_count(species_type species);

    int get_exp_modifier(species_type species);
    int get_hp_modifier(species_type species);
    int get_mp_modifier(species_type species);
    int get_wl_modifier(species_type species);
    bool has_low_str(species_type species);
    bool recommends_job(species_type species, job_type job);
    bool recommends_weapon(species_type species, weapon_type wpn);

    bool is_valid(species_type species);
    bool is_starting_species(species_type species);
    species_type random_starting_species();
    bool is_removed(species_type species);
    vector<species_type> get_all_species();
}

#define DRACONIAN_BREATH_USES_KEY "drac_breath_uses"
#define DRACONIAN_BREATH_RECHARGE_KEY "drac_breath_recharge"
const int MAX_DRACONIAN_BREATH = 3;

int draconian_breath_uses_available();
bool gain_draconian_breath_uses(int num);

void species_stat_init(species_type species);
void species_stat_gain(species_type species);

void give_basic_mutations(species_type species);
void give_level_mutations(species_type species, int xp_level);

void change_species_to(species_type sp);
