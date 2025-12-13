/**
 * @file
 * @brief Monster abilities.
**/

#pragma once

#include "killer-type.h"
#include "mon-poly.h"

#define BORIS_ORB_KEY "boris orb key"

#define NOBODY_MEMORIES_KEY "nobody_memories"
#define NOBODY_MAX_MEMORIES 3
#define NOBODY_RECOVERY_KEY "nobody_recovery"

#define TESSERACT_SPAWN_COUNTER_KEY "tesseract_spawn_count"
#define TESSERACT_SPAWN_TIMER_KEY "tesseract_spawn_timer"

#define SLYMDRA_FAKE_HEADS_KEY "slymdra_fake_heads"
#define SLYMDRA_SLIMES_EATEN_KEY "slymdra_slimes_eaten"

class actor;
class monster;
struct bolt;

bool mon_special_ability(monster* mons);

void draconian_change_colour(monster* drac);

void boris_covet_orb(monster* boris);

bool ugly_thing_mutate(monster& ugly, bool force = true);
bool slime_creature_polymorph(monster& slime, poly_power_type power = PPT_SAME);
void merge_ench_durations(monster& initial, monster& merge_to, bool usehd = false);
bool slymdra_polymorph(monster& slimedra, poly_power_type power = PPT_SAME);
void slymdra_scale_hp(monster& slymdra);

bool lost_soul_revive(monster& mons, killer_type killer);

void treant_release_fauna(monster& mons);
void check_grasping_roots(actor& act, bool quiet = false);

void seismosaurus_egg_hatch(monster* mons);
bool egg_is_incubating(const monster& egg);

void initialize_nobody_memories(monster& nobody);
bool pyrrhic_recollection(monster& nobody);

void solar_ember_blast();

void activate_tesseracts();
void tesseract_action(monster& mon);
