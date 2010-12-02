#ifndef SPL_TRANSLOC_H
#define SPL_TRANSLOC_H

int blink(int pow, bool high_level_controlled_blink, bool wizard_blink = false,
          std::string *pre_msg = NULL);
void random_blink(bool, bool override_abyss = false);

bool allow_control_teleport(bool quiet = false);
void you_teleport();
void you_teleport_now(bool allow_control,
                      bool new_abyss_area = false,
                      bool wizard_tele = false);
bool you_teleport_to(const coord_def where,
                     bool move_monsters = false);

bool cast_portal_projectile(int pow);

struct bolt;
bool cast_apportation(int powc, bolt& beam);
int cast_semi_controlled_blink(int pow);
bool cast_golubrias_passage(const coord_def& where);
bool can_cast_golubrias_passage();

#endif
