#pragma once

#include "spl-cast.h"

struct bolt;
class actor;
class monster;

#define VILE_CLUTCH_POWER_KEY "vile_clutch_power"
#define FASTROOT_POWER_KEY "fastroot_power"

int englaciate(coord_def where, int pow, actor *agent);
spret cast_englaciation(int pow, bool fail);
bool backlight_monster(monster* mons);

//returns true if it slowed the monster
bool do_slow_monster(monster& mon, const actor *agent, int dur = 0);
bool enfeeble_monster(monster &mon, int pow);
string mons_simulacrum_immune_reason(const monster *mons);
spret cast_simulacrum(coord_def target, int pow, bool fail);
spret cast_vile_clutch(int pow, bolt &beam, bool fail);
void grasp_with_roots(actor &caster, actor &target, int turns);

void fill_grasp_chain_targets(const bolt& beam, const monster& first, int num, vector<monster*>& targs);
