#pragma once

#include "spl-cast.h"

spret cast_disjunction(int pow, bool fail);
void disjunction_spell();

spret cast_blink(bool allow_control = true, bool fail = false);
spret cast_controlled_blink(bool fail = false, bool safe = true);
void uncontrolled_blink(bool override_stasis = false);
spret semicontrolled_blink(int pow = 100, bool fail = false,
    bool safe_cancel = true,
    bool end_ctele = true);
spret controlled_blink(bool fail, bool safe_cancel = true, int range = -1);
spret frog_hop(bool fail);
spret crab_walk();
void wizard_blink();

void you_teleport();
void you_teleport_now(bool allow_control, bool wizard_tele = false, bool teleportitis = false,
                      string reason = "");
bool you_teleport_to(const coord_def where,
                     bool move_monsters = false);

spret cast_portal_projectile(int pow, bool fail);

struct bolt;
spret cast_apportation(int pow, bolt& beam, bool fail);
spret cast_golubrias_passage(const coord_def& where, bool fail);

spret cast_dispersal(int pow, bool fail);
spret cast_singularity(actor* agent, int pow, const coord_def& where, bool fail);

int gravitas_range(int pow, int strength = 1);
bool fatal_attraction(const coord_def& pos, const actor *agent, int pow);
void singularity_pull(const monster *singularity);
spret cast_gravitas(int pow, const coord_def& where, bool fail);

bool beckon(actor &beckoned, const bolt &path);