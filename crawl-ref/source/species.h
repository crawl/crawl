#ifndef SPECIES_H
#define SPECIES_H

enum genus_type
{
    GENPC_DRACONIAN,
    GENPC_ELVEN,
    GENPC_DWARVEN,
    GENPC_OGREISH,
    GENPC_NONE,
};

genus_type species_genus(species_type species);
int species_has_claws(species_type species, bool mut_level = false);
size_type species_size(species_type species,
                       size_part_type psize = PSIZE_TORSO);

// from newgame.cc
species_type get_species(const int index);
species_type random_draconian_player_species();
int ng_num_species();
species_type get_species_by_abbrev(const char *abbrev);
const char *get_species_abbrev(species_type which_species);
int get_species_index_by_abbrev(const char *abbrev);
int get_species_index_by_name(const char *name);

// from player.cc
std::string species_name(species_type speci, bool genus = false, bool adj = false);
species_type str_to_species(const std::string &species);

monster_type player_species_to_mons_species(species_type species);

// species_type bounds checking.
bool is_valid_species(species_type);

#endif
