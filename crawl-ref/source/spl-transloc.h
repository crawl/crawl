#pragma once

#include "spl-cast.h"

class actor;
class dist;

spret cast_disjunction(int pow, bool fail);
void disjunction_spell();

spret cast_blink(int pow, bool fail = false);
void uncontrolled_blink(bool override_stasis = false);
spret controlled_blink(bool safe_cancel = true, dist *target = nullptr);
void wizard_blink();

int frog_hop_range();
spret frog_hop(bool fail, dist *target = nullptr);

bool electric_charge_possible(bool allow_safe_monsters);
spret electric_charge(int powc, bool fail);
string movement_impossible_reason();

void you_teleport();
void you_teleport_now(bool wizard_tele = false, bool teleportitis = false,
                      string reason = "");
bool you_teleport_to(const coord_def where,
                     bool move_monsters = false);
bool cell_vetoes_teleport(coord_def cell, bool check_monsters = true,
                          bool wizard_tele = false);

spret cast_portal_projectile(int pow, bool fail);

spret cast_manifold_assault(int pow, bool fail, bool real = true);
string weapon_unprojectability_reason();

struct bolt;
spret cast_apportation(int pow, bolt& beam, bool fail);
int golubria_fuzz_range();
bool golubria_valid_cell(coord_def p);
spret cast_golubrias_passage(int pow, const coord_def& where, bool fail);

spret cast_dispersal(int pow, bool fail);

int gravitas_range(int pow);
bool fatal_attraction(const coord_def& pos, const actor *agent, int pow);
spret cast_gravitas(int pow, const coord_def& where, bool fail);

bool beckon(actor &beckoned, const bolt &path);
void attract_monsters(int delay);
vector<monster *> find_chaos_targets(bool just_check = false);
spret word_of_chaos(int pow, bool fail);
spret blinkbolt(int power, bolt &beam, bool fail);
