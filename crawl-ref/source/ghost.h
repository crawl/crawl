/**
 * @file
 * @brief Player ghost and random Pandemonium demon handling.
**/

#ifndef GHOST_H
#define GHOST_H

#include "enum.h"
#include "itemprop.h"
#include "mon-enum.h"
#include "mon_resist_def.h"

#ifdef USE_TILE
int tile_offset_for_labrat_colour(uint8_t l_colour);
#endif
std::string adjective_for_labrat_colour(uint8_t l_colour);
uint8_t colour_for_labrat_adjective(std::string adjective);

class ghost_demon
{
public:
    std::string name;

    species_type species;
    job_type job;
    god_type religion;
    skill_type best_skill;
    short best_skill_level;
    short xl;

    short max_hp, ev, ac, damage, speed;
    bool see_invis;
    brand_type brand;
    mon_attack_type att_type;
    mon_attack_flavour att_flav;
    mon_resist_def resists;

    bool spellcaster, cycle_colours;
    uint8_t colour;
    flight_type fly;

    monster_spells spells;

public:
    ghost_demon();
    bool has_spells() const;
    void reset();
    void init_random_demon();
    void init_player_ghost();
    void init_ugly_thing(bool very_ugly, bool only_mutate = false,
                         uint8_t force_colour = BLACK);
    void init_dancing_weapon(const item_def& weapon, int power);
    void init_labrat (uint8_t force_colour = BLACK);
    void ugly_thing_to_very_ugly_thing();

public:
    static std::vector<ghost_demon> find_ghosts();

private:
    static int n_extra_ghosts();
    static void find_extra_ghosts(std::vector<ghost_demon> &ghosts, int n);
    static void find_transiting_ghosts(std::vector<ghost_demon> &gs, int n);
    static void announce_ghost(const ghost_demon &g);

private:
    void add_spells();
    spell_type translate_spell(spell_type playerspell) const;
    void ugly_thing_add_resistance(bool very_ugly,
                                   mon_attack_flavour u_att_flav);
};

bool debug_check_ghosts();
int ghost_level_to_rank(const int xl);

extern std::vector<ghost_demon> ghosts;

#endif
