
#ifndef SPL_SELFENCH_H
#define SPL_SELFENCH_H

#include "spl-cast.h"
#include "transformation.h"

int allowed_deaths_door_hp();
spret_type cast_deaths_door(int pow, bool fail);
void remove_ice_armour();
spret_type ice_armour(int pow, bool fail);

int harvest_corpses(const actor &harvester,
                    bool dry_run = false, bool defy_god = false);
spret_type corpse_armour(int pow, bool fail);

spret_type deflection(int pow, bool fail);

spret_type cast_regen(int pow, bool fail = false);
spret_type cast_revivification(int pow, bool fail);

spret_type cast_swiftness(int power, bool fail = false);

int cast_selective_amnesia(const string &pre_msg = "");
spret_type cast_silence(int pow, bool fail = false);

spret_type cast_infusion(int pow, bool fail = false);
spret_type cast_song_of_slaying(int pow, bool fail = false);

spret_type cast_liquefaction(int pow, bool fail);
spret_type cast_shroud_of_golubria(int pow, bool fail);
spret_type cast_transform(int pow, transformation which_trans, bool fail);

spret_type cast_piercing_shot(int pow, bool fail);

#endif
