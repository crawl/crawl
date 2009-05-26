/*
 * File:    ghost.h
 * Summary: Player ghost and random Pandemonium demon handling.
 *
 * Created for Dungeon Crawl Reference by dshaligram on
 * Thu Mar 15 20:10:20 2007 UTC.
 *
 * Modified for Crawl Reference by $Author$ on $Date$
 */

#ifndef GHOST_H
#define GHOST_H

#include "externs.h"
#include "enum.h"
#include "itemprop.h"
#include "mon-util.h"

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
    mon_resist_def resists;

    bool spellcaster, cycle_colours;
    flight_type fly;

    monster_spells spells;

public:
    ghost_demon();
    void reset();
    void init_random_demon();
    void init_player_ghost();

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
};

bool debug_check_ghosts();

extern std::vector<ghost_demon> ghosts;

#endif
