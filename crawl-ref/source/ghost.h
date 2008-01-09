#ifndef GHOST_H
#define GHOST_H

#include "externs.h"
#include "enum.h"
#include "itemprop.h"
#include "mon-util.h"

struct ghost_demon
{
public:
    std::string name;

    species_type species;
    job_type job;
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

extern std::vector<ghost_demon> ghosts;

#endif
