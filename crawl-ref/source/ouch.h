/**
 * @file
 * @brief Functions used when Bad Things happen to the player.
**/


#ifndef OUCH_H
#define OUCH_H


#define DEATH_NAME_LENGTH 10

#include "enum.h"
#include "beam.h"

// Keep in sync with names in hiscores.cc.
// Note that you can't ever remove entries from here -- not even when a major
// save tag is bumped, or listing scores will break.  The order doesn't matter.
enum kill_method_type
{
    KILLED_BY_MONSTER,
    KILLED_BY_POISON,
    KILLED_BY_CLOUD,
    KILLED_BY_BEAM,
    KILLED_BY_LAVA,
    KILLED_BY_WATER,
    KILLED_BY_STUPIDITY,
    KILLED_BY_WEAKNESS,
    KILLED_BY_CLUMSINESS,
    KILLED_BY_TRAP,
    KILLED_BY_LEAVING,
    KILLED_BY_WINNING,
    KILLED_BY_QUITTING,
    KILLED_BY_DRAINING,
    KILLED_BY_STARVATION,
    KILLED_BY_FREEZING,
    KILLED_BY_BURNING,
    KILLED_BY_WILD_MAGIC,
    KILLED_BY_XOM,
    KILLED_BY_ROTTING,
    KILLED_BY_TARGETTING,
    KILLED_BY_SPORE,
    KILLED_BY_TSO_SMITING,
    KILLED_BY_PETRIFICATION,
    KILLED_BY_SOMETHING,
    KILLED_BY_FALLING_DOWN_STAIRS,
    KILLED_BY_ACID,
    KILLED_BY_CURARE,
    KILLED_BY_BEOGH_SMITING,
    KILLED_BY_DIVINE_WRATH,
    KILLED_BY_BOUNCE,
    KILLED_BY_REFLECTION,
    KILLED_BY_SELF_AIMED,
    KILLED_BY_FALLING_THROUGH_GATE,
    KILLED_BY_DISINT,
    KILLED_BY_HEADBUTT,
    KILLED_BY_ROLLING,
    KILLED_BY_MIRROR_DAMAGE,
    KILLED_BY_SPINES,
    KILLED_BY_FRAILTY,

    NUM_KILLBY
};

int check_your_resists(int hurted, beam_type flavour, string source,
                       bolt *beam = 0, bool doEffects = true);
void splash_with_acid(int acid_strength, int death_source,
                      bool corrode_items = true, const char* hurt_msg = nullptr);

class actor;
int actor_to_death_source(const actor* agent);

string morgue_name(string char_name, time_t when_crawl_got_even);

void reset_damage_counters();
void ouch(int dam, int death_source, kill_method_type death_type,
          const char *aux = NULL, bool see_source = true,
          const char *death_source_name = NULL, bool attacker_effects = true);

void lose_level(int death_source, const char* aux);
bool drain_exp(bool announce_full = true, int power = 25);

bool expose_player_to_element(beam_type flavour, int strength = 0,
                              bool damage_inventory = true,
                              bool slow_dracs = true);

void screen_end_game(string text);
int timescale_damage(const actor *act, int damage);
#endif
