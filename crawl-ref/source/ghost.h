/**
 * @file
 * @brief Player ghost and random Pandemonium demon handling.
**/

#pragma once

#include <vector>

#include "enchant-type.h"
#include "enum.h"
#include "god-type.h"
#include "item-prop-enum.h"
#include "job-type.h"
#include "mon-enum.h"
#include "mutant-beast.h"
#include "species-type.h"

using std::vector;

#define MIRRORED_GHOST_KEY "mirrored_ghost"

// Keys for mgen_data to create the right type of apostle
#define APOSTLE_TYPE_KEY "apostle_type"
#define APOSTLE_POWER_KEY "apostle_power"
#define APOSTLE_BAND_POWER_KEY "apostle_band_power"

enum apostle_type
{
    // Basic generation types
    APOSTLE_WARRIOR,
    APOSTLE_WIZARD,
    APOSTLE_PRIEST,

    // Sub-types (not directly chosen for generation)

    // End
    NUM_APOSTLE_TYPES,
};

const string apostle_type_names[] =
{
    "warrior",
    "wizard",
    "priest",
};
COMPILE_CHECK(ARRAYSZ(apostle_type_names) == NUM_APOSTLE_TYPES);

int apostle_type_by_name(const string name);

class ghost_demon
{
public:
    string name;

    species_type species;
    job_type job;
    god_type religion;
    skill_type best_skill;
    short best_skill_level;
    short xl;

    short max_hp, ev, ac, damage, speed, move_energy;
    bool see_invis, flies;
    brand_type brand;
    attack_type att_type;
    attack_flavour att_flav;
    resists_t resists;
    enchant_type cloud_ring_ench;
    int umbra_rad;

    colour_t colour;

    monster_spells spells;

public:
    ghost_demon();
    bool has_spells() const;
    void reset();
    void barebones_init();
    void init_pandemonium_lord(bool friendly = false);
    void init_player_ghost();
    void init_ugly_thing(bool very_ugly, bool only_mutate = false,
                         colour_t force_colour = BLACK);
    void init_dancing_weapon(const item_def& weapon, int power);
    void init_spectral_weapon(const item_def& weapon);
    void init_orc_apostle(apostle_type type, int pow);

    void ugly_thing_to_very_ugly_thing();

    void init_inugami_from_player(int power);

    void init_platinum_paragon(int power);


public:
    static const vector<ghost_demon> find_ghosts(bool include_player=true);
    static bool ghost_eligible();


private:
    static void find_extra_ghosts(vector<ghost_demon> &ghosts);
    static void find_transiting_ghosts(vector<ghost_demon> &gs);
    static void announce_ghost(const ghost_demon &g);

private:
    void add_spells();
    spell_type translate_spell(spell_type playerspell) const;
    void ugly_thing_add_resistance(bool very_ugly,
                                   attack_flavour u_att_flav);
    void set_pan_lord_special_attack();
    void set_pan_lord_cloud_ring();
    void pick_apostle_spells(apostle_type type, int pow);
};

bool debug_check_ghosts(vector<ghost_demon> &ghosts);
bool debug_check_ghost(const ghost_demon &ghost);
int ghost_level_to_rank(const int xl);
int ghost_rank_to_level(const int rank);
