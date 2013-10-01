#ifndef SPECIES_H
#define SPECIES_H

enum genus_type
{
    GENPC_DRACONIAN,
    GENPC_ELVEN,
    GENPC_ORCISH,
    GENPC_NONE,
};

genus_type species_genus(species_type species);
int species_has_claws(species_type species, bool mut_level = false);
bool species_likes_water(species_type species);
bool species_likes_lava(species_type species);
bool species_can_throw_large_rocks(species_type species);
size_type species_size(species_type species,
                       size_part_type psize = PSIZE_TORSO);

// from newgame.cc
species_type get_species(const int index);
species_type random_draconian_player_species();
int ng_num_species();
species_type get_species_by_abbrev(const char *abbrev);
const char *get_species_abbrev(species_type which_species);

// from player.cc
string species_name(species_type speci, bool genus = false, bool adj = false);
species_type str_to_species(const string &species);

monster_type player_species_to_mons_species(species_type species);

// species_type bounds checking.
bool is_valid_species(species_type);
int species_exp_modifier(species_type species);
int species_hp_modifier(species_type species);
int species_mp_modifier(species_type species);
#endif
