#ifndef SPL_TRANSLOC_H
#define SPL_TRANSLOC_H

#include "spl-cast.h"

spret_type cast_disjunction(int pow, bool fail);
void disjunction();

spret_type cast_blink(bool allow_control = true, bool fail = false);
spret_type cast_controlled_blink(int pow = 100, bool fail = false,
                                 bool safe = true);
void uncontrolled_blink(bool override_stasis = false);
spret_type semicontrolled_blink(int pow = 100, bool fail = false,
                                bool safe_cancel = true,
                                bool end_ctele = true);
spret_type controlled_blink(bool fail, bool safe_cancel = true);
void wizard_blink();

bool allow_control_teleport(bool quiet = false);
spret_type cast_teleport_self(bool fail);
void you_teleport();
void you_teleport_now(bool allow_control,
                      bool wizard_tele = false,
                      int range = GDM);
bool you_teleport_to(const coord_def where,
                     bool move_monsters = false);

spret_type cast_portal_projectile(int pow, bool fail);

struct bolt;
spret_type cast_apportation(int pow, bolt& beam, bool fail);
spret_type cast_golubrias_passage(const coord_def& where, bool fail);

spret_type cast_dispersal(int pow, bool fail = false);

int singularity_range(int pow, int strength = 1);
spret_type cast_singularity(actor* agent, int pow, const coord_def& where,
                            bool fail);
void attract_actor(const actor* agent, actor* victim, const coord_def pos,
                   int pow, int strength);
void singularity_pull(const monster *singularity);
bool fatal_attraction(actor *victim, actor *agent, int pow);
#endif
