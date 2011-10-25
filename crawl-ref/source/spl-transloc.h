#ifndef SPL_TRANSLOC_H
#define SPL_TRANSLOC_H

#include "spl-cast.h"

spret_type cast_controlled_blink(int pow, bool fail);
int blink(int pow, bool high_level_controlled_blink, bool wizard_blink = false,
          std::string *pre_msg = NULL);
spret_type cast_blink(bool allow_partial_control, bool fail);
void random_blink(bool, bool override_abyss = false);

bool allow_control_teleport(bool quiet = false);
spret_type cast_teleport_self(bool fail);
void you_teleport();
void you_teleport_now(bool allow_control,
                      bool new_abyss_area = false,
                      bool wizard_tele = false);
bool you_teleport_to(const coord_def where,
                     bool move_monsters = false);

spret_type cast_portal_projectile(int pow, bool fail);

struct bolt;
spret_type cast_apportation(int pow, bolt& beam, bool fail);
int cast_semi_controlled_blink(int pow);
spret_type cast_golubrias_passage(const coord_def& where, bool fail);
bool can_cast_golubrias_passage();

#endif
