#pragma once

#include "spl-cast.h"

class actor;
class dist;

spret cast_disjunction(int pow, bool fail);
void disjunction_spell();

spret cast_blink(bool fail = false);
void uncontrolled_blink(bool override_stasis = false);
spret controlled_blink(bool safe_cancel = true, dist *target=nullptr);
void wizard_blink();

spret frog_hop(bool fail, dist *target=nullptr);
bool palentonga_charge_possible(bool quiet, bool ignore_safe_monsters);
spret palentonga_charge(bool fail, dist *target=nullptr);
int palentonga_charge_range();

void you_teleport();
void you_teleport_now(bool wizard_tele = false, bool teleportitis = false,
                      string reason = "");
bool you_teleport_to(const coord_def where,
                     bool move_monsters = false);

spret cast_portal_projectile(int pow, bool fail);

spret cast_manifold_assault(int pow, bool fail, bool real = true);
string weapon_unprojectability_reason();

struct bolt;
spret cast_apportation(int pow, bolt& beam, bool fail);
spret cast_golubrias_passage(const coord_def& where, bool fail);

spret cast_dispersal(int pow, bool fail);

int gravitas_range(int pow);
bool fatal_attraction(const coord_def& pos, const actor *agent, int pow);
spret cast_gravitas(int pow, const coord_def& where, bool fail);

bool beckon(actor &beckoned, const bolt &path);
void attract_monsters();
spret word_of_chaos(int pow, bool fail);
spret blinkbolt(int power, bolt &beam, bool fail);
