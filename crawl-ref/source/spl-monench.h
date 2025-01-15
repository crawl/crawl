#pragma once

#include "constrict-type.h"
#include "random.h"
#include "spl-cast.h"

struct bolt;
class actor;
class monster;

#define VILE_CLUTCH_POWER_KEY "vile_clutch_power"
#define FASTROOT_POWER_KEY "fastroot_power"
#define RIMEBLIGHT_POWER_KEY "rimeblight_power"
#define RIMEBLIGHT_TICKS_KEY "rimeblight_ticks"
#define RIMEBLIGHT_DEATH_KEY "death_by_rimeblight"

int englaciate(coord_def where, int pow, actor *agent);
spret cast_englaciation(int pow, bool fail);
bool backlight_monster(monster* mons);

//returns true if it slowed the monster
bool do_slow_monster(monster& mon, const actor *agent, int dur = 0);
bool enfeeble_monster(monster &mon, int pow);
spret cast_vile_clutch(int pow, bolt &beam, bool fail);
bool start_ranged_constriction(actor& caster, actor& target, int duration,
                               constrict_type type);

dice_def rimeblight_dot_damage(int pow, bool random = true);
string describe_rimeblight_damage(int pow, bool terse);
void do_rimeblight_explosion(coord_def pos, int power, int size);
bool maybe_spread_rimeblight(monster& victim, int power);
bool apply_rimeblight(monster& victim, int power, bool quiet = false);
void tick_rimeblight(monster& victim);

spret cast_sign_of_ruin(actor& caster, coord_def target, int duration, bool check_only = false);

spret cast_percussive_tempering(const actor& caster, monster& target, int power, bool fail);
bool is_valid_tempering_target(const monster& mon, const actor& caster);

void do_vexed_attack(actor& actor);
