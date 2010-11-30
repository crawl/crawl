/*
 * File:    ghost.h
 * Summary: Player ghost and random Pandemonium demon handling.
 *
 * Created for Dungeon Crawl Reference by dshaligram on
 * Thu Mar 15 20:10:20 2007 UTC.
 */

#ifndef GHOST_H
#define GHOST_H

#include "enum.h"
#include "itemprop.h"
#include "mon-enum.h"
#include "mon_resist_def.h"

mon_attack_flavour ugly_thing_colour_to_flavour(uint8_t u_colour);
mon_resist_def ugly_thing_resists(bool very_ugly, mon_attack_flavour u_att_flav);

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
    void reset();
    void init_random_demon();
    void init_player_ghost();
    void init_ugly_thing(bool very_ugly, bool only_mutate = false,
                         uint8_t force_colour = BLACK);
    void init_dancing_weapon(const item_def& weapon, int power);
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
