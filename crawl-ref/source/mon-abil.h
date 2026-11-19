/**
 * @file
 * @brief Monster abilities.
**/

#pragma once

#include "beam-type.h"
#include "kill-method-type.h"
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

#define SEISMOSAURUS_EGG_RADIUS 4

// When (in elapsed time) a treasure goblin will next try to leave, and the
// time beyond which wounding it no longer puts that off.
#define TREASURE_GOBLIN_ESCAPE_KEY "treasure_goblin_escape"
#define TREASURE_GOBLIN_LIMIT_KEY "treasure_goblin_limit"
// Set once the player has been warned that it is about to try.
#define TREASURE_GOBLIN_WARNED_KEY "treasure_goblin_warned"
// Set while the goblin has been wounded since it last used its passage.
#define TREASURE_GOBLIN_STARTLED_KEY "treasure_goblin_startled"
// When its two tricks will next be ready.
#define TREASURE_GOBLIN_PASSAGE_KEY "treasure_goblin_passage"
#define TREASURE_GOBLIN_ROOTS_KEY "treasure_goblin_roots"

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

int slymdra_split(monster& slymdra, int count = -1, bool quiet = false);

void treasure_goblin_alarm(monster& goblin);
void treasure_goblin_wounded(monster& goblin, const actor* oppressor,
                             kill_method_type ktype, beam_type flavour);
bool treasure_goblin_can_open_portal(const monster& goblin);
bool treasure_goblin_wants_out(const monster& goblin);
bool treasure_goblin_about_to_leave(const monster& goblin);
void treasure_goblin_consider_escape(monster& goblin);
void treasure_goblin_escape(monster& goblin);
bool treasure_goblin_left_unwatched(monster& goblin);

bool thorn_hunter_range_check(monster& mon);
void thorn_hunter_raise_barrier(monster& mon, bool skip_proximity_check = false);
