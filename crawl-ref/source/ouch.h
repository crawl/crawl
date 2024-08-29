/**
 * @file
 * @brief Functions used when Bad Things happen to the player.
**/

#pragma once

#define DEATH_NAME_LENGTH 10

#include "beam.h"
#include "enum.h"
#include "kill-method-type.h"

/**
 * Key for <tt>you.props</tt> indicating that the player already received a
 * message about melting Ozocubu's Armour this turn. The value does not
 * matter, only the key's existence in the hash.
 */
#define MELT_ARMOUR_KEY "melt_armour"

void maybe_melt_player_enchantments(beam_type flavour, int damage);
int check_your_resists(int hurted, beam_type flavour, string source,
                       bolt *beam = 0, bool doEffects = true);

class actor;
int actor_to_death_source(const actor* agent);

string morgue_name(string char_name, time_t when_crawl_got_even);

int corrosion_chance(int sources);

int outgoing_harm_amount(int levels);
int incoming_harm_amount(int levels);

void reset_damage_counters();
void ouch(int dam, kill_method_type death_type, mid_t source = MID_NOBODY,
          const char *aux = nullptr, bool see_source = true,
          const char *death_source_name = nullptr);

void lose_level();
bool drain_player(int power = 25, bool announce_full = true,
                  bool ignore_protection = false, bool quiet = false);

void expose_player_to_element(beam_type flavour, int strength = 0,
                              bool slow_cold_blooded = true);

int timescale_damage(const actor *act, int damage);
void _maybe_blood_hastes_allies();
#if TAG_MAJOR_VERSION == 34
bool can_shave_damage();
int do_shave_damage(int dam);
#endif
