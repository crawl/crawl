
#pragma once

#include "spl-cast.h"
#include "transformation.h"

class actor;

spret cast_deaths_door(int pow, bool fail);
void remove_ice_armour();
spret ice_armour(int pow, bool fail);

void fiery_armour();

int harvest_corpses(const actor &harvester,
                    bool dry_run = false, bool defy_god = false);
spret corpse_armour(int pow, bool fail);

spret cast_revivification(int pow, bool fail);

spret cast_swiftness(int power, bool fail);

int cast_selective_amnesia(const string &pre_msg = "");

int silence_min_range(int pow);
int silence_max_range(int pow);
spret cast_silence(int pow, bool fail = false);

spret cast_wereblood(int pow, bool fail);

int liquefaction_max_range(int pow);
spret cast_liquefaction(int pow, bool fail);
spret cast_transform(int pow, transformation which_trans, bool fail);
spret cast_corpse_rot(int pow, bool fail);
