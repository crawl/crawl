/**
 * @file
 * @brief Functions used when Bad Things happen to the player.
**/

#pragma once

#define DEATH_NAME_LENGTH 10

#include "beam.h"
#include "enum.h"

/**
 * Key for <tt>you.props</tt> indicating that the player already received a
 * message about melting Ozocubu's Armour this turn. The value does not
 * matter, only the key's existence in the hash.
 */
#define MELT_ARMOUR_KEY "melt_armour"

// Keep in sync with names in hiscores.cc.
// Note that you can't ever remove entries from here -- not even when a major
// save tag is bumped, or listing scores will break. The order doesn't matter.
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
    KILLED_BY_WIZMODE,
    KILLED_BY_DRAINING,
    KILLED_BY_STARVATION,
    KILLED_BY_FREEZING,
    KILLED_BY_BURNING,
    KILLED_BY_WILD_MAGIC,
    KILLED_BY_XOM,
    KILLED_BY_ROTTING,
    KILLED_BY_TARGETING,
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
    KILLED_BY_BARBS,
    KILLED_BY_BEING_THROWN,
    KILLED_BY_COLLISION,
    NUM_KILLBY
};

void maybe_melt_player_enchantments(beam_type flavour, int damage);
int check_your_resists(int hurted, beam_type flavour, string source,
                       bolt *beam = 0, bool doEffects = true);

class actor;
int actor_to_death_source(const actor* agent);

string morgue_name(string char_name, time_t when_crawl_got_even);

void reset_damage_counters();
void ouch(int dam, kill_method_type death_type, mid_t source = MID_NOBODY,
          const char *aux = nullptr, bool see_source = true,
          const char *death_source_name = nullptr);

void lose_level();
bool drain_player(int power = 25, bool announce_full = true,
                  bool ignore_protection = false);

void expose_player_to_element(beam_type flavour, int strength = 0,
                              bool slow_cold_blooded = true);

int timescale_damage(const actor *act, int damage);
bool can_shave_damage();
int do_shave_damage(int dam);
