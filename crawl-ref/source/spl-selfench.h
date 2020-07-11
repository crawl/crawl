
#pragma once

#include "spl-cast.h"
#include "transformation.h"

spret cast_deaths_door(int pow, bool fail);
void remove_ice_armour();
spret ice_armour(int pow, bool fail);

int harvest_corpses(const actor &harvester,
                    bool dry_run = false, bool defy_god = false);
spret corpse_armour(int pow, bool fail);

spret cast_revivification(int pow, bool fail);

spret cast_swiftness(int power, bool fail);

int cast_selective_amnesia(const string &pre_msg = "");
spret cast_silence(int pow, bool fail = false);

spret cast_wereblood(int pow, bool fail);

spret cast_liquefaction(int pow, bool fail);
spret cast_transform(int pow, transformation which_trans, bool fail);

spret cast_noxious_bog(int pow, bool fail);
void noxious_bog_cell(coord_def p);
