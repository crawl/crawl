/**
 * @file
 * @brief Monster abilities.
**/

#pragma once

#include "killer-type.h"
#include "mon-poly.h"

#define BORIS_ORB_KEY "boris orb key"

class actor;
class monster;
struct bolt;

bool mon_special_ability(monster* mons);

void draconian_change_colour(monster* drac);

void boris_covet_orb(monster* boris);

bool ugly_thing_mutate(monster& ugly, bool force = true);
bool slime_creature_polymorph(monster& slime, poly_power_type power = PPT_SAME);
void merge_ench_durations(monster& initial, monster& merge_to, bool usehd = false);

bool lost_soul_revive(monster& mons, killer_type killer);

void treant_release_fauna(monster& mons);
void check_grasping_roots(actor& act, bool quiet = false);

bool egg_is_incubating(const monster& egg);
