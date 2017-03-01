#pragma once

#include "enum.h"
#include "ability-type.h"
#include "item-prop-enum.h"
#include "job-type.h"
#include "size-part-type.h"
#include "size-type.h"
#include "species-type.h"
#include "undead-state-type.h"

bool species_is_elven(species_type species);
bool species_is_draconian(species_type species);
bool species_is_orcish(species_type species);
bool species_has_hair(species_type species);
bool species_can_throw_large_rocks(species_type species);

bool species_has_claws(species_type species);
undead_state_type species_undead_type(species_type species) PURE;
bool species_is_undead(species_type species);
bool species_is_unbreathing(species_type species);
bool species_can_swim(species_type species);
bool species_likes_water(species_type species);
bool species_likes_lava(species_type species);
size_type species_size(species_type species,
                       size_part_type psize = PSIZE_TORSO);
bool species_recommends_job(species_type species, job_type job);
bool species_recommends_weapon(species_type species, weapon_type wpn);

species_type get_species_by_abbrev(const char *abbrev);
const char *get_species_abbrev(species_type which_species);

// from player.cc
enum species_name_type
{
    SPNAME_PLAIN,
    SPNAME_GENUS,
    SPNAME_ADJ
};
string species_name(species_type speci, species_name_type spname = SPNAME_PLAIN);
species_type str_to_species(const string &species);
string species_walking_verb(species_type sp);
const vector<string>& fake_mutations(species_type species, bool terse);

monster_type dragon_form_dragon_type();
const char* scale_type(species_type species);
ability_type draconian_breath(species_type species);

monster_type player_species_to_mons_species(species_type species);
string species_prayer_action(species_type species);

void give_basic_mutations(species_type species);
void give_level_mutations(species_type species, int xp_level);

int species_exp_modifier(species_type species);
int species_hp_modifier(species_type species);
int species_mp_modifier(species_type species);
int species_mr_modifier(species_type species);

void species_stat_init(species_type species);
void species_stat_gain(species_type species);
bool species_has_low_str(species_type species);

void change_species_to(species_type sp);
