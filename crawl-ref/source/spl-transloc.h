#ifndef SPL_TRANSLOC_H
#define SPL_TRANSLOC_H

#include "spl-cast.h"

spret_type cast_controlled_blink(int pow, bool fail);
spret_type cast_disjunction(int pow, bool fail);
void disjunction();
int blink(int pow, bool high_level_controlled_blink, bool wizard_blink = false,
          const string &pre_msg = "", bool safely_cancellable = false);
spret_type cast_blink(bool allow_partial_control, bool fail);
void random_blink(bool, bool override_abyss = false, bool override_stasis = false);

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
spret_type cast_semi_controlled_blink(int pow, bool cheap_cancel,
                                      bool end_ctele,
                                      bool fail = false);
spret_type cast_golubrias_passage(const coord_def& where, bool fail);

spret_type cast_dispersal(int pow, bool fail = false);

#endif
