#ifndef SPECIES_H
#define SPECIES_H

// from newgame.cc
species_type get_species(const int index);
species_type random_draconian_player_species();
int ng_num_species();
species_type get_species_by_abbrev(const char *abbrev);
const char *get_species_abbrev(species_type which_species);
int get_species_index_by_abbrev(const char *abbrev);
int get_species_index_by_name(const char *name);

// from player.cc
std::string species_name(species_type speci, int level, bool genus = false,
                         bool adj = false);
species_type str_to_species(const std::string &species);

#endif

