/*
 *  File:       ouch.h
 *  Summary:    Functions used when Bad Things happen to the player.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 */


#ifndef OUCH_H
#define OUCH_H


#define DEATH_NAME_LENGTH 10

#include "enum.h"

// Keep in sync with names in hiscores.cc.
enum kill_method_type
{
    KILLED_BY_MONSTER,                 // 0
    KILLED_BY_POISON,
    KILLED_BY_CLOUD,
    KILLED_BY_BEAM,                    // 3
    KILLED_BY_DEATHS_DOOR,  // should be deprecated, but you never know {dlb}
    KILLED_BY_LAVA,                    // 5
    KILLED_BY_WATER,
    KILLED_BY_STUPIDITY,
    KILLED_BY_WEAKNESS,
    KILLED_BY_CLUMSINESS,
    KILLED_BY_TRAP,                    // 10
    KILLED_BY_LEAVING,
    KILLED_BY_WINNING,
    KILLED_BY_QUITTING,
    KILLED_BY_DRAINING,
    KILLED_BY_STARVATION,              // 15
    KILLED_BY_FREEZING,
    KILLED_BY_BURNING,
    KILLED_BY_WILD_MAGIC,
    KILLED_BY_XOM,
    KILLED_BY_STATUE,                  // 20
    KILLED_BY_ROTTING,
    KILLED_BY_TARGETTING,
    KILLED_BY_SPORE,
    KILLED_BY_TSO_SMITING,
    KILLED_BY_PETRIFICATION,           // 25
    // 26
    KILLED_BY_SOMETHING = 27,
    KILLED_BY_FALLING_DOWN_STAIRS,
    KILLED_BY_ACID,
    KILLED_BY_CURARE,                  // 30
    KILLED_BY_MELTING,
    KILLED_BY_BLEEDING,
    KILLED_BY_BEOGH_SMITING,
    KILLED_BY_DIVINE_WRATH,
    KILLED_BY_BOUNCE,                  // 35
    KILLED_BY_REFLECTION,
    KILLED_BY_SELF_AIMED,

    NUM_KILLBY
};

int check_your_resists(int hurted, beam_type flavour);
void splash_with_acid(int acid_strength, bool corrode_items = true);
void weapon_acid(int acid_strength);

class actor;
int actor_to_death_source(const actor* agent);

void ouch(int dam, int death_source, kill_method_type death_type,
          const char *aux = NULL, bool see_source = true);

void lose_level(void);
bool drain_exp(bool announce_full = true);

bool expose_items_to_element(beam_type flavour, const coord_def& where,
                             int strength = 0);
bool expose_player_to_element(beam_type flavour, int strength = 0);

#endif
