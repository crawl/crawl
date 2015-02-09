#ifndef SPECIES_H
#define SPECIES_H

#include "enum.h"

enum genus_type
{
    GENPC_DRACONIAN,
    GENPC_ELVEN,
    GENPC_ORCISH,
    GENPC_NONE,
};

genus_type species_genus(species_type species);
int species_has_claws(species_type species, bool mut_level = false);
undead_state_type species_undead_type(species_type species) PURE;
bool species_is_undead(species_type species);
bool species_is_unbreathing(species_type species);
bool species_can_swim(species_type species);
bool species_likes_water(species_type species);
bool species_likes_lava(species_type species);
bool species_can_throw_large_rocks(species_type species);
size_type species_size(species_type species,
                       size_part_type psize = PSIZE_TORSO);

species_type random_draconian_player_species();
species_type get_species_by_abbrev(const char *abbrev);
const char *get_species_abbrev(species_type which_species);

// from player.cc
string species_name(species_type speci, bool genus = false, bool adj = false);
species_type str_to_species(const string &species);
string species_walking_verb(species_type sp);

monster_type player_species_to_mons_species(species_type species);
string species_prayer_action(species_type species);

int species_exp_modifier(species_type species);
int species_hp_modifier(species_type species);
int species_mp_modifier(species_type species);
#endif
